# Apollo Twin Linux Support Project

## Overview

This project implements native Linux support for Universal Audio Apollo Twin Thunderbolt audio interfaces on Arch Linux. The solution provides functional audio I/O through a combination of kernel-space components, user-space daemons, and audio subsystem integration.

**Target Hardware:** Universal Audio Apollo Twin (Thunderbolt)
**Target OS:** Arch Linux (rolling) with Hyprland/Wayland
**Audio Stack:** PipeWire (with JACK compatibility)
**Scope:** Audio I/O only (ADC/DAC, mic pres, monitoring) - UAD DSP plugins out of scope

## Architecture Overview

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Apollo Twin   │────│  Thunderbolt     │────│   Linux Host    │
│   (PCIe Device) │    │  Controller      │    │                 │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                        │                       │
         │ PCIe Tunneling         │                       │
         ▼                        ▼                       ▼
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   DSP Core      │    │  Thunderbolt     │────│  Kernel Space   │
│   (UAD-2)       │────│  PCIe Bridge     │    │                 │
│   ADC/DAC       │    │                  │    │ • thunderbolt.ko│
└─────────────────┘    └──────────────────┘    │ • pci.ko        │
                                               │ • apollo.ko     │
                                               └─────────────────┘
                                                        │
┌───────────────────────────────────────────────────────┼───────────────────────┐
│                                                       │                       │
│ User Space                                            │                       │
│                                                       ▼                       ▼
│ ┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐            │
│ │  Apollo Daemon  │    │   ALSA PCM       │    │   PipeWire      │            │
│ │  (Control)      │────│   Interface      │────│   Nodes         │            │
│ │                 │    │                  │    │                 │            │
│ └─────────────────┘    └──────────────────┘    └─────────────────┘            │
│          │                                      │                           │
│          │                                      │                           │
│          ▼                                      ▼                           │
│ ┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐            │
│ │   CLI Tools     │    │   Control        │    │   JACK Apps     │            │
│ │   (apolloctl)   │    │   Protocol       │    │   (optional)    │            │
│ └─────────────────┘    │   Reverse-       │    └─────────────────┘            │
│                        │   Engineered     │                                   │
│                        └──────────────────┘                                   │
└───────────────────────────────────────────────────────────────────────────────┘
```

### Layer Explanations

1. **Hardware Layer**: Apollo Twin presents as PCIe device via Thunderbolt tunneling
2. **Kernel Layer**: Handles Thunderbolt enumeration, PCIe bridging, custom audio driver
3. **Audio Subsystem**: ALSA PCM interface for low-level audio I/O
4. **PipeWire Layer**: High-level audio routing, session management, client integration
5. **Control Layer**: Reverse-engineered control protocol for device configuration
6. **User Interface**: CLI tools for device management and monitoring

## Thunderbolt & Device Access

### Device Presentation

The Apollo Twin uses Thunderbolt's PCIe tunneling to present as a standard PCIe device:

- **PCIe Vendor ID**: 13f4 (SIS) or custom UA ID
- **Device ID**: Varies by model (Twin X: 0x0001, etc.)
- **Class**: Audio Controller (0x04) or Multimedia (0x04)
- **Thunderbolt Protocol**: PCIe tunneling with optional DisplayPort

### Linux Enumeration

Current Linux Thunderbolt stack (`thunderbolt.ko`):

1. **Device Discovery**: Thunderbolt controller scans bus for connected devices
2. **Authorization**: Devices require explicit authorization via `/sys/bus/thunderbolt/devices/*/authorized`
3. **PCIe Bridging**: `pcieport.ko` creates virtual PCIe bus
4. **Driver Binding**: Standard PCI probe/bind mechanism

### Device Access Methods

**Raw Access Points:**
- `/sys/bus/thunderbolt/devices/` - Device enumeration and authorization
- `/sys/bus/pci/devices/` - PCIe device attributes and resources
- `/dev/mem` or `/sys/devices/pci*/*/resource*` - Memory-mapped I/O
- VFIO/UIO for user-space driver access

**Security Model:**
- Thunderbolt requires explicit user authorization (no auto-enumeration)
- IOMMU required for VFIO passthrough
- DMA protection via IOMMU translation

**Required Kernel Config:**
```
CONFIG_THUNDERBOLT=y
CONFIG_PCI=y
CONFIG_VFIO=y
CONFIG_VFIO_PCI=y
CONFIG_IOMMU_SUPPORT=y
CONFIG_INTEL_IOMMU=y  # or AMD_IOMMU
```

## Driver Strategy Selection

### Evaluation Matrix

| Strategy | Pros | Cons | Feasibility |
|----------|------|------|-------------|
| A. Custom ALSA Kernel Driver | Best performance, native integration | Complex reverse engineering, kernel stability risks | High |
| B. User-space VFIO Driver | Easier development, crash isolation | Higher latency, complex DMA management | Medium-High |
| C. FireWire Legacy Layer | N/A | Apollo uses Thunderbolt, not FireWire | N/A |
| D. USB Audio Emulation | Simple if compatible | Apollo doesn't present USB audio class | Low |
| E. Wine/macOS Driver | Complete feature support | Complex virtualization, performance overhead | Medium |

### Selected Approach: Hybrid A+B (Kernel ALSA + User-space Control)

**Rationale:**
- Kernel ALSA driver for low-latency audio I/O (critical path)
- User-space daemon for device control/configuration (non-critical path)
- Tradeoff: Performance vs development complexity/maintainability

**Technical Justification:**
- Audio I/O requires kernel-level access for acceptable latency (<10ms)
- Control operations can tolerate higher latency (100-500ms)
- Separation allows incremental development and testing
- User-space control daemon can be restarted without affecting audio stream

## Audio Pipeline Integration

### ALSA Integration

**PCM Interface (`apollo-pcm.c`):**
```c
static struct snd_pcm_ops apollo_pcm_ops = {
    .open = apollo_pcm_open,
    .close = apollo_pcm_close,
    .ioctl = apollo_pcm_ioctl,
    .hw_params = apollo_pcm_hw_params,
    .hw_free = apollo_pcm_hw_free,
    .prepare = apollo_pcm_prepare,
    .trigger = apollo_pcm_trigger,
    .pointer = apollo_pcm_pointer,
    .copy_user = apollo_pcm_copy_user,
};
```

**Hardware Parameters:**
- Formats: S32_LE (primary), S24_3LE, S16_LE
- Rates: 44.1k, 48k, 88.2k, 96k, 176.4k, 192k
- Channels: 2-8 (depending on model)
- Buffer sizes: 64-4096 frames
- Periods: 2-32

### PipeWire Integration

**Node Registration:**
- Source nodes for ADC inputs (analog + digital)
- Sink nodes for DAC outputs (analog + digital)
- Device profile with channel mappings

**Clock Synchronization:**
- Word clock recovery from device
- PTP support if available
- Buffer alignment with PipeWire quantum

**Latency Expectations:**
- Round-trip: 2-5ms (kernel driver)
- Processing: 0.5-1ms (DSP bypass mode)
- Total system: 3-8ms (including PipeWire overhead)

### JACK Compatibility

Fallback support via PipeWire's JACK compatibility layer:
- `pipewire-jack` package
- Automatic client bridging
- No additional configuration required

## Control & Mixer Functionality

### Control Protocol Reverse Engineering

**Device Communication:**
- PCIe configuration space registers
- Memory-mapped I/O regions
- Interrupt-driven control messages

**Key Controls:**
- Analog gain (mic pres: 0-65dB, line: -10/+10dB)
- Input selection (mic/line/inst per channel)
- Monitor mix (main out, alt out, cue)
- Phantom power (+48V) per channel
- High-pass filter (75Hz/150Hz)
- Pad (-20dB) per channel

### Implementation Strategy

**Minimal CLI Tool (`apolloctl`):**
```bash
# Set input gain
apolloctl gain analog1 45.0

