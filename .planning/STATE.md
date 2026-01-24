# Project State: HydraQuartet VCO

**Updated:** 2026-01-24
**Session:** Phase 6 Plan 1 complete (Through-Zero FM)

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

**Phase:** 6 - Through-Zero FM IN PROGRESS
**Plan:** 1 of 1 complete
**Status:** Phase 6 complete, ready for Phase 7
**Progress:** 6/8 phases complete

```
[████████████████████████████████████░░] 75%
```

**Next Action:** Plan Phase 7 (Cross-Sync)

---

## Performance Metrics

**Roadmap Progress:**
- Phases complete: 6/8
- Phases in progress: 0/8
- Phases pending: 2/8

**Requirements Coverage:**
- v1 requirements: 34 total
- Mapped to phases: 34 (100%)
- Completed: 27 (PANEL-01, PANEL-02, PANEL-03, FOUND-01, FOUND-02, FOUND-03, FOUND-04, CV-01, CV-02, OUT-01, OUT-02, WAVE-01, WAVE-02, WAVE-03, WAVE-04, PITCH-01, PITCH-02, PITCH-03, PWM-01, PWM-02, PWM-03, PWM-04, FM-01, FM-02, FM-03, FM-04, FM-05)
- Remaining: 7

**Quality Gates:**
- Panel design verified: YES (36 HP, all controls, outputs distinguished)
- Polyphonic I/O verified: YES (8 voices, V/Oct tracking, mix output)
- Antialiasing verified: YES (MinBLEP on saw/square, triangle via integration)
- SIMD optimization verified: YES (0.8% CPU at 8 voices, float_4 processing)
- CPU budget verified: YES (<5% target, achieved ~1.6% with dual VCO)
- Dual VCO verified: YES (VcoEngine struct, independent pitch control)
- PWM CV modulation verified: YES (polyphonic CV, attenuverters, LED indicators)
- Sub-oscillator verified: YES (-1 octave tracking, square/sine switch, dedicated output)
- Through-zero FM quality verified: YES (linear FM, maintains pitch at unison, poly/mono CV)

---

## Accumulated Context

### Key Decisions Made

**Phase 6 Plan 1 Complete (2026-01-24):**
- Linear FM applied after exponential pitch conversion (maintains tuning at unison)
- FM depth clamped to 0-2 range (allows up to 2x frequency modulation)
- Single FM_INPUT auto-detects poly/mono instead of separate inputs
- Clamp freq2 to positive 0.1Hz minimum (quasi through-zero, not true phase reversal)
- 0.1 CV scaling factor: +/-5V * 1.0 att * 0.1 = +/-0.5 contribution (matches PWM pattern)
- Auto-detect poly/mono pattern: check getChannels(), use getPolyVoltageSimd or broadcast

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

- Phase 7 can leverage Phase 6 research: Sync antialiasing similar to FM approaches
- Cross-sync may need special handling for MinBLEP compatibility

---

## Session Continuity

### What We Just Completed
- Phase 6 Plan 1 (Through-Zero FM) complete
- Linear FM: VCO1 modulates VCO2 frequency (freq2 += freq1 * fmDepth)
- Auto-detecting poly/mono FM CV input with attenuverter
- FM_CV_LIGHT indicator for CV activity
- Maintains pitch at unison for wave shaping synthesis

### What Comes Next
1. Plan Phase 7 (Cross-Sync)
2. Research cross-sync antialiasing strategies
3. Implement hard sync between VCOs

### Files Modified This Session
- src/HydraQuartetVCO.cpp - FM_ATT_PARAM, FM_CV_LIGHT, through-zero FM DSP, widget controls
- .planning/phases/06-through-zero-fm/06-01-SUMMARY.md - created

---

*Last updated: 2026-01-24 after Phase 6 Plan 1 completion*
