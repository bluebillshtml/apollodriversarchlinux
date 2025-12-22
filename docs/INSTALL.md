# Apollo Twin Linux Driver Installation Guide

## Prerequisites

### System Requirements
- **Arch Linux** (rolling release)
- **Kernel**: 5.15+ with Thunderbolt support
- **Hardware**: Intel or AMD system with Thunderbolt controller
- **Permissions**: Root access for installation

### Dependencies
```bash
# Install build dependencies
pacman -S base-devel linux-headers dkms git make gcc alsa-utils pipewire pipewire-alsa

# Optional: For development and debugging
pacman -S wireshark-cli gdb strace perf thunderbolt-tools
```

### Kernel Configuration
Ensure the following kernel options are enabled:
```
CONFIG_THUNDERBOLT=y
CONFIG_PCI=y
CONFIG_VFIO=y
CONFIG_VFIO_PCI=y
CONFIG_IOMMU_SUPPORT=y
CONFIG_INTEL_IOMMU=y  # or AMD_IOMMU
CONFIG_SND_ALSA=y
CONFIG_SND_PCM=y
```

## Installation

### 1. Clone Repository
```bash
git clone https://github.com/yourorg/apollodriver.git
cd apollodriver
```

### 2. Build Kernel Module
```bash
cd kernel
make
sudo make install
```

### 3. Build User-space Components
```bash
cd ../userspace
make
sudo make install
```

### 4. Install Configuration
```bash
sudo mkdir -p /etc/apollo
sudo cp ../config/apollo.conf /etc/apollo/
```

### 5. Load Kernel Module
```bash
sudo modprobe apollo
```

### 6. Start Control Daemon
```bash
sudo systemctl enable apollo
sudo systemctl start apollo
```

## Device Setup

### Thunderbolt Authorization
1. Connect Apollo Twin via Thunderbolt cable
2. Authorize the device:
```bash
# List Thunderbolt devices
thunderboltctl list

# Authorize the Apollo device (replace DOMAIN:PORT with actual values)
sudo thunderboltctl authorize DOMAIN:PORT
```

### Verification
```bash
# Check kernel module
lsmod | grep apollo

# Check ALSA device
aplay -l | grep Apollo

# Check PipeWire devices
pactl list cards

# Test audio
speaker-test -D hw:Apollo -c 2
```

## Configuration

### Device Settings
Edit `/etc/apollo/apollo.conf` to customize default settings.

### PipeWire Integration
The device should automatically appear in PipeWire. If not:
```bash
# Reload PipeWire configuration
systemctl --user restart pipewire
```

### JACK Support
For JACK applications, ensure PipeWire JACK compatibility is installed:
```bash
pacman -S pipewire-jack
```

## Troubleshooting

### Device Not Recognized
```bash
# Check Thunderbolt status
thunderboltctl list

# Check PCI devices
lspci | grep -i audio

# Check kernel logs
dmesg | grep -i apollo
dmesg | grep -i thunderbolt
```

### Audio Issues
```bash
# Test ALSA directly
aplay -D hw:Apollo,0 /usr/share/sounds/alsa/Front_Center.wav

# Check sample rates and formats
aplay -D hw:Apollo,0 --dump-hw-params /dev/null

# Monitor for xruns
jackd -d alsa -d hw:Apollo -r 48000 -p 256
```

### Control Issues
```bash
# Check daemon status
systemctl status apollo

# Check daemon logs
journalctl -u apollo

# Test CLI tool
apolloctl status
```

## Uninstallation

```bash
# Stop and disable service
sudo systemctl stop apollo
sudo systemctl disable apollo

# Remove kernel module
sudo rmmod apollo

# Remove files
sudo rm -rf /usr/lib/modules/*/extra/apollo.ko
sudo rm -f /usr/bin/apollod /usr/bin/apolloctl
sudo rm -rf /etc/apollo
sudo rm -f /usr/lib/systemd/system/apollo.service
```

## Advanced Configuration

### Custom Kernel Build
If building a custom kernel:
```bash
# In kernel config
CONFIG_EXPERIMENTAL=y
CONFIG_THUNDERBOLT_DEBUG=y
CONFIG_VFIO_DEBUG=y
```

### VFIO Passthrough (Advanced)
For virtual machine passthrough:
```bash
# Bind device to VFIO
echo "13f4 0001" > /sys/bus/pci/drivers/vfio-pci/new_id

# Use with QEMU
qemu-system-x86_64 -device vfio-pci,host=XX:XX.X
```

### Development Mode
For development and debugging:
```bash
# Build with debug symbols
make CFLAGS="-g -DDEBUG"

# Load with debugging
sudo modprobe apollo dyndbg=+p

# Monitor debug output
dmesg -w | grep apollo
```

