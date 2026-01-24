# Roadmap: HydraQuartet VCO

**Project:** 8-voice polyphonic dual-VCO module for VCV Rack
**Core Value:** Rich, musical polyphonic oscillator with dual-VCO-per-voice architecture and through-zero FM that stays in tune
**Created:** 2026-01-22
**Depth:** Standard (5-8 phases)

---

## Overview

This roadmap delivers the HydraQuartet VCO module in 8 phases, progressing from foundation through dual-oscillator architecture to advanced cross-modulation features. The phase structure respects critical architectural dependencies: antialiasing and SIMD optimization must be established early and cannot be retrofitted. Through-zero FM and hard sync receive dedicated phases due to their complexity and quality requirements.

**Total Requirements:** 34 v1 requirements
**Phase Count:** 8 phases
**Coverage:** 34/34 requirements mapped (100%)

---

## Phases

### Phase 1: Foundation & Panel

**Goal:** Development environment ready, panel designed, and basic polyphonic infrastructure established

**Dependencies:** None (starting point)

**Plans:** 2 plans

Plans:
- [x] 01-01-PLAN.md - SDK setup and 36 HP panel design
- [x] 01-02-PLAN.md - Polyphonic module implementation with V/Oct tracking

**Requirements:**
- PANEL-01: Wide panel (30+ HP) with all controls visible
- PANEL-02: Follows VCV Rack panel guidelines (spacing, labeling, colors)
- PANEL-03: Output ports visually distinguished from inputs
- FOUND-01: Module supports 8-voice polyphony via VCV polyphonic cables
- FOUND-02: V/Oct pitch tracking is accurate across full range
- CV-01: V/Oct input (polyphonic)
- CV-02: Gate input (polyphonic)
- OUT-01: Polyphonic audio output (8-channel)
- OUT-02: Mix output (mono sum of all voices)

**Success Criteria:**
1. User can see panel design with all controls and ports laid out (30+ HP)
2. Developer can compile and load the module in VCV Rack
3. User can send polyphonic V/Oct + Gate and see 8-channel polyphonic output
4. User can verify V/Oct tracking is accurate (C4 = 0V produces correct pitch)
5. Panel follows VCV spacing guidelines (can put thumbs between knobs)

---

### Phase 2: Core Oscillator with Antialiasing

**Goal:** Single VCO producing all waveforms with verified antialiasing quality

**Dependencies:** Phase 1 (infrastructure)

**Plans:** 1 plan

Plans:
- [x] 02-01-PLAN.md - MinBLEP antialiased waveforms with VCO1 volume controls

**Requirements:**
- FOUND-04: MinBLEP antialiasing on all harmonically rich waveforms
- WAVE-01: VCO1 outputs four waveforms: triangle, square, sine, sawtooth
- WAVE-03: Each waveform has individual volume control (partial - VCO1 only)

**Success Criteria:**
1. User can play sawtooth wave at high pitch (>2kHz) with no harsh metallic artifacts
2. User can verify with spectrum analyzer that harmonics are properly band-limited
3. User can switch between triangle, square, sine, sawtooth waveforms on VCO1
4. User can adjust individual waveform volumes and mix them simultaneously
5. Developer can verify phase accumulator stability over 10+ minutes continuous play (no drift)

---

### Phase 3: SIMD Polyphony

**Goal:** 8-voice polyphony running efficiently with SIMD optimization

**Dependencies:** Phase 2 (oscillator core must be complete before templating)

**Plans:** 1 plan

Plans:
- [x] 03-01-PLAN.md - SIMD float_4 engine with strided MinBLEP antialiasing

**Requirements:**
- FOUND-03: SIMD optimization using float_4 for CPU efficiency

**Success Criteria:**
1. User can play 8-note polyphonic chord with acceptable CPU usage (<5% baseline)
2. Developer can verify oscillator core is templated for float_4 SIMD processing
3. User can verify polyphonic patch behavior matches monophonic (no audio artifacts)
4. Developer can measure 3-4x CPU improvement vs naive per-voice processing

