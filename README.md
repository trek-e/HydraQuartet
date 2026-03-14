# COLOSSUS · 16

**An 8-voice polyphonic dual-VCO module for VCV Rack 2, inspired by the TipTop Audio Triax 8.**

Part of the **Synth-etic Intelligence** module collection.

---

## Overview

COLOSSUS · 16 packs 16 oscillators into a single 30 HP module — two independent VCOs per voice across 8 voices of polyphony. It was designed from the ground up around the idea that a dual oscillator should be more than two oscillators sitting next to each other: VCO1 and VCO2 talk to each other through through-zero FM, hard sync, and XOR ring modulation, producing the kind of rich, evolving timbres that normally require multiple modules and careful patching.

Every harmonically-rich waveform (sawtooth, square, triangle, XOR) is antialiased with MinBLEP correction. The entire DSP engine runs on SIMD `float_4` processing, keeping CPU usage well under 2% at full 8-voice polyphony.

## Features

- **8-Voice Polyphony** — VCV polyphonic cables, up to 16 channels
- **Dual VCO per Voice** — 16 oscillators total, each with 4 mixable waveforms
- **Through-Zero FM** — VCO1 → VCO2 linear FM that stays in tune at unison
- **Hard Sync** — Bidirectional between VCO1 ↔ VCO2, MinBLEP antialiased
- **XOR Ring Modulation** — Pulse wave multiplication with full MinBLEP tracking of all 4 edge types
- **Sub-Oscillator** — One octave below VCO1, switchable square/sine, with dedicated output
- **PWM** — Per-VCO pulse width modulation with polyphonic CV and attenuverter
- **6 Waveform Volume CVs** — Polyphonic per-voice volume automation (SAW1, SQR1, SUB, XOR, SQR2, SAW2)
- **Per-Voice Outputs** — Individual audio and gate outputs for voices 1–8
- **Soft Clipping** — tanh() saturation at ±3V prevents harsh digital distortion
- **SIMD Optimized** — `float_4` processing, ~1.6% CPU at 8 voices (48 kHz)

## Panel Layout

```
┌─────────────────────────────────────────────────────────┐
│  COLOSSUS · 16                SYNTH-ETIC INTELLIGENCE   │
│                                                         │
│  ┌─VCO 1──────┐  ┌─GLOBAL──┐  ┌─VCO 2──────┐          │
│  │ DET OCT FM │  │  SYNC1  │  │ FM  OCT FIN │          │
│  │ SUB TRI SIN│VIB│  SYNC2  │VIB│ SIN TRI XOR│          │
│  │ PWM SQR SAW│  │  V/OCT  │  │ SAW SQR PWM│          │
│  │ [sw] [sub] │  │  GATE   │  │  [cv] [cv] │          │
│  │ cv  cv  cv │  │  AUDIO  │  │  cv  cv  cv│          │
│  └────────────┘  └─────────┘  └────────────┘          │
│  PWM1 │ GT1 GT2 GT3 GT4 │MIX│ GT5 GT6 GT7 GT8        │
│       │ V1  V2  V3  V4  │MIX│ V5  V6  V7  V8         │
└─────────────────────────────────────────────────────────┘
```

## Controls Reference

### VCO1 (Left Section)

| Control | Type | Range | Default | Description |
|---------|------|-------|---------|-------------|
| **Detune** | Knob | 0–50 cents | 0 | Detune VCO1 against VCO2 for thickness and beating |
| **Octave** | Snap knob | 16' 8' 4' 2' 1' | 4' (0V) | Octave selection in organ footage notation |
| **FM Source** | Snap knob | Sin/Tri/Saw/Sqr/Sub | Sine | Which VCO1 waveform modulates VCO2 via FM |
| **Sub Level** | Knob | 0–10 | 0 (off) | Sub-oscillator volume (one octave below VCO1) |
| **Triangle** | Knob | 0–10 | 0 | VCO1 triangle wave volume |
| **Sine** | Knob | 0–10 | 1 | VCO1 sine wave volume |
| **Vibrato** | Knob | 0–100% | 0% | VCO1 vibrato depth (~5.5 Hz sine LFO) |
| **PWM** | Knob | 0–100% | 50% | VCO1 square wave pulse width |
| **Square** | Knob | 0–10 | 1 | VCO1 square wave volume |
| **Sawtooth** | Knob | 0–10 | 0 | VCO1 sawtooth wave volume |
| **Sub Wave** | Toggle | Sqr / Sin | Square | Sub-oscillator waveform selection |

