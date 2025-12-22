# Apollo Twin Linux Driver - Complete Design Specification

## 1. ARCHITECTURE OVERVIEW

### System Diagram
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
│ │  Apollo Daemon  │    │   ALSA PCM       │────│   PipeWire      │            │
│ │  (Control)      │────│   Interface      │    │   Nodes         │            │
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

### Layer Functions
1. **Hardware**: Apollo Twin presents as PCIe device via Thunderbolt
2. **Kernel**: Handles Thunderbolt enumeration, PCIe bridging, custom audio driver
3. **Audio Subsystem**: ALSA PCM interface for low-level audio I/O
4. **PipeWire**: High-level audio routing, session management, client integration
5. **Control**: Reverse-engineered protocol for device configuration
6. **User Interface**: CLI tools for device management and monitoring

## 2. THUNDERBOLT & DEVICE ACCESS

### Device Presentation
- **Protocol**: PCIe tunneling over Thunderbolt
- **PCI IDs**: Vendor 0x13f4 (SIS), Device varies by model
- **Class**: Audio Controller (0x04)
- **Access**: Memory-mapped I/O, DMA, interrupts

### Linux Enumeration
- `thunderbolt.ko` handles bus scanning and authorization
- `pcieport.ko` creates virtual PCIe bus
- Device requires explicit user authorization via `/sys/bus/thunderbolt/`
- Standard PCI probe/bind mechanism

### Access Methods
- **Sysfs**: `/sys/bus/thunderbolt/devices/*/authorized`
- **PCI**: `/sys/bus/pci/devices/*/resource*`
- **Raw Access**: `/dev/mem`, `pcimem`, DMA buffers
- **Security**: Thunderbolt authorization, IOMMU protection

### Required Kernel Config
```
CONFIG_THUNDERBOLT=y
CONFIG_PCI=y
CONFIG_VFIO=y
CONFIG_VFIO_PCI=y
CONFIG_IOMMU_SUPPORT=y
CONFIG_INTEL_IOMMU=y  # or AMD_IOMMU
```

## 3. DRIVER STRATEGY

### Selected Approach: Hybrid ALSA Kernel Driver + User-space Control

**Technical Justification:**
- Audio I/O requires kernel-level access for <10ms latency
- Control operations tolerate 100-500ms latency
- Separation enables incremental development and crash isolation
- User-space daemon restartable without affecting audio stream

**Tradeoffs:**
- **Pros**: Best performance, native integration, crash isolation
- **Cons**: Complex development, kernel stability requirements
- **Risk**: Kernel code must be robust to avoid system instability

