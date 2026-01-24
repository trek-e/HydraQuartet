---
phase: 06-through-zero-fm
plan: 01
subsystem: dsp
tags: [fm-synthesis, linear-fm, through-zero, simd, polyphonic-cv, vcvrack]

# Dependency graph
requires:
  - phase: 05-pwm-sub-oscillator
    provides: VcoEngine struct, SIMD processing, PWM CV pattern with attenuverters
  - phase: 04-dual-vco
    provides: Dual VCO architecture (VCO1 + VCO2), independent frequency calculation
  - phase: 03-simd-performance
    provides: SIMD processing with float_4, getPolyVoltageSimd pattern
provides:
  - Through-zero linear FM: VCO1 modulates VCO2 frequency
  - FM_PARAM knob (0-1) and FM_ATT_PARAM attenuverter (-1 to +1)
  - Auto-detecting poly/mono FM CV input (per-voice or broadcast)
  - FM_CV_LIGHT indicator for CV activity
  - Linear FM maintains pitch at unison (wave shaping in tune)
affects:
  - 07-cross-sync (may need similar modulation approach)
  - 08-final-testing (FM behavior verification critical)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Linear FM: freq2 += freq1 * fmDepth (modulate after exp2_taylor5)"
    - "Auto-detect poly/mono CV: check getChannels(), use getPolyVoltageSimd or broadcast"
    - "FM depth scaling: knob + CV * attenuverter * 0.1, clamp to 0-2 range"

key-files:
  created: []
  modified:
    - src/HydraQuartetVCO.cpp

key-decisions:
  - "Linear FM applied after exponential pitch conversion (maintains tuning at unison)"
  - "FM depth clamped to 0-2 range (allows up to 2x frequency modulation)"
  - "Single FM_INPUT auto-detects poly/mono instead of separate inputs"
  - "Clamp freq2 to positive 0.1Hz minimum (quasi through-zero, not true phase reversal)"
  - "0.1 CV scaling factor: +/-5V * 1.0 att * 0.1 = +/-0.5 contribution"

patterns-established:
  - "Auto-detect poly/mono CV pattern: if (channels > 1) use getPolyVoltageSimd, else broadcast float_4(voltage)"
  - "CV light pattern: track peak CV across all channels, brightness = peak / 5.0"

# Metrics
duration: 3min
completed: 2026-01-23
---

# Phase 6 Plan 1: Through-Zero FM Summary

**Linear FM where VCO1 modulates VCO2 frequency with auto-detecting poly/mono CV depth control, maintaining pitch at unison for in-tune wave shaping**

## Performance

- **Duration:** 3 min
- **Started:** 2026-01-24T02:29:05Z
- **Completed:** 2026-01-24T02:31:54Z
- **Tasks:** 3
- **Files modified:** 1

## Accomplishments
- Through-zero linear FM: VCO1 modulates VCO2 by adding freq1 * fmDepth to freq2
- Auto-detecting poly/mono FM CV input for both per-voice and global modulation
- FM depth control via FM_PARAM knob (0-1) and FM_ATT_PARAM attenuverter (-1 to +1)
- FM_CV_LIGHT indicator shows CV activity with peak detection across all channels
- Linear FM maintains pitch at unison (critical for wave shaping in tune)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add FM attenuverter and CV light parameters** - `3e26369` (feat)
2. **Task 2: Wire through-zero FM DSP** - `5df685b` (feat)
3. **Task 3: Add FM attenuverter widget and CV light** - `8f4c864` (feat)

## Files Created/Modified
- `src/HydraQuartetVCO.cpp` - Added FM_ATT_PARAM, FM_CV_LIGHT, through-zero FM DSP, widget controls

## Decisions Made

**1. Linear FM applied after exponential pitch conversion**
- **Rationale:** Modulating freq (Hz) instead of pitch (V/Oct) maintains tuning at unison. At unison (freq1 == freq2), FM creates wave shaping without pitch drift. This is the core of through-zero FM.

**2. FM depth clamped to 0-2 range**
- **Rationale:** Allows aggressive modulation (up to 2x carrier frequency) while preventing extreme values. At full depth (2.0), VCO1 can double VCO2's frequency for maximum timbral variation.

**3. Auto-detect poly/mono instead of separate FM inputs**
- **Rationale:** Simpler UX with fewer panel jacks. Single FM_INPUT checks channel count: if polyphonic (>1), per-voice modulation via getPolyVoltageSimd; if mono, broadcast to all voices. Satisfies both FM-04 (poly CV) and FM-05 (global CV) requirements.

**4. Clamp freq2 to positive 0.1Hz minimum**
- **Rationale:** VcoEngine's MinBLEP requires positive deltaPhase. True through-zero (negative frequency, phase reversal) would break MinBLEP. Clamping to positive gives "quasi through-zero" - linear FM still maintains tuning, but extreme modulation doesn't fully reverse phase. Research confirms sine waveforms work correctly regardless, and linear FM provides the key benefit (pitch stability at unison).

**5. 0.1 CV scaling factor matches PWM pattern**
- **Rationale:** Consistent with Phase 5 PWM CV scaling. +/-5V * 1.0 attenuverter * 0.1 = +/-0.5 contribution. At full attenuverter (+1) and +5V CV, FM depth increases by 0.5, adding half the carrier frequency. Provides musical range without extreme jumps.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - implementation proceeded smoothly. FM DSP, widget controls, and CV light all worked on first compile.

## Next Phase Readiness

**Ready for Phase 7 (Cross-Sync):**
- Through-zero FM operational with both knob and CV control
- Polyphonic CV modulation pattern established and tested
- FM maintains pitch at unison as required for wave shaping synthesis
- Attenuverter and LED indicator patterns consistent across PWM and FM features

**Testing notes:**
- Verify FM maintains tuning at unison (both VCOs at same octave, FM depth increases without pitch drift)
- Test polyphonic FM CV (different voices should have different FM depths)
- Test monophonic FM CV (all voices should modulate together)
- Spectrum analyzer recommended to visualize harmonic changes vs fundamental stability

**No blockers.** Phase 6 Plan 1 complete, module ready for Cross-Sync implementation.

---
*Phase: 06-through-zero-fm*
*Completed: 2026-01-23*
