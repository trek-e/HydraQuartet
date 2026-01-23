# Project State: Triax VCO

**Updated:** 2026-01-23
**Session:** Phase 2 Plan 1 complete

---

## Project Reference

**Core Value:** Rich, musical polyphonic oscillator with dual-VCO-per-voice architecture and through-zero FM that stays in tune

**What Makes This Different:**
- 16 oscillators total (2 VCOs per voice x 8 voices)
- Through-zero FM that produces wave shaping in tune when VCOs at same pitch
- Cross-sync and XOR waveshaping for complex timbres
- SIMD-optimized for acceptable CPU usage with massive oscillator count

---

## Current Position

**Phase:** 2 - Core Oscillator with Antialiasing IN PROGRESS
**Status:** Plan 02-01 complete
**Progress:** Phase 2 started

```
[█████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░] 25% (1.5/8 phases)
```

**Next Action:** Continue Phase 2 (remaining plans if any, or proceed to Phase 3)

---

## Performance Metrics

**Roadmap Progress:**
- Phases complete: 1/8
- Phases in progress: 1/8 (Phase 2 - Core Oscillator)
- Phases pending: 6/8

**Requirements Coverage:**
- v1 requirements: 34 total
- Mapped to phases: 34 (100%)
- Completed: 13 (PANEL-01, PANEL-02, PANEL-03, FOUND-01, FOUND-02, CV-01, CV-02, OUT-01, OUT-02, WAVE-01, WAVE-02, WAVE-03, ALIAS-01)
- Remaining: 21

**Quality Gates:**
- Panel design verified: YES (36 HP, all controls, outputs distinguished)
- Polyphonic I/O verified: YES (8 voices, V/Oct tracking, mix output)
- Antialiasing verified: YES (MinBLEP sawtooth/square, integrated triangle, native sine)
- SIMD optimization verified: No (Phase 3)
- CPU budget verified: No (needs measurement)
- Through-zero FM quality verified: No (Phase 6)

---

## Accumulated Context

### Key Decisions Made

**Phase 2 Plan 1 (2026-01-23):**
- Per-voice scalar processing required for MinBLEP (not SIMD compatible)
- Triangle generated via leaky integration (0.999 decay) of antialiased square
- DC filter applied to mixed output, not individual waveforms
- TRCFilter API: call process() then highpass() (not single-call with state param)
- MinBLEP discontinuity insertion: subsample = (crossing_point - phasePrev) / deltaPhase
- Phase crossing detection: phasePrev < threshold && phaseNow >= threshold (avoid VCV Fundamental bug)

**Phase 1 Complete (2026-01-23):**
- SDK 2.6.6 installed at /Users/trekkie/projects/vcvrack_modules/Rack-SDK
- Panel is 36 HP (182.88mm x 128.5mm)
- Dark industrial blue theme (#1a1a2e)
- Three-column layout: VCO1 | Global | VCO2
- Outputs on gradient plate for visual distinction
- V/Oct uses dsp::FREQ_C4 and dsp::exp2_taylor5() for accuracy
- Mix output averages voices (divides by channel count)
- SIN1 knob connected as proof of parameter connectivity

**Known Issue:** Panel labels use SVG text elements which don't render in VCV Rack. User must convert to paths in Inkscape.

**Roadmap Structure (2026-01-22):**
- 8 phases derived from requirements and research
- Phase 2 (Core Oscillator) is critical foundation - antialiasing cannot be retrofitted
- Phase 3 (SIMD) is architectural milestone - must happen before Phase 4 doubles oscillator count
- Phase 6 (Through-Zero FM) is high-complexity differentiator
- Phase 7 (Hard Sync) leverages antialiasing research from Phase 6

### Research Flags for Future Phases

- Phase 2 needs CRITICAL research: PolyBLEP/polyBLAMP implementation (study Befaco EvenVCO, avoid VCV Fundamental bug)
- Phase 6 needs CRITICAL research: Through-zero FM algorithms and antialiasing strategies
- Phase 7 can leverage Phase 6 research: Sync antialiasing similar to FM approaches

---

## Session Continuity

### What We Just Completed
- Phase 2 Plan 1: Antialiased VCO1 with 4 waveforms
- MinBLEP generators for sawtooth and square waveforms
- Leaky integrator for triangle waveform
- DC filtering on mixed output
- All 4 VCO1 volume knobs functional (TRI1, SQR1, SIN1, SAW1)
- Per-voice state management for 16-channel polyphony

### What Comes Next
1. User verification: Test antialiasing quality at >2kHz
2. User verification: Test PWM sweep for clicks
3. User verification: Test polyphonic operation
4. Continue Phase 2 if additional plans exist, or proceed to Phase 3 (SIMD optimization)

### Files Modified This Session
- src/TriaxVCO.cpp - VCO1Voice struct, antialiased waveform generation, volume mixing
- .planning/phases/02-core-oscillator/02-01-SUMMARY.md - created
- .planning/STATE.md - updated with Phase 2 progress

---

*Last updated: 2026-01-23 after Phase 2 Plan 1 completion*
