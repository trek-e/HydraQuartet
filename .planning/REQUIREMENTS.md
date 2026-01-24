# Requirements: HydraQuartet VCO

**Defined:** 2026-01-22
**Core Value:** Rich, musical polyphonic oscillator with dual-VCO-per-voice architecture and through-zero FM that stays in tune

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### Foundation

- [ ] **FOUND-01**: Module supports 8-voice polyphony via VCV polyphonic cables
- [ ] **FOUND-02**: V/Oct pitch tracking is accurate across full range
- [ ] **FOUND-03**: SIMD optimization using float_4 for CPU efficiency
- [ ] **FOUND-04**: MinBLEP antialiasing on all harmonically rich waveforms

### Waveforms

- [ ] **WAVE-01**: VCO1 outputs four waveforms: triangle, square, sine, sawtooth
- [ ] **WAVE-02**: VCO2 outputs four waveforms: triangle, square, sine, sawtooth
- [ ] **WAVE-03**: Each waveform has individual volume control (mixable simultaneously)
- [ ] **WAVE-04**: VCO1 has sub-oscillator output (one octave below)
- [ ] **WAVE-05**: VCO2 has XOR output (combines pulse waves of VCO1 and VCO2)

### Pitch Control

- [ ] **PITCH-01**: VCO1 has octave switch
- [ ] **PITCH-02**: VCO2 has octave switch
- [ ] **PITCH-03**: VCO1 has detune knob (0 = in tune, up = detuning for thickness)

### PWM

- [ ] **PWM-01**: VCO1 has PWM knob controlling square wave pulse width
- [ ] **PWM-02**: VCO1 has PWM CV input
- [ ] **PWM-03**: VCO2 has PWM knob controlling square wave pulse width
- [ ] **PWM-04**: VCO2 has PWM CV input
- [ ] **PWM-05**: PWM affects XOR output (both VCOs' PWM shape the XOR waveform)

### FM

- [ ] **FM-01**: VCO2 receives through-zero FM from VCO1
- [ ] **FM-02**: VCO2 has FM Amount knob controlling modulation depth
- [ ] **FM-03**: FM maintains tuning when VCOs at same pitch (wave shaping in tune)
- [ ] **FM-04**: FM depth has polyphonic CV input
- [ ] **FM-05**: FM depth has global CV input with attenuator

### Sync

- [ ] **SYNC-01**: VCO1 has sync switch: Off / Hard (syncs to VCO2)
- [ ] **SYNC-02**: VCO2 has sync switch: Off / Hard (syncs to VCO1)
- [ ] **SYNC-03**: Hard sync resets oscillator phase at start of other oscillator's cycle
- [ ] **SYNC-04**: Sync operations are properly antialiased

### Outputs

- [ ] **OUT-01**: Polyphonic audio output (8-channel)
- [ ] **OUT-02**: Mix output (mono sum of all voices)

### CV Inputs

- [ ] **CV-01**: V/Oct input (polyphonic)
- [ ] **CV-02**: Gate input (polyphonic)
- [ ] **CV-03**: Waveform volume CV inputs (polyphonic)

### Panel

- [ ] **PANEL-01**: Wide panel (30+ HP) with all controls visible
- [ ] **PANEL-02**: Follows VCV Rack panel guidelines (spacing, labeling, colors)
- [ ] **PANEL-03**: Output ports visually distinguished from inputs

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Sync (Extended)

- **SYNC-05**: Soft sync option (reverses waveform direction instead of phase reset)
- **SYNC-06**: Cross-sync capability (both VCOs sync simultaneously)

### FM (Extended)

- **FM-06**: FM Source knob on VCO1 (selects which waveform modulates VCO2)

### Voicing

- **VOICE-01**: Voicing mode switch: Poly / Dual / Unison
- **VOICE-02**: Dual mode assigns 2 voices per note (4-note max polyphony)
- **VOICE-03**: Unison mode plays all 8 voices on one note (16-VCO mono)

### Character

- **CHAR-01**: User-controllable analog character knob (drift, warmth)
- **CHAR-02**: Analog character has OFF position for clean digital

### Vibrato

- **VIB-01**: VCO1 has vibrato amount knob + CV input
- **VIB-02**: VCO2 has vibrato amount knob + CV input

### Pitch (Extended)

- **PITCH-04**: VCO2 has fine knob (0-1 semitone smooth, beyond: stepped to +1 octave)

### Outputs (Extended)

- **OUT-03**: Velocity/Key tracking output with mode switch (+/-/KEYT)

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Individual voice outputs (1-8 jacks) | Use VCV polyphonic cables instead |
| Individual gate outputs | Use VCV polyphonic cables instead |
| ART protocol | Replaced by VCV polyphonic protocol |
| Auto-tune function | Digital oscillators don't drift |
| Embedded fonts in panel SVG | Technical requirement - convert to paths |
| Menu-diving for core features | All controls visible on wide panel |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PANEL-01 | Phase 1 | Complete |
| PANEL-02 | Phase 1 | Complete |
| PANEL-03 | Phase 1 | Complete |
| FOUND-01 | Phase 1 | Complete |
| FOUND-02 | Phase 1 | Complete |
| CV-01 | Phase 1 | Complete |
| CV-02 | Phase 1 | Complete |
| OUT-01 | Phase 1 | Complete |
| OUT-02 | Phase 1 | Complete |
| FOUND-04 | Phase 2 | Complete |
| WAVE-01 | Phase 2 | Complete |
| WAVE-03 | Phase 2 | Complete |
| FOUND-03 | Phase 3 | Complete |
| WAVE-02 | Phase 4 | Complete |
| PITCH-01 | Phase 4 | Complete |
| PITCH-02 | Phase 4 | Complete |
| PITCH-03 | Phase 4 | Complete |
| PWM-01 | Phase 5 | Complete |
| PWM-02 | Phase 5 | Complete |
| PWM-03 | Phase 5 | Complete |
| PWM-04 | Phase 5 | Complete |
| WAVE-04 | Phase 5 | Complete |
| FM-01 | Phase 6 | Complete |
| FM-02 | Phase 6 | Complete |
| FM-03 | Phase 6 | Complete |
| FM-04 | Phase 6 | Complete |
| FM-05 | Phase 6 | Complete |
| SYNC-01 | Phase 7 | Pending |
| SYNC-02 | Phase 7 | Pending |
| SYNC-03 | Phase 7 | Pending |
| SYNC-04 | Phase 7 | Pending |
| WAVE-05 | Phase 8 | Pending |
| PWM-05 | Phase 8 | Pending |
| CV-03 | Phase 8 | Pending |

**Coverage:**
- v1 requirements: 34 total
- Mapped to phases: 34
- Unmapped: 0 (100% coverage)

---
*Requirements defined: 2026-01-22*
*Last updated: 2026-01-24 after Phase 6 completion*
