# Apollo Twin Audio Activation Guide

## 🎵 Getting Apollo Twin Audio Working

The Apollo Twin MkII is detected on Thunderbolt but doesn't automatically expose PCIe endpoints for audio I/O. This guide provides step-by-step instructions to activate the device and enable audio functionality.

## 📋 Current Status

### ✅ What's Working
- Thunderbolt connection established
- Device authorization successful
- Driver framework compiled and loaded
- Basic device detection functional

### ❌ What's Not Working
- PCIe endpoints not exposed
- No ALSA audio devices available
- Audio I/O not accessible

## 🚀 Step-by-Step Audio Activation

### Step 1: Verify Device Connection

```bash
# Check device detection
./tools/apollo_detect -v

# Expected output:
# Found Apollo device: Apollo Twin MkII
# Vendor: 0x1176, Device: 0x5, Authorized: 1
```

### Step 2: Attempt Device Activation

```bash
# Try automated activation (may need sudo)
sudo ./tools/apollo_activate

# Or manually check attributes
cat /sys/bus/thunderbolt/devices/0-1/authorized  # Should be 1
cat /sys/bus/thunderbolt/devices/0-1/device_name # Should show Apollo Twin
```

### Step 3: Check for PCIe Endpoints

```bash
# Look for Apollo PCI devices
lspci -nn | grep 1176

# If no devices found, try:
sudo lspci -nn | grep 1176

# Check Thunderbolt topology
ls /sys/bus/thunderbolt/devices/
```

### Step 4: Alternative Activation Methods

If automated activation fails, try these manual methods:

#### Method A: Thunderbolt Daemon
```bash
# Ensure Thunderbolt daemon is running
sudo systemctl start bolt
sudo systemctl enable bolt

# Check daemon status
boltctl list
```

#### Method B: Device Reset
```bash
# Unplug and re-plug the device
# Or try different Thunderbolt ports
# Or restart the system with device connected
```

#### Method C: Firmware Check
```bash
# Check system firmware
dmidecode | grep -i thunderbolt

# Update system firmware if available
# Check for Thunderbolt firmware updates
```

### Step 5: Manual PCIe Endpoint Activation

If the device still doesn't expose PCIe endpoints, we may need to:

1. **Reverse Engineer Activation Protocol**: Analyze how the official macOS/Windows drivers activate the device
2. **Implement Thunderbolt Commands**: Send specific control packets to enable audio mode
3. **Firmware Analysis**: Extract activation sequences from device firmware

### Step 6: Test Audio Functionality

Once PCIe endpoints appear:

```bash
# Load the driver
sudo insmod kernel/apollo.ko

# Check for ALSA device
aplay -l | grep Apollo

# Test audio playback
speaker-test -D hw:Apollo -c 2

# Check PipeWire integration
pactl list cards | grep Apollo
```

## 🔧 Troubleshooting Guide

### Issue: "No Apollo PCI devices found"

**Possible Causes:**
- Device not in audio mode
- Thunderbolt controller not properly configured
- Firmware compatibility issues

**Solutions:**
```bash
# Try different activation approaches
sudo ./tools/apollo_activate

# Check Thunderbolt controller
lspci | grep -i thunderbolt

# Verify device is powered
lsusb | grep -i universal  # Some Apollos show as USB
```

### Issue: "Device activation failed"

**Possible Causes:**
- Insufficient permissions
- Device already in correct mode
- Hardware-specific requirements

**Solutions:**
```bash
# Run with proper permissions
sudo ./tools/apollo_activate

# Check device state
cat /sys/bus/thunderbolt/devices/0-1/*

# Try device reset
# Unplug device, wait 10 seconds, replug
```

### Issue: "Kernel module loads but no audio"

**Possible Causes:**
- PCIe endpoints detected but not claimed by driver
- Driver probe function not matching device
- ALSA card registration failing

**Solutions:**
```bash
# Check kernel logs
dmesg | grep apollo

# Verify device IDs
lspci -nn | grep 1176

# Check driver binding
ls /sys/bus/pci/drivers/apollo/
```

## 🎯 Advanced Activation Techniques

### Technique 1: Thunderbolt Control Packets

The Apollo may require specific Thunderbolt control messages to enable audio mode:

```c
// Example control packet structure (hypothetical)
struct apollo_control_packet {
    uint32_t magic;      // 0xAPOLLO
    uint16_t command;    // ACTIVATE_AUDIO = 0x01
    uint16_t length;
    uint8_t data[];
};
```

### Technique 2: Firmware-Assisted Activation

Some devices load audio-specific firmware:

```bash
# Check for firmware files
find /lib/firmware -name "*apollo*" -o -name "*universal*"

# Manual firmware loading (if available)
echo 1 > /sys/bus/thunderbolt/devices/0-1/firmware_load
```

### Technique 3: USB Fallback Mode

Some Apollos can operate in USB audio mode:

```bash
# Check for USB audio devices
lsusb | grep -i audio
aplay -l | grep -i usb
```

## 📊 Expected Device Behavior

### Successful Activation Sequence:

1. **Thunderbolt Connection**: Device appears in `/sys/bus/thunderbolt/devices/0-1/`
2. **Authorization**: `authorized` attribute becomes `1`
3. **PCIe Exposure**: New PCI devices appear with vendor `1176`
4. **Driver Binding**: Apollo kernel module claims PCI devices
5. **ALSA Registration**: Audio devices appear in `aplay -l`
6. **Audio I/O**: Full duplex audio streaming available

### Current Status:
- ✅ Thunderbolt connection
- ✅ Device authorization
- ❌ PCIe endpoint exposure
- ❌ Driver binding
- ❌ ALSA registration
- ❌ Audio I/O

## 🔄 Next Development Steps

### Immediate (This Session):
1. **Test with root permissions**: `sudo ./tools/apollo_activate`
2. **Monitor system logs**: `dmesg -w | grep -i thunderbolt`
3. **Try device reset**: Unplug/replug Apollo
4. **Check alternative ports**: Test different Thunderbolt ports

### Short Term (Next Few Days):
1. **Implement Thunderbolt communication**: Add control packet sending
2. **Reverse engineer activation protocol**: Analyze device behavior
3. **Add firmware loading support**: If firmware is required
4. **Test USB audio mode**: As fallback option

### Long Term (Weeks):
1. **Complete driver implementation**: Full audio pipeline
2. **User-space control**: Gain, routing, monitoring
3. **Performance optimization**: Low latency tuning
4. **Documentation and packaging**: AUR package creation

## 💡 Key Insights

1. **Thunderbolt Complexity**: The Apollo requires explicit activation to expose PCIe endpoints
2. **Authorization ≠ Activation**: Being authorized doesn't mean audio is enabled
3. **Hardware-Specific**: Each Apollo model may need different activation sequences
4. **Root Access Required**: Device control attributes need elevated permissions

## 🎉 Success Criteria

Audio is working when:
- `lspci -nn | grep 1176` shows Apollo PCI devices
- `aplay -l | grep Apollo` shows ALSA audio devices
- `speaker-test -D hw:Apollo` produces audible test tones
- PipeWire/PulseAudio can route audio through the device

**Let's get that Apollo singing! 🎵**