---

### Phase 4: Dual VCO Architecture

**Goal:** Two independent oscillators per voice with pitch control

**Dependencies:** Phase 3 (SIMD must be established before doubling oscillator count)

**Plans:** 1 plan

Plans:
- [x] 04-01-PLAN.md - VcoEngine extraction with dual oscillator wiring and pitch controls

**Requirements:**
- WAVE-02: VCO2 outputs four waveforms: triangle, square, sine, sawtooth
- WAVE-03: Each waveform has individual volume control (complete - VCO2 added)
- PITCH-01: VCO1 has octave switch
- PITCH-02: VCO2 has octave switch
- PITCH-03: VCO1 has detune knob (0 = in tune, up = detuning for thickness)

**Success Criteria:**
1. User can hear two oscillators per voice playing simultaneously (16 oscillators total)
2. User can detune VCO1 to create thickness/beating effect against VCO2
3. User can shift VCO1 and VCO2 independently by octaves
4. User can mix waveforms from both VCOs independently
5. Developer can verify CPU usage is approximately 2x Phase 3 baseline

---

### Phase 5: PWM & Sub-Oscillator

**Goal:** Pulse width modulation and sub-oscillator complete basic dual-VCO functionality

**Dependencies:** Phase 4 (requires dual VCO architecture)

**Plans:** 2 plans

Plans:
- [x] 05-01-PLAN.md - PWM CV wiring with attenuverters and LED indicators
- [x] 05-02-PLAN.md - Sub-oscillator with waveform switch and dedicated output

**Requirements:**
- PWM-01: VCO1 has PWM knob controlling square wave pulse width
- PWM-02: VCO1 has PWM CV input
- PWM-03: VCO2 has PWM knob controlling square wave pulse width
- PWM-04: VCO2 has PWM CV input
- WAVE-04: VCO1 has sub-oscillator output (one octave below)

**Success Criteria:**
1. User can sweep PWM knob on VCO1 and hear pulse width change (no clicks)
2. User can modulate PWM via CV input (e.g., with LFO) without audible clicks
3. User can sweep PWM knob on VCO2 and hear pulse width change (no clicks)
4. User can hear sub-oscillator one octave below VCO1 fundamental
5. Developer can verify sub-oscillator tracks VCO1 pitch exactly (-1 octave)

---

### Phase 6: Through-Zero FM

**Goal:** Musical frequency modulation that maintains tuning at unison

**Dependencies:** Phase 5 (requires stable dual VCO with pitch control)

**Plans:** 1 plan

Plans:
- [x] 06-01-PLAN.md - Through-zero linear FM with CV control and attenuverter

**Requirements:**
- FM-01: VCO2 receives through-zero FM from VCO1
- FM-02: VCO2 has FM Amount knob controlling modulation depth
- FM-03: FM maintains tuning when VCOs at same pitch (wave shaping in tune)
- FM-04: FM depth has polyphonic CV input
- FM-05: FM depth has global CV input with attenuator

**Success Criteria:**
1. User can set both VCOs to same pitch and apply FM to create wave shaping (stays in tune)
2. User can increase FM amount and hear timbral changes without pitch drift
3. User can modulate FM depth via polyphonic CV (per-voice control)
4. User can modulate FM depth via global CV with attenuator
5. Developer can verify with spectrum analyzer that FM at audio rates doesn't produce severe aliasing

---

### Phase 7: Hard Sync

**Goal:** Cross-sync between oscillators with proper antialiasing

**Dependencies:** Phase 5 (requires dual VCO), research insights from Phase 6 (sync antialiasing)

**Requirements:**
- SYNC-01: VCO1 has sync switch: Off / Hard (syncs to VCO2)
- SYNC-02: VCO2 has sync switch: Off / Hard (syncs to VCO1)
- SYNC-03: Hard sync resets oscillator phase at start of other oscillator's cycle
- SYNC-04: Sync operations are properly antialiased