# Enable phantom power
apolloctl phantom analog1 on

# Switch monitor source
apolloctl monitor main

# Save/load settings
apolloctl save preset1
apolloctl load preset1
```

**DBus Integration (Future):**
- System bus service: `org.apollodriver.Control`
- Methods for all control operations
- Property change signals

## Implementation Plan

### Phase 1: Device Reconnaissance

**Tools:** `thunderboltctl`, `lspci`, `pcimem`, `dma-buf`
**Commands:**
```bash
# Authorize device
echo 1 > /sys/bus/thunderbolt/devices/0-1/authorized

# Enumerate PCI devices
lspci -nn | grep -i audio

# Dump PCIe config space
lspci -xxx -s <device>

# Memory map inspection
pcimem /dev/mem 0x<BAR_address> 256
```

**Expected Output:** Device IDs, BAR addresses, register maps
**Failure Modes:** Authorization timeout, IOMMU conflicts, hardware revision differences

### Phase 2: Kernel Driver Development

**Tools:** Kernel build system, `gcc`, `make`
**Commands:**
```bash
# Configure kernel
make menuconfig  # Enable VFIO, Thunderbolt

# Build and install
make -j$(nproc) && make modules_install

# Load modules
modprobe thunderbolt
modprobe vfio-pci
```

**Expected Output:** Loadable kernel modules, ALSA device registration
**Failure Modes:** Kernel oops, resource conflicts, interrupt storms

### Phase 3: Audio Passthrough Testing

**Tools:** `aplay`, `arecord`, `jackd`, `pipewire`
**Commands:**
```bash
# Test ALSA playback
aplay -D hw:Apollo,0 /dev/urandom