### VCO2 (Right Section)

| Control | Type | Range | Default | Description |
|---------|------|-------|---------|-------------|
| **FM Amt** | Knob | 0–10 | 0 (off) | Through-zero FM depth (VCO1 → VCO2) |
| **Octave** | Snap knob | 16' 8' 4' 2' | 4' (0V) | Octave selection |
| **Fine** | Knob | 0–10 | 0 | Fine tuning: 0–5 = 0–1 semitone (smooth), 5–10 = +2–12 semitones (FM ratios) |
| **Vibrato** | Knob | 0–100% | 0% | VCO2 vibrato depth |
| **Sine** | Knob | 0–10 | 0 | VCO2 sine wave volume |
| **Triangle** | Knob | 0–10 | 0 | VCO2 triangle wave volume |
| **XOR** | Knob | 0–10 | 0 (off) | XOR ring modulation output volume |
| **Sawtooth** | Knob | 0–10 | 0 | VCO2 sawtooth wave volume |
| **Square** | Knob | 0–10 | 1 | VCO2 square wave volume |
| **PWM** | Knob | 0–100% | 50% | VCO2 square wave pulse width |

### Global (Center Section)

| Control | Type | Description |
|---------|------|-------------|
| **Sync 1** | 3-way switch | VCO1 sync: Hard / Off / Soft (syncs VCO1 to VCO2) |
| **Sync 2** | 3-way switch | VCO2 sync: Hard / Off / Soft (syncs VCO2 to VCO1) |

### Inputs

| Jack | Polyphonic | Description |
|------|------------|-------------|
| **V/Oct** | Yes (up to 16 ch) | Pitch input — drives polyphony channel count |
| **Gate** | Yes | Gate input |
| **PWM1 CV** | Yes | VCO1 pulse width CV with attenuverter (±5V × att = ±0.5 PWM) |
| **PWM2 CV** | Yes | VCO2 pulse width CV with attenuverter |
| **FM CV** | Yes | FM depth CV (auto-detects mono/poly) |
| **SUB CV** | Yes | Sub-oscillator volume CV (0–10V, replaces knob when patched) |
| **SQR1 CV** | Yes | VCO1 square volume CV |
| **SAW1 CV** | Yes | VCO1 sawtooth volume CV |
| **SAW2 CV** | Yes | VCO2 sawtooth volume CV |
| **SQR2 CV** | Yes | VCO2 square volume CV |
| **XOR CV** | Yes | XOR output volume CV |

### Outputs

| Jack | Polyphonic | Description |
|------|------------|-------------|
| **Audio** | Yes (8 ch) | Main polyphonic output (VCO1 + VCO2 mix) |
| **Mix** | No (mono) | Mono sum of all voices |
| **Sub** | Yes (8 ch) | Dedicated sub-oscillator output |
| **Voice 1–8** | No (mono each) | Individual per-voice audio outputs |
| **Gate 1–8** | No (mono each) | Individual per-voice gate pass-through |
| **Gate Mix** | No (mono) | OR of all gates |

## Patch Ideas

### Classic Detuned Unison
Set VCO1 Sine to 10, VCO2 Sine to 10, Detune to ~30%. Play a chord. Instant thick analog pad.

### Through-Zero FM Bell
Set both VCOs to the same octave. Turn FM Amt to ~3. Use Fine knob on VCO2 to dial in harmonic ratios (the 5–10 range gives stepped semitones ideal for FM). The FM stays perfectly in tune — you get waveshaping, not detuning.

### Sync Lead
Enable Sync 2 → Hard. Raise VCO2 Sawtooth. Sweep VCO2 Fine knob for the classic hard sync lead sound. The MinBLEP antialiasing keeps it clean even at high pitches.

### XOR Ring Modulation
Turn up Square on both VCOs. Raise XOR volume. Detune VCO2 slightly with Fine. Sweep PWM on either VCO to morph the XOR character — both VCOs' pulse widths shape the ring modulation output.

