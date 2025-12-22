# Apollo Twin Driver Development Guide

## Overview

This guide covers the development process for the Apollo Twin Linux driver, including reverse engineering techniques, debugging strategies, and contribution guidelines.

## Development Environment

### Prerequisites
```bash
# Core development tools
pacman -S base-devel git gdb strace perf valgrind

# Kernel development
pacman -S linux-headers dkms cscope

# Audio debugging
pacman -S alsa-utils pipewire jack2 aplay

# Reverse engineering
pacman -S wireshark-cli radare2 binutils

# Documentation
pacman -S doxygen graphviz
```

### Project Structure
```
apollo/
├── kernel/           # Kernel-space driver
│   ├── apollo_main.c # Main driver entry
│   ├── apollo_pcm.c  # ALSA PCM interface
│   ├── apollo_hw.c   # Hardware abstraction
│   ├── apollo_control.c # Control interface
│   └── apollo.h      # Shared headers
├── userspace/        # User-space components
│   ├── apollod.c     # Control daemon
│   ├── apolloctl.c   # CLI tool
│   ├── apollo_control.c # Control library
│   └── apollo_control.h
├── config/           # Configuration files
├── docs/            # Documentation
└── tools/           # Development utilities
```

## Reverse Engineering Process

### Device Analysis

#### Thunderbolt Device Discovery
```bash
# List Thunderbolt devices
boltctl list

# Get detailed device information
boltctl info <device_id>

# Monitor device connection
udevadm monitor --subsystem=thunderbolt

# Check PCI configuration
lspci -nn -s <device_address>
lspci -xxx -s <device_address>
```

#### PCIe Register Analysis
```bash
# Dump PCI configuration space
pcimem /sys/bus/pci/devices/<device>/config 256

# Map device memory
pcimem /sys/bus/pci/devices/<device>/resource0 1024

# Monitor register access
strace -e ioctl apollod 2>&1 | grep ioctl
```

#### USB Traffic Capture (macOS/Windows)
```bash
# On macOS/Windows host with Wireshark
# Capture USB traffic during device operation
# Look for control transfers to Apollo device

# Common USB request types:
# - SET_CONFIGURATION
# - GET_DESCRIPTOR
# - VENDOR_DEVICE (0x40)
```

### Protocol Analysis

#### Control Message Format
Based on reverse engineering of UA Console (macOS):

```
Control Message Structure:
- Header: 8 bytes (magic + length + command)
- Payload: Variable length
- Checksum: 4 bytes (CRC32)

Commands identified:
- 0x01: Set gain
- 0x02: Get gain
- 0x03: Set phantom power
- 0x04: Set input routing
- 0x05: Set monitor mode
```

#### Firmware Analysis
```bash
# Extract firmware (if available)
dd if=/sys/bus/pci/devices/<device>/resource0 of=firmware.bin bs=1M count=1

# Disassemble with radare2
r2 firmware.bin
> aaa  # Analyze all
> afl  # List functions
> pdf @ main  # Disassemble main function
```

### Driver Development

#### Kernel Module Development
```bash
# Build with debug symbols
make CFLAGS="-g -DDEBUG" -C /usr/src/linux-headers-$(uname -r) M=$(pwd) modules

# Load with debug logging
sudo modprobe apollo dyndbg="+p"

# Monitor debug output
dmesg -w | grep apollo

# Unload module
sudo rmmod apollo
```

#### Debugging Techniques
```bash
# Use kernel debugger
gdb vmlinux /proc/kcore
(gdb) add-symbol-file apollo.ko <load_address>

# Trace function calls
perf probe --add 'apollo_pcm_open'
perf record -e probe:apollo_* -aR sleep 10
perf script

# Memory debugging
kmemleak
cat /sys/kernel/debug/kmemleak
```

#### ALSA Integration Testing
```bash
# Test PCM interface
alsa-utils/test/pcm.c -D hw:Apollo

# Check hardware constraints
aplay -D hw:Apollo --dump-hw-params /dev/null

# Monitor ALSA events
cat /proc/asound/card0/pcm0p/sub0/status
```