# PipeWire enumeration
pactl list sources
pactl list sinks
```

**Expected Output:** Audible test tones, clean audio I/O
**Failure Modes:** Clock sync issues, buffer underruns, format incompatibilities

### Phase 4: Control Protocol Implementation

**Tools:** `wireshark` (USB sniffing), `gdb`, custom debug tools
**Commands:**
```bash
# Monitor control traffic
strace -f apolloctl gain analog1 50

# Debug register writes
gdb apolloctl -ex "break control_write" -ex "run"
```

**Expected Output:** Working gain control, persistent settings
**Failure Modes:** Protocol changes, register mapping errors

### Phase 5: Stability Hardening

**Tools:** `stress-ng`, `rt-tests`, `perf`
**Commands:**
```bash
# Latency testing
cyclictest -t1 -p 80 -n -i 10000

# Audio stress test
speaker-test -D hw:Apollo -c 2 -t sine -f 1000 -l 100000
```

**Expected Output:** <10ms latency stability, no xruns under load
**Failure Modes:** Thermal throttling, PCIe link issues

## Risks & Reality Check

### Realistically Achievable
- Basic audio I/O (ADC/DAC) with acceptable latency
- Input/output routing and monitoring
- Gain control for analog stages
- Device enumeration and authorization

### Borderline Achievable
- Full mixer functionality (requires extensive protocol RE)
- DSP monitoring modes (firmware-dependent)
- Multi-device synchronization
- Hotplug support

### Impossible Without UA Cooperation
- UAD DSP plugin support (proprietary algorithms)
- Official firmware updates
- Hardware calibration data access
- Undocumented DSP modes

**Key Risk:** Device firmware updates may break compatibility. Solution requires firmware pinning or update detection.

## Deliverables

### Core Components
- `apollo.ko` - Kernel ALSA driver module
- `apollod` - User-space control daemon
- `apolloctl` - Command-line control utility
- `apollo.conf` - Device configuration file

### Build System
- `Makefile` with kernel module targets
- `PKGBUILD` for Arch Linux package
- Docker container for cross-compilation

### Documentation
- `INSTALL.md` - Build and installation instructions
- `USAGE.md` - Operation and troubleshooting guide
- `HACKING.md` - Development and debugging guide

### Build Instructions

**Prerequisites:**
```bash
# Install build dependencies
pacman -S linux-headers dkms git make gcc
```

**Build Process:**
```bash
# Clone repository
git clone https://github.com/yourorg/apollodriver
cd apollodriver

# Build kernel module
make -C /usr/src/linux-headers-$(uname -r) M=$(pwd)/kernel modules

# Install
sudo make install
sudo modprobe apollo
```

**Verification:**
```bash
# Check module loading
lsmod | grep apollo

# Verify ALSA device
aplay -l | grep Apollo

# Test PipeWire integration
pactl list cards
```

## Fallback Strategies

### Virtualized macOS (Borderline)
- Thunderbolt passthrough to macOS VM (QEMU/KVM)
- Access via network audio (RTP/RAOP)
- Performance penalty: 10-20ms additional latency

### Dual-Boot Hybrid
- Boot into macOS for recording/mixing
- Export stems to Linux for additional processing
- Workflow disruption but guaranteed compatibility

### Network Bridge
- Run macOS in VM or separate machine
- Bridge audio via LAN (JACK netjack or PipeWire network)
- Latency: 5-15ms depending on network quality

---

**Status:** Design Phase - Implementation requires extensive reverse engineering of Apollo Twin hardware protocols.