**Success Criteria:**
1. User can enable VCO1→VCO2 hard sync and hear classic sync sweep sound
2. User can enable VCO2→VCO1 hard sync (bidirectional capability)
3. User can sweep synced oscillator pitch without hearing clicks or digital artifacts
4. Developer can verify sync triggers on rising edge (0V crossing from below)
5. Developer can verify with spectrum analyzer that sync doesn't produce excessive aliasing

---

### Phase 8: XOR Waveshaping

**Goal:** XOR output combining pulse waves with PWM interaction

**Dependencies:** Phase 5 (requires PWM from both VCOs), Phase 7 (completes modulation features)

**Requirements:**
- WAVE-05: VCO2 has XOR output (combines pulse waves of VCO1 and VCO2)
- PWM-05: PWM affects XOR output (both VCOs' PWM shape the XOR waveform)
- CV-03: Waveform volume CV inputs (polyphonic)

**Success Criteria:**
1. User can hear XOR output combining pulse waves from VCO1 and VCO2
2. User can adjust PWM on VCO1 and hear XOR waveform character change
3. User can adjust PWM on VCO2 and hear XOR waveform character change
4. User can modulate waveform volumes via polyphonic CV inputs
5. Developer can verify XOR output is properly antialiased (no excessive harmonics above Nyquist)

---

## Progress

| Phase | Status | Requirements | Completion |
|-------|--------|--------------|------------|
| 1 - Foundation & Panel | Complete | 9 | 100% |
| 2 - Core Oscillator | Complete | 3 | 100% |
| 3 - SIMD Polyphony | Complete | 1 | 100% |
| 4 - Dual VCO Architecture | Complete | 5 | 100% |
| 5 - PWM & Sub-Oscillator | Complete | 5 | 100% |
| 6 - Through-Zero FM | Complete | 5 | 100% |
| 7 - Hard Sync | Pending | 4 | 0% |
| 8 - XOR Waveshaping | Pending | 3 | 0% |

**Overall:** 28/34 requirements complete (82%)

---

## Notes

### Critical Architectural Decisions

**Phase 2 is make-or-break:** Antialiasing implementation cannot be retrofitted. Research recommends implementing polyBLEP/polyBLAMP from first principles (VCV Fundamental has documented minBLEP bug). Study Befaco EvenVCO for reference implementation.

**Phase 3 is mandatory:** With 16 oscillators total, SIMD float_4 optimization is required for acceptable CPU usage. Must happen before Phase 4 doubles the oscillator count.

**Phase 6 is high-complexity:** Through-zero FM is a key differentiator but technically challenging. Requires proper algorithm (phase runs backwards when frequency negative) and careful antialiasing. Budget 1-2 weeks for this phase.

**Phase 7 antialiasing:** Hard sync creates discontinuities that must be properly antialiased. Research from Phase 6 (FM antialiasing strategies) will inform this phase.

### Research Integration

Research flags indicate deep study needed for:
- Phase 2: PolyBLEP/polyBLAMP implementation (study Befaco EvenVCO, Bogaudio)
- Phase 6: Through-zero FM algorithms and antialiasing strategies
- Phase 7: Sync antialiasing (likely similar to FM approaches)

### v2 Deferred Features

The following requirements are deferred to v2 and not in this roadmap:
- SYNC-05, SYNC-06: Soft sync and cross-sync enhancements
- FM-06: FM source knob (waveform selection)
- VOICE-01, VOICE-02, VOICE-03: Voicing modes (Poly/Dual/Unison)
- CHAR-01, CHAR-02: Analog character control
- VIB-01, VIB-02: Vibrato controls
- PITCH-04: Fine tuning knob for VCO2
- OUT-03: Velocity/Key tracking output

---

*Last updated: 2026-01-23*