### Alternatives Considered
- **VFIO User-space Driver**: Easier development but higher latency
- **USB Audio Emulation**: Not applicable (device doesn't present USB audio)
- **FireWire Compatibility**: N/A (Thunderbolt, not FireWire)

## 4. AUDIO PIPELINE INTEGRATION

### ALSA Implementation
**PCM Operations:**
- `apollo_pcm_open/close`: Stream management
- `apollo_pcm_hw_params`: Format/rate/channel configuration
- `apollo_pcm_trigger`: Start/stop audio streaming
- `apollo_pcm_pointer`: Position reporting
- `apollo_pcm_copy_user`: Data transfer

**Hardware Parameters:**
- **Formats**: S16_LE, S24_3LE, S32_LE
- **Rates**: 44.1k, 48k, 88.2k, 96k, 176.4k, 192k
- **Channels**: 2-8 (model dependent)
- **Buffers**: 64-4096 frames, 2-32 periods

### PipeWire Integration
**Node Registration:**
- Source nodes for ADC inputs (analog + digital)
- Sink nodes for DAC outputs (analog + digital)
- Device profile with proper channel mappings

**Clock Synchronization:**
- Word clock recovery from device
- PTP support if available
- Buffer alignment with PipeWire quantum

**Latency Expectations:**
- **Kernel Driver**: 2-5ms round-trip
- **DSP Bypass**: 0.5-1ms processing
- **Total System**: 3-8ms with PipeWire overhead

### JACK Compatibility
- Automatic bridging via `pipewire-jack`
- No additional configuration required
- Full JACK client compatibility

## 5. CONTROL & MIXER FUNCTIONALITY

### Reverse Engineering Scope
**Identified Controls:**
- Analog gain: 0-65dB per input
- Phantom power: +48V per analog input
- Input routing: Analog 1-4, Digital 1-2
- Monitor selection: Main, Alt, Cue outputs
- HPF: 75Hz/150Hz per channel
- Pad: -20dB per analog input

### Implementation Strategy
**CLI Tool (`apolloctl`):**
```bash
apolloctl gain 1 45.0          # Set input 1 gain to 45dB
apolloctl phantom 1 on         # Enable phantom power
apolloctl input 1 analog2      # Route input 1 to analog 2
apolloctl monitor main         # Set monitor to main output
apolloctl save preset1         # Save current settings
apolloctl load preset1         # Load settings
```

**Configuration Persistence:**
- `/etc/apollo/apollo.conf` for system-wide defaults
- User presets for session management
- Automatic saving on parameter changes

**Future DBus Integration:**
- System bus service: `org.apollodriver.Control`
- Property change signals for UI integration
- Method calls for programmatic control

## 6. IMPLEMENTATION PLAN

### Phase 1: Device Reconnaissance (Week 1-2)
**Tools:** `thunderboltctl`, `lspci`, `pcimem`, `dmesg`
**Commands:**
```bash
thunderboltctl list
echo 1 > /sys/bus/thunderbolt/devices/0-1/authorized
lspci -nn | grep -i audio
pcimem /dev/mem 0x<BAR_address> 256
```
**Deliverables:** Device IDs, register maps, memory layouts

### Phase 2: Kernel Driver Development (Week 3-6)
**Tools:** Kernel build system, `gdb`, `perf`
**Implementation:**
- PCI driver registration and probe
- ALSA PCM interface skeleton
- DMA buffer allocation and mapping
- Interrupt handling framework
- Basic hardware initialization

**Testing:**
```bash
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
sudo insmod apollo.ko
lsmod | grep apollo
aplay -l | grep Apollo
```

### Phase 3: Audio Passthrough (Week 7-8)
**Tools:** `aplay`, `arecord`, `jackd`
**Implementation:**
- Complete PCM operations
- DMA transfer implementation
- Clock synchronization
- Error handling and recovery

**Testing:**
```bash
speaker-test -D hw:Apollo -c 2 -t sine -f 1000
jackd -d alsa -d hw:Apollo -r 48000 -p 128
```

### Phase 4: Control Protocol (Week 9-12)
**Tools:** `strace`, `wireshark`, custom debug tools
**Implementation:**
- Protocol message format identification
- Control command implementation
- CLI tool completion
- Configuration management

**Testing:**
```bash
./apolloctl gain 1 30.0
./apolloctl status
```

### Phase 5: Integration & Optimization (Week 13-16)
**Tools:** `cyclictest`, `stress-ng`, `perf`
**Implementation:**
- PipeWire node registration
- Latency optimization
- Stability hardening
- Comprehensive testing

**Performance Targets:**
- <10ms round-trip latency
- No xruns under normal load
- Stable 24/7 operation

## 7. RISKS & REALITY CHECK

### Realistic Achievements (High Confidence)
- ✅ Basic ADC/DAC functionality with acceptable latency
- ✅ Device enumeration and authorization
- ✅ ALSA/PipeWire/JACK integration
- ✅ Fundamental gain and routing controls
- ✅ Stable long-term operation

### Borderline Achievements (Medium Confidence)
- ⚠️ Complete mixer functionality (extensive reverse engineering required)
- ⚠️ DSP monitoring modes (firmware-dependent)
- ⚠️ Multi-device synchronization
- ⚠️ Hot-plug robustness

### Impossible Without UA Cooperation
- ❌ UAD DSP plugin support (proprietary algorithms)
- ❌ Official firmware updates
- ❌ Hardware calibration data access
- ❌ Undocumented DSP modes

### Critical Risk: Firmware Compatibility
**Issue:** Device firmware updates may break reverse-engineered protocols
**Mitigation:**
- Firmware version detection
- Protocol versioning
- Community update mechanism
- Fallback to known working versions

## 8. DELIVERABLES

### Core Components
- **Kernel Module** (`apollo.ko`): ALSA PCM driver
- **Control Daemon** (`apollod`): Device management service
- **CLI Tool** (`apolloctl`): User interface
- **Configuration**: `/etc/apollo/apollo.conf`

### Build System
- **Makefiles**: Kernel and user-space builds
- **Arch PKGBUILD**: AUR package distribution
- **DKMS Integration**: Automatic kernel module rebuilding

### Documentation
- **INSTALL.md**: Setup and installation guide
- **USAGE.md**: Operation and configuration
- **HACKING.md**: Development and debugging guide

### Installation Process
```bash
# Clone repository
git clone https://github.com/yourorg/apollodriver

# Build kernel module
cd kernel && make && sudo make install

# Build user-space tools
cd ../userspace && make && sudo make install

# Install configuration
sudo cp ../config/apollo.conf /etc/apollo/

# Load module and start daemon
sudo modprobe apollo
sudo systemctl enable apollo --now
```

## 9. FALLBACK STRATEGIES

### Virtualized macOS (Borderline Viable)
**Approach:** Thunderbolt passthrough to macOS VM
**Tools:** QEMU/KVM with VFIO Thunderbolt passthrough
**Pros:** Complete feature compatibility
**Cons:** 10-20ms additional latency, complex setup
**Feasibility:** Medium (Thunderbolt passthrough challenging)

### Dual-Boot Workflow (Reliable but Disruptive)
**Approach:** macOS for recording/mixing, Linux for additional processing
**Pros:** Guaranteed compatibility, no performance penalty
**Cons:** Workflow disruption, data transfer overhead
**Feasibility:** High (but poor UX)

### Network Audio Bridge (Limited but Functional)
**Approach:** macOS host streams audio to Linux over network
**Tools:** JACK network components, RTP streaming
**Pros:** Single machine operation
**Cons:** 5-15ms latency depending on network
**Feasibility:** Medium-High (for basic I/O only)

---

**Conclusion:** This design provides a technically sound foundation for Apollo Twin Linux support. The hybrid architecture balances performance requirements with development practicality. Core audio I/O functionality is realistically achievable, though advanced features depend on successful reverse engineering of proprietary protocols.

