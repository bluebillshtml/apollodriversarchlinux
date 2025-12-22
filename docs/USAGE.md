# Apollo Twin Linux Driver Usage Guide

## Overview

The Apollo Twin Linux driver provides native audio I/O support for Universal Audio Apollo Twin Thunderbolt interfaces. This guide covers basic operation and advanced features.

## Basic Usage

### Audio Playback
```bash
# Play audio file
aplay -D hw:Apollo /path/to/audio.wav

# Play test tone
speaker-test -D hw:Apollo -c 2 -f 1000

# Play through PipeWire
pw-play /path/to/audio.wav
```

### Audio Recording
```bash
# Record from analog inputs
arecord -D hw:Apollo -c 2 -r 48000 -f S32_LE recording.wav

# Record through PipeWire
pw-record recording.wav
```

### Device Control

#### Gain Control
```bash
# Set analog input 1 gain to 30 dB
apolloctl gain 1 30.0

# Get current gain for input 1
apolloctl gain 1

# Set all inputs to unity gain
for i in {1..4}; do apolloctl gain $i 0.0; done
```

#### Phantom Power
```bash
# Enable phantom power for microphone input 1
apolloctl phantom 1 on

# Disable phantom power
apolloctl phantom 1 off

# Check phantom power status
apolloctl phantom 1
```

#### Input Routing
```bash
# Route input 1 to analog 2
apolloctl input 1 analog2

# Route input 2 to digital 1
apolloctl input 2 digital1

# Check current routing
apolloctl input 1
```

#### Monitor Control
```bash
# Set monitor to main output
apolloctl monitor main

# Set monitor to alternate output
apolloctl monitor alt

# Set monitor to cue mix
apolloctl monitor cue
```

#### Presets
```bash
# Save current settings as preset
apolloctl save my_preset

# Load preset
apolloctl load my_preset
```

## PipeWire Integration

### Device Discovery
```bash
# List available audio devices
pactl list cards

# List sources (inputs)
pactl list sources

# List sinks (outputs)
pactl list sinks
```

### Routing Audio
```bash
# Set default source (microphone input)
pactl set-default-source alsa_input.usb-Apollo_Twin-00.analog-stereo

# Set default sink (main output)
pactl set-default-sink alsa_output.usb-Apollo_Twin-00.analog-stereo

# Route application to Apollo
pactl move-sink-input <app_id> alsa_output.usb-Apollo_Twin-00.analog-stereo
```

### PulseAudio Compatibility
Applications using PulseAudio automatically work through PipeWire's compatibility layer.

## JACK Applications

### Basic Setup
```bash
# Start JACK with Apollo
jackd -d alsa -d hw:Apollo -r 48000 -p 128

# Or use PipeWire JACK compatibility
pw-jack <jack_application>
```

### Professional Audio Workflows
```bash
# Start with low latency settings
jackd -d alsa -d hw:Apollo -r 96000 -p 64 -n 3

# Use with Ardour
ardour6

# Use with Qtractor
qtractor
```

## Advanced Features

### Multi-client Setup
```bash
# Monitor multiple applications
pactl load-module module-null-sink sink_name=monitor

# Route DAW output to monitor
pactl load-module module-loopback source=alsa_output.usb-Apollo_Twin-00.analog-stereo sink=monitor

# Route microphone to DAW input
pactl load-module module-loopback source=alsa_input.usb-Apollo_Twin-00.analog-stereo sink=alsa_input.usb-DAW-00.capture
```

### Sample Rate Switching
```bash
# Change sample rate (requires application restart)
# Edit /etc/apollo/apollo.conf
# Set desired sample rate and restart daemon

# Check current rate
aplay -D hw:Apollo --dump-hw-params /dev/null
```

### DSP Monitoring
```bash
# Enable DSP monitoring (when implemented)
apolloctl monitor dsp

# Adjust monitor return level
# This will be added in future versions
```

## Monitoring and Diagnostics

### Device Status
```bash
# Show comprehensive device status
apolloctl status

# Monitor kernel logs
dmesg -w | grep apollo

# Check Thunderbolt connection
thunderboltctl list
```

### Performance Monitoring
```bash
# Check for audio underruns
jack_iodelay

# Monitor CPU usage
top -p $(pgrep -f apollod)

# Check PipeWire status
systemctl --user status pipewire
```

### Audio Quality Tests
```bash
# Frequency response test
sox -r 48000 -c 2 -b 32 /dev/urandom -t wav - | aplay -D hw:Apollo

# Latency measurement
jack_delay -I hw:Apollo -O hw:Apollo
```

## Configuration Files

### Main Configuration
Location: `/etc/apollo/apollo.conf`

Key settings:
- `analog_gain[1-4]`: Input gain in dB (0-65)
- `phantom_power[1-4]`: Phantom power (0=off, 1=on)
- `monitor_source`: Monitor output selection

### PipeWire Configuration
Location: `~/.config/pipewire/pipewire.conf`

Add device-specific settings:
```
context.modules = [
  { name = module-alsa-card
    args = {
      card-name = "alsa_card.usb-Apollo_Twin"
      device-profile = "analog-stereo"
    }
  }
]
```

## Troubleshooting

### Common Issues

#### No Audio Output
```bash
# Check device connection
aplay -l | grep Apollo

# Verify PipeWire routing
pactl info

# Reset audio system
systemctl --user restart pipewire pipewire-pulse
```

#### Distorted Audio
```bash
# Check gain levels
apolloctl status

# Reduce input gain
apolloctl gain 1 20.0

# Check for clipping indicators (future feature)
```

#### High Latency
```bash
# Use smaller buffer sizes
jackd -d alsa -d hw:Apollo -p 64

# Check system configuration
sysctl vm.swappiness
sysctl kernel.sched_latency_ns
```

#### Device Disconnection
```bash
# Reauthorize Thunderbolt device
thunderboltctl authorize <domain>:<port>

# Restart daemon
systemctl restart apollo
```

### Debug Information
```bash
# Collect system information
uname -a
lspci | grep -i thunderbolt
lsmod | grep apollo

# Generate debug logs
journalctl -u apollo --since "1 hour ago" > apollo_debug.log
dmesg | grep -i apollo > dmesg_debug.log
```

## Performance Tuning

### System Optimization
```bash
# Disable CPU frequency scaling for consistent latency
cpupower frequency-set -g performance

# Increase real-time priorities
usermod -a -G realtime $USER

# Optimize kernel parameters
echo 2048 > /proc/sys/dev/hpet/max-user-freq
```

### Audio Buffer Tuning
```bash
# Find optimal buffer size
jackd -d alsa -d hw:Apollo -r 48000 -p 128
# Adjust -p parameter until no xruns occur
```

### Network Audio (Advanced)
```bash
# Set up network audio bridge
jack_netsource -H host -o 2

# Use with remote Apollo
# Requires additional network configuration
```

