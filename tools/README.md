# Apollo Driver Development Tools

This directory contains development and debugging tools for the Apollo Twin Linux driver.

## Tools Overview

### apollo_detect
Device detection and status reporting tool.

**Usage:**
```bash
# Basic device scan
./apollo_detect

# Verbose output
./apollo_detect -v

# Thunderbolt devices only
./apollo_detect -t

# PCI devices only
./apollo_detect -p
```

**Features:**
- Scans Thunderbolt and PCI buses for Apollo devices
- Reports device authorization status
- Checks kernel module and daemon status
- Provides troubleshooting guidance

### apollo_dump
Hardware register dumping tool for reverse engineering.

**Usage:**
```bash
# Dump PCI resource registers
./apollo_dump /sys/bus/pci/devices/0000:01:00.0/resource0 0x00 256

# Dump from physical memory (requires root)
./apollo_dump -m 0xfebf1000 0x00 1024

# 32-bit word format
./apollo_dump -w /dev/apollo 0x10

# Binary output for analysis
./apollo_dump -b /sys/bus/pci/devices/0000:01:00.0/resource0 0x00 1024 > dump.bin
```

**Features:**
- Safe PCI resource file access
- Direct physical memory access (with root)
- Multiple output formats (hex, binary, word, double-word)
- Bounds checking and error handling

### apollo_test
Automated test suite for driver validation.

**Usage:**
```bash
# Run build and configuration tests
./apollo_test

# Run all tests including hardware-dependent ones
./apollo_test --device
```

**Features:**
- Build system integrity checks
- Compilation verification
- Configuration file validation
- Hardware functionality tests (when device available)
- CI/CD integration support

## Building Tools

```bash
# Build all tools
make

# Clean build artifacts
make clean

# Install tools system-wide
sudo make install
```

## Development Usage

### Device Investigation
```bash
# First, detect connected devices
./apollo_detect -v

# Dump device registers for analysis
./apollo_dump /sys/bus/pci/devices/0000:01:00.0/resource0 0x00 1024

# Check if everything compiles
./apollo_test
```

### Debugging Workflow
1. Use `apollo_detect` to verify device connection
2. Use `apollo_dump` to examine hardware state
3. Use `apollo_test` to validate driver functionality
4. Check kernel logs: `dmesg | grep apollo`
5. Monitor system activity: `journalctl -f -u apollo`

## Safety Notes

- **apollo_dump** requires careful usage - incorrect register access can crash the system
- Always use PCI resource files when possible instead of direct memory access
- Root privileges are required for hardware access tools
- Test on development systems before production use

## Integration

These tools integrate with the main build system:

```bash
# Build everything including tools
make all

# Run full test suite
make test

# Clean all components
make clean
```

See the main project documentation for complete setup instructions.
