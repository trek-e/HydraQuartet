# HydraQuartet VCO

An 8-voice polyphonic dual-VCO module for VCV Rack, inspired by the Tiptop Audio Triax8.

**Brand:** Synth-etic Intelligence

## Features

- **8-Voice Polyphony** - Full polyphonic support via VCV polyphonic cables
- **Dual VCO Architecture** - Two independent oscillators per voice (16 total)
- **Four Waveforms per VCO** - Triangle, Square, Sine, Sawtooth with individual volume controls
- **Through-Zero FM** - Musical frequency modulation that stays in tune at unison
- **Hard Sync** - Bidirectional sync between VCO1 and VCO2 with MinBLEP antialiasing
- **PWM** - Pulse width modulation on both VCOs with CV control
- **XOR Output** - Ring modulation combining pulse waves from both VCOs
- **Sub-Oscillator** - One octave below VCO1 with square/sine selection
- **SIMD Optimized** - Efficient float_4 processing for low CPU usage

## Controls

### VCO1 (Left Section)
- **Octave** - Octave shift (-2 to +2)
- **Detune** - Detuning for thickness/beating effects
- **TRI/SQR/SIN/SAW** - Individual waveform volume knobs
- **PWM** - Pulse width control with CV input
- **Sync** - Hard sync to VCO2
- **Sub Level** - Sub-oscillator volume with CV input
- **Sub Wave** - Sub-oscillator waveform switch (square/sine)

### VCO2 (Right Section)
- **Octave** - Octave shift (-2 to +2)
- **Fine** - Fine tuning for FM frequency ratios
- **TRI/SQR/SIN/SAW** - Individual waveform volume knobs
- **XOR** - XOR ring modulation output volume
- **PWM** - Pulse width control with CV input
- **Sync** - Hard sync to VCO1
- **FM Amt** - Through-zero FM depth with CV input

### Global (Center Section)
- **V/Oct** - Polyphonic pitch input
- **Gate** - Polyphonic gate input
- **Audio** - Polyphonic audio output (8 channels)
- **Mix** - Mono sum of all voices

## Installation

### From Release
1. Download the `.vcvplugin` file from the [Releases](https://github.com/trek-e/HydraQuartet/releases) page
2. Copy to your VCV Rack plugins folder:
   - **macOS:** `~/Library/Application Support/Rack2/plugins-mac-arm64/` (or `plugins-mac-x64`)
   - **Windows:** `%LOCALAPPDATA%/Rack2/plugins-win-x64/`
   - **Linux:** `~/.local/share/Rack2/plugins-lin-x64/`

### Building from Source
```bash
# Clone the repository
git clone https://github.com/trek-e/HydraQuartet.git
cd HydraQuartet

# Build (requires VCV Rack SDK)
export RACK_DIR=/path/to/Rack-SDK
make

# Install
make install
```

## Requirements

- VCV Rack 2.x

## License

GPL-3.0-or-later - See [LICENSE](LICENSE) for details.

## Author

Synth-etic Intelligence
syntheticint@thepainterofsilence.com

## Links

- [GitHub Repository](https://github.com/trek-e/HydraQuartet)
- [VCV Rack](https://vcvrack.com/)