### User-space Development

#### Control Protocol Implementation
```c
// Example control message construction
struct apollo_control_msg {
    uint32_t magic;    // 0xdeadbeef
    uint16_t length;
    uint16_t command;
    uint8_t data[];
    uint32_t checksum;
};

int send_control_message(int fd, uint16_t cmd, const void *data, size_t len) {
    struct apollo_control_msg *msg;
    size_t total_len = sizeof(*msg) + len;

    msg = malloc(total_len);
    msg->magic = 0xdeadbeef;
    msg->length = total_len;
    msg->command = cmd;
    memcpy(msg->data, data, len);
    msg->checksum = crc32(msg, total_len - 4);

    return write(fd, msg, total_len);
}
```

#### Testing Control Interface
```bash
# Test individual controls
./apolloctl gain 1 30.0
./apolloctl phantom 1 on

# Monitor control traffic
strace -f ./apolloctl gain 1 30.0

# Debug daemon
gdb ./apollod
(gdb) break control_write
(gdb) run
```

## Testing Strategy

### Unit Testing
```bash
# Kernel module testing
sudo insmod apollo.ko test_mode=1

# User-space testing
make test
./test_control
```

### Integration Testing
```bash
# Full audio loopback test
jackd -d alsa -d hw:Apollo -r 48000 -p 128 &
jack_connect system:capture_1 system:playback_1
jack_connect system:capture_2 system:playback_2

# Latency measurement
jack_delay -I hw:Apollo -O hw:Apollo
```

### Regression Testing
```bash
# Automated test suite
make check

# Benchmarking
perf stat -e cycles,instructions,cache-misses ./benchmark_audio
```

## Performance Optimization

### Latency Reduction
```bash
# Profile interrupt handling
perf record -e irq:irq_handler_entry -a sleep 10
perf report

# Optimize DMA transfers
# Use coherent DMA mappings
# Minimize interrupt frequency
```

### CPU Usage Optimization
```bash
# Profile hot paths
perf record -F 1000 -a -- sleep 10
perf report

# Optimize data copying
# Use DMA where possible
# Minimize system calls
```

## Contribution Guidelines

### Code Style
```bash
# Check code style
./scripts/checkpatch.pl kernel/*.c

# Format code
clang-format -i kernel/*.c userspace/*.c
```

### Documentation
```bash
# Generate API documentation
doxygen Doxyfile

# Build manual pages
make docs
```

### Version Control
```bash
# Branch naming
git checkout -b feature/reverse-engineer-gain-control

# Commit messages
git commit -m "control: Add gain control protocol reverse engineering

- Identified gain control message format
- Implemented gain setting/getting functions
- Added validation and error handling"
```

### Code Review Process
1. Create feature branch
2. Implement changes with tests
3. Run full test suite
4. Submit pull request
5. Address review comments
6. Merge after approval

## Known Issues and TODOs

### High Priority
- Complete control protocol reverse engineering
- Implement proper error recovery
- Add comprehensive test suite

### Medium Priority
- VFIO passthrough support
- Network audio bridge
- GUI configuration tool

### Low Priority
- DSP monitoring support
- Multi-device synchronization
- Advanced routing features

## Resources

### Documentation
- [Linux Kernel ALSA Documentation](https://www.kernel.org/doc/html/latest/sound/index.html)
- [Thunderbolt Documentation](https://docs.kernel.org/admin-guide/thunderbolt.html)
- [PipeWire Documentation](https://docs.pipewire.org/)

### Tools
- [ALSATools](https://github.com/alsa-project/alsa-tools)
- [Wireshark](https://www.wireshark.org/)
- [Radare2](https://rada.re/n/)

### Community
- [Linux Audio Users](https://linuxaudio.org/)
- [ALSA Mailing List](mailto:alsa-user@alsa-project.org)
- [PipeWire GitLab](https://gitlab.freedesktop.org/pipewire/pipewire)

