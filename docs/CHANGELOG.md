# Changelog

All notable changes to COLOSSUS · 16 are documented here.

## [2.0.9] — 2026-03-14

### Changed
- **Renamed from HydraQuartet VCO to COLOSSUS · 16** — new module identity under Synth-etic Intelligence
- Plugin slug, module slug, all source files, SVG panel, and documentation updated
- **Panel resized from 40 HP to 30 HP** (152.4mm) — eliminated dead space, tighter layout
- Redesigned component layout with consistent 13mm grid spacing across all sections
- Fixed FM CV / PWM2 CV collision (was 5mm apart, now 13mm)
- Fixed Sub output / SAW1 CV collision (was 7.1mm apart, now 16.4mm)
- All documentation updated to reflect 30 HP panel dimensions

## [2.0.8] — 2026-01-31

### Changed
- Rebranded to **Synth-etic Intelligence** manufacturer identity
- Panel updated with redesigned layout matching Synth-etic Intelligence design language

## [2.0.7] — 2026-01-29

### Added
- **XOR ring modulation output** — `sqr1 × sqr2` with full MinBLEP antialiasing tracking all 4 edge types
- **6 polyphonic CV inputs** for waveform volume control (SAW1, SQR1, SUB, XOR, SQR2, SAW2)
- **Soft clipping** via `tanh()` saturation at ±3V on output stage
- V/Oct input repositioned to center GLOBAL section for visibility

### Changed
- Panel expanded to accommodate new CV inputs
- CV-replaces-knob behavior: patching a waveform CV bypasses the corresponding knob (0–10V = 0–10 volume)

## [2.0.6] — 2026-01-24

### Added
- **Hard sync** between VCO1 ↔ VCO2 with MinBLEP antialiasing
- Bidirectional sync capability (both VCOs can sync simultaneously)
- Subsample-accurate sync trigger detection
- Separate waveform generation and sync phases for clean antialiasing

## [2.0.5] — 2026-01-24

### Added
- **Through-zero linear FM** — VCO1 modulates VCO2's frequency
- FM maintains tuning at unison (waveshaping in tune)
- FM depth CV with polyphonic/monophonic auto-detection
- FM CV activity LED indicator

## [2.0.4] — 2026-01-23

### Added
- **PWM CV inputs** for both VCOs with bipolar attenuverter processing
- **LED activity indicators** for PWM CV inputs
- **Sub-oscillator** tracking VCO1 at −1 octave
- Sub-oscillator waveform switch (square/sine)
- Dedicated sub-oscillator output jack

## [2.0.3] — 2026-01-23

### Added
- **Dual VCO architecture** — VcoEngine struct extracted for reuse
- VCO2 with independent waveforms, octave switch, and fine tuning
- Detune knob on VCO1 (0–50 cents via V/Oct conversion)
- VCO2 fine tuning with dual-range behavior (smooth 0–1 semitone, then stepped 2–12 semitones)

## [2.0.2] — 2026-01-23

### Changed
- **SIMD optimization** — Converted from scalar per-voice to `float_4` SIMD processing
- CPU usage reduced from ~3.2% to ~0.8% at 8 voices (4× improvement)
- MinBLEP buffers restructured for strided SIMD lane access

## [2.0.1] — 2026-01-23

### Added
- **MinBLEP antialiasing** on sawtooth and square waveforms
- Triangle wave via leaky integration of antialiased square
- Four waveforms per VCO: triangle, square, sine, sawtooth
- Individual waveform volume controls

## [2.0.0] — 2026-01-22

### Added
- Initial module scaffold with VCV Rack SDK 2.6.6
- 36 HP panel design (dark industrial blue)
- 8-voice polyphonic infrastructure via V/Oct-driven channel count
- V/Oct tracking with `dsp::exp2_taylor5()`
- Polyphonic audio output and mono mix output
