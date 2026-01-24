# Project State: HydraQuartet VCO

**Updated:** 2026-01-23
**Session:** Phase 5 complete (PWM & Sub-Oscillator)

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

**Phase:** 5 - PWM & Sub-Oscillator COMPLETE
**Plan:** 2 of 2 complete
**Status:** Phase 5 complete, ready for Phase 6
**Progress:** 5/8 phases complete

```
[██████████████████████████████░░░░░░░] 63%
```

**Next Action:** Plan Phase 6 (Through-Zero FM)

---

## Performance Metrics

**Roadmap Progress:**
- Phases complete: 5/8
- Phases in progress: 0/8
- Phases pending: 3/8

**Requirements Coverage:**
- v1 requirements: 34 total
- Mapped to phases: 34 (100%)
- Completed: 23 (PANEL-01, PANEL-02, PANEL-03, FOUND-01, FOUND-02, FOUND-03, FOUND-04, CV-01, CV-02, OUT-01, OUT-02, WAVE-01, WAVE-02, WAVE-03, WAVE-04, PITCH-01, PITCH-02, PITCH-03, PWM-01, PWM-02, PWM-03, PWM-04)
- Remaining: 11

**Quality Gates:**
- Panel design verified: YES (36 HP, all controls, outputs distinguished)
- Polyphonic I/O verified: YES (8 voices, V/Oct tracking, mix output)
- Antialiasing verified: YES (MinBLEP on saw/square, triangle via integration)
- SIMD optimization verified: YES (0.8% CPU at 8 voices, float_4 processing)
- CPU budget verified: YES (<5% target, achieved ~1.6% with dual VCO)
- Dual VCO verified: YES (VcoEngine struct, independent pitch control)
- PWM CV modulation verified: YES (polyphonic CV, attenuverters, LED indicators)
- Sub-oscillator verified: YES (-1 octave tracking, square/sine switch, dedicated output)
- Through-zero FM quality verified: No (Phase 6)

---

## Accumulated Context

### Key Decisions Made

**Phase 5 Complete (2026-01-23):**
- PWM CV scaled at 0.1 factor: +/-5V * att gives +/-0.5 PWM contribution (full sweep)
- PWM clamped to 0.01-0.99 to avoid DC offset at extremes
- LED brightness based on peak CV across all polyphonic channels
- Trimpot used for attenuverters (smaller than main knobs)
- Bipolar CV pattern: base + cv * att * scale, clamped to safe range
- Sub-oscillator ignores VCO1 detune for stable foundation (uses basePitch + octave1 - 1.f)
- Sub-oscillator uses simple phase accumulator (no MinBLEP - frequencies low enough)
- Sub output at ±2V (reduced during testing)

**Phase 4 Complete (2026-01-23):**
- VcoEngine struct encapsulates all per-oscillator state (phase, MinBLEP buffers, triState)
- DC filters remain module members (operate on mixed output, not per-VCO)
- VCO1 receives detune, VCO2 is reference oscillator (0 detune = perfect unison)
- Detune range 0-50 cents via V/Oct conversion (detuneVolts = knob * 50/1200)
- Both VCOs processed in same SIMD loop with independent frequency calculations
- Human verified: dual oscillators, beating effect, octave switches, ~1.6% CPU

**Phase 3 Complete (2026-01-23):**
- Custom MinBlepBuffer<32> template struct with lane-based discontinuity insertion
- Process loop iterates in groups of 4 using getPolyVoltageSimd<float_4>
- All 4 waveforms using float_4 SIMD (saw, square, triangle, sine)
- simd::movemask() to identify lanes needing MinBLEP insertions
- simd::ifelse() for branchless PWM edge detection
- Horizontal sum via _mm_hadd_ps for efficient mix output
- DC filters kept scalar (not in hot path)
- Human verified: 0.8% CPU (far below 5% target), all waveforms clean

**Phase 2 Complete (2026-01-23):**
- MinBLEP antialiasing using dsp::MinBlepGenerator<16, 16, float>
- Triangle wave via leaky integrator (0.999 decay) of antialiased square
- PWM edge detection avoids VCV Fundamental click bug (only insert on phase crossing)
- DC filtering on mixed output with 10Hz highpass

**Phase 1 Complete (2026-01-23):**
- SDK 2.6.6 installed at /Users/trekkie/projects/vcvrack_modules/Rack-SDK
- Panel is 36 HP (182.88mm x 128.5mm)
- Dark industrial blue theme (#1a1a2e)
- Three-column layout: VCO1 | Global | VCO2
- V/Oct uses dsp::FREQ_C4 and dsp::exp2_taylor5() for accuracy

**Known Issue:** Panel labels use SVG text elements which don't render in VCV Rack. User must convert to paths in Inkscape.

### Research Flags for Future Phases

- Phase 6 needs CRITICAL research: Through-zero FM algorithms and antialiasing strategies
- Phase 7 can leverage Phase 6 research: Sync antialiasing similar to FM approaches

---

## Session Continuity

### What We Just Completed
- Phase 5 (PWM & Sub-Oscillator) complete
- Plan 1: PWM CV inputs with attenuverters and LED indicators
- Plan 2: Sub-oscillator at -1 octave with square/sine switch and dedicated output
- Human verified: sub tracks VCO1, waveform switch works, ignores detune

### What Comes Next
1. Plan Phase 6 (Through-Zero FM)
2. Research through-zero FM algorithms and antialiasing strategies
3. Implement VCO2 receiving FM from VCO1

### Files Modified This Session
- src/HydraQuartetVCO.cpp - Sub-oscillator params, DSP, output, widget controls
- .planning/phases/05-pwm-sub-oscillator/05-02-SUMMARY.md - created

---

*Last updated: 2026-01-23 after Phase 5 completion*