### Per-Voice Volume Animation
Patch an 8-channel random source into SAW1 CV. Each voice gets independent sawtooth volume, creating shimmering polyphonic textures. CV replaces the knob when patched (0–10V maps directly to 0–10 volume).

## Technical Details

### DSP Architecture

- **VcoEngine struct** encapsulates all per-oscillator state (phase, MinBLEP buffers, delta tracking)
- Two VcoEngine instances per module (VCO1, VCO2), processed in SIMD groups of 4 voices
- Phase accumulation with `simd::clamp(freq * sampleTime, 0, 0.49)` prevents aliasing at extreme frequencies
- V/Oct conversion uses `dsp::exp2_taylor5()` for fast, accurate pitch tracking

### Antialiasing

All waveforms with discontinuities use subsample-accurate MinBLEP correction:

- **Sawtooth** — MinBLEP on phase wrap (1 edge type)
- **Square** — MinBLEP on phase wrap + PWM threshold crossing (2 edge types)
- **XOR** — MinBLEP tracking all 4 edge types: VCO1 wrap, VCO1 PWM fall, VCO2 wrap, VCO2 PWM fall
- **Triangle** — Direct phase calculation with MinBLEP correction
- **Sine** — Inherently band-limited, no correction needed

MinBLEP impulse uses 16 zero crossings × 16× oversampling, stored in a shared static lookup table.

### FM Implementation

Through-zero linear FM is applied after exponential pitch conversion:

```
freq2 = baseFreq2 + (fmModulator * fmDepth * baseFreq2)
```

This keeps both VCOs in tune at unison (FM Amount = 0 → perfect unison). At higher FM depths, the frequency can go negative (through-zero), clamped to a minimum of 0.1 Hz to prevent phase reversal crashes.

### Output Stage

1. Per-voice waveform mix (all volumes are SIMD `float_4`)
2. DC blocking via `dsp::TRCFilter` at 10 Hz cutoff
3. Soft clipping: `3.0 × tanh(signal / 3.0)` — saturates smoothly at ±3V
4. Output scaled to ±2V
5. NaN/Inf sanitization

### CV Behavior

Waveform volume CV inputs follow the **CV-replaces-knob** pattern (not additive):
- When a CV cable is patched, the knob value is ignored
- 0–10V maps directly to 0–10 volume (1:1, no scaling)
- Polyphonic CV gives per-voice volume control

## Building from Source

### Requirements

- VCV Rack SDK 2.x ([download](https://vcvrack.com/downloads))
- C++11 compiler (GCC 7+, Clang, or MSVC)
- GNU Make

### Build

```bash
git clone https://github.com/trek-e/Colossus16.git
cd Colossus16

# Point to your Rack SDK
export RACK_DIR=/path/to/Rack-SDK

# Build
make -j$(nproc)

# Install to VCV Rack plugins directory
make install
```

### Cross-Platform Notes

The Makefile auto-detects the platform and installs to the correct plugins directory:

| Platform | Plugins Directory |
|----------|-------------------|
| macOS ARM | `~/Library/Application Support/Rack2/plugins-mac-arm64/` |
| macOS Intel | `~/Library/Application Support/Rack2/plugins-mac-x64/` |
| Windows | `%LOCALAPPDATA%/Rack2/plugins-win-x64/` |
| Linux | `~/.Rack2/plugins-lin-x64/` |

## Other Synth-etic Intelligence Modules

| Module | Description |
|--------|-------------|
| **[Hurricane 8](https://github.com/trek-e/Hurricane8)** | 8-voice polyphonic PPG-style wavetable oscillator with mip-mapped band-limited playback |
| **[HydraQuartet VCF-OB](https://github.com/trek-e/HydraQuartetVCF)** | 16-voice polyphonic Oberheim-style multimode filter with 12dB SEM / 24dB OB-X modes |

## License

GPL-3.0-or-later — See [LICENSE](LICENSE) for details.

## Author

**Synth-etic Intelligence**
syntheticint@thepainterofsilence.com

- [GitHub](https://github.com/trek-e/Colossus16)
- [VCV Rack Library](https://library.vcvrack.com/?brand=Synth-etic+Intelligence)
