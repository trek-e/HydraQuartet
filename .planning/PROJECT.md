# Triax VCO

## What This Is

An 8-voice polyphonic dual-VCO module for VCV Rack, inspired by the TipTop Audio Triax 8. Each voice contains two oscillators with mixable waveforms, cross-modulation (FM, sync, XOR), and comprehensive CV control. Uses VCV's native polyphonic protocol instead of ART.

## Core Value

Rich, musical polyphonic oscillator with the dual-VCO-per-voice architecture and cross-modulation capabilities of the Triax 8 — through-zero FM that stays in tune, cross-sync between oscillators, and XOR waveshaping.

## Requirements

### Validated

(None yet — ship to validate)

### Active

**Per-Voice Architecture (x8 voices, 16 oscillators total):**
- [ ] Two VCOs per voice with independent controls
- [ ] All waveforms mixable simultaneously (not crossfade)

**VCO1 Features:**
- [ ] Four waveforms with individual volume controls: Triangle, Square, Sine, Saw
- [ ] Sub-oscillator (one octave below)
- [ ] PWM knob + CV input
- [ ] Vibrato amount knob + CV input (external LFO)
- [ ] Octave switch
- [ ] Detune knob (0 = in tune, positive = detuning for thickness)
- [ ] Sync switch: Off / Soft / Hard (syncs to VCO2)
- [ ] FM Source knob (selects which VCO1 waveform modulates VCO2)

**VCO2 Features:**
- [ ] Four waveforms with individual volume controls: Triangle, Square, Sine, Saw
- [ ] XOR output (combines pulse waves of VCO1 and VCO2, shaped by both PWM settings)
- [ ] Through-zero FM input from VCO1
- [ ] FM Amount knob
- [ ] PWM knob + CV input
- [ ] Vibrato amount knob + CV input (external LFO)
- [ ] Octave switch
- [ ] Fine knob (0-1 semitone smooth, beyond: stepped semitones to +1 octave)
- [ ] Sync switch: Off / Soft / Hard (syncs to VCO1)

**FM Implementation:**
- [ ] Through-zero FM that maintains tuning when VCOs at same pitch (wave shaping in tune)
- [ ] FM Source selects modulator waveform from VCO1
- [ ] Fine knob creates frequency ratios for timbral variety
- [ ] FM depth CV: polyphonic input + global input with attenuator

**Cross-Sync:**
- [ ] Both VCOs can sync simultaneously (cross-sync)
- [ ] Soft sync: resets based on waveform magnitude
- [ ] Hard sync: resets at start of other oscillator's cycle

**Voicing Modes:**
- [ ] Poly: Standard 8-voice polyphony (each note → one voice)
- [ ] Dual: Each note uses 2 voices (4-note max, fuller sound)
- [ ] Unison: All 8 voices on one note (16-VCO mono)

**Outputs:**
- [ ] Polyphonic audio output (8-channel)
- [ ] Mix output (mono sum of all voices)
- [ ] Velocity/Key tracking output with mode switch (+/-/KEYT)

**CV Inputs:**
- [ ] V/Oct (polyphonic)
- [ ] Gate (polyphonic)
- [ ] Waveform volumes (polyphonic CV control)
- [ ] FM depth (polyphonic + global with attenuator)
- [ ] PWM per VCO
- [ ] Vibrato per VCO

**Analog Character:**
- [ ] User-controllable analog character knob (drift, warmth, slight detuning between voices)

**Panel:**
- [ ] Wide panel (30+ HP) with all controls visible

### Out of Scope

- Individual voice outputs (1-8 jacks) — use VCV poly cables instead
- Individual gate outputs — use VCV poly cables instead
- ART protocol — replaced by VCV polyphonic protocol
- Auto-tune function — digital oscillators don't drift (unless analog character is enabled)

## Context

**Target Platform:** VCV Rack 2.x
**Inspiration:** TipTop Audio Triax 8 polyphonic analog oscillator
**Architecture Philosophy:** Dave Rossum-style oscillator design — vintage analog character with modern stability

The Triax 8's FM implementation is notably musical: when both VCOs are at the same pitch, FM produces wave shaping in tune rather than harsh detuned sounds typical of analog FM. The Fine knob on VCO2 creates frequency ratios between oscillators for exploring FM timbres while maintaining musicality.

XOR output combines the pulse waves of both VCOs — adjusting PWM on either oscillator changes the XOR waveform character.

## Constraints

- **Platform**: VCV Rack 2.x module (C++ with VCV SDK)
- **Polyphony**: Must handle 8 voices efficiently (16 oscillators, potentially 64+ waveform generators)
- **CPU**: Polyphonic modules need careful optimization — SIMD where possible

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| VCV poly instead of ART | Native integration with VCV ecosystem | — Pending |
| Skip individual voice outputs | Leverage VCV's poly cables, reduce panel complexity | — Pending |
| User-controllable analog character | Best of both worlds — clean when needed, warm when wanted | — Pending |
| Wide panel (30+ HP) | Match hardware ergonomics, room for all controls | — Pending |

---
*Last updated: 2026-01-22 after initialization*
