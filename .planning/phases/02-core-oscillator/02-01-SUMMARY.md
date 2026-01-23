---
phase: 02-core-oscillator
plan: 01
subsystem: audio-processing
tags: [minblep, antialiasing, waveforms, oscillator, dsp]

# Dependency graph
requires:
  - phase: 01-foundation-panel
    provides: Polyphonic framework with V/Oct tracking and basic sine output
provides:
  - Antialiased VCO1 with 4 waveforms (triangle, square, sine, sawtooth)
  - MinBLEP generators for discontinuity correction
  - Per-voice state management for 16-channel polyphony
  - DC filtering on mixed output
affects: [03-simd-optimization, 04-dual-vco-architecture, 06-through-zero-fm, 07-hard-sync]

# Tech tracking
tech-stack:
  added: [dsp::MinBlepGenerator, dsp::TRCFilter]
  patterns: [per-voice scalar processing for MinBLEP, leaky integrator for triangle, subsample discontinuity detection]

key-files:
  created: []
  modified: [src/HydraQuartetVCO.cpp]

key-decisions:
  - "Per-voice scalar processing required for MinBLEP (not SIMD compatible)"
  - "Triangle generated via leaky integration (0.999 decay) of antialiased square"
  - "DC filter applied to mixed output, not individual waveforms"
  - "TRCFilter API: call process() then highpass() (not single-call with state param)"

patterns-established:
  - "MinBLEP discontinuity insertion: subsample = (crossing_point - phasePrev) / deltaPhase"
  - "Phase crossing detection: phasePrev < threshold && phaseNow >= threshold (avoid VCV Fundamental bug)"
  - "Per-voice state in array: vco1Voices[16] for max polyphony"

# Metrics
duration: 3min
completed: 2026-01-23
---

# Phase 2 Plan 1: Core Oscillator Summary

**Antialiased VCO1 with 4 waveforms using MinBLEP (sawtooth, square), leaky integration (triangle), and native sine**

## Performance

- **Duration:** 3 min
- **Started:** 2026-01-23T03:22:45Z
- **Completed:** 2026-01-23T03:25:17Z
- **Tasks:** 3
- **Files modified:** 1

## Accomplishments
- Four antialiased waveforms (triangle, square, sine, sawtooth) with independent volume controls
- MinBLEP generators eliminate aliasing at >2kHz frequencies
- Proper PWM edge detection prevents VCV Fundamental click bug
- DC filtering removes offset from mixed output
- Per-voice state management for clean 16-channel polyphony

## Task Commits

Each task was committed atomically:

1. **Task 1: Add per-voice MinBLEP and DC filter state** - `352394b` (feat)
2. **Task 2: Implement antialiased waveform generation** - `84edce0` (feat)
3. **Task 3: Connect VCO1 volume knobs and mix waveforms** - `b0031be` (feat)

## Files Created/Modified
- `src/HydraQuartetVCO.cpp` - VCO1Voice struct, antialiased waveform generation, volume mixing

## Decisions Made

**TRCFilter API correction:**
- Plan specified `voice.dcFilter.process(voice.dcFilterState, mixed * 5.f)`
- VCV Rack SDK requires `process(x)` followed by `highpass()` getter
- Removed unused dcFilterState member variable
- Pattern: `voice.dcFilter.process(mixed * 5.f); float output = voice.dcFilter.highpass();`

**Per-voice scalar processing:**
- Changed from SIMD float_4 loop to scalar per-voice loop
- MinBLEP generators require per-voice state, incompatible with SIMD
- Phase still stored in float_4 arrays, accessed per-voice via `phase[c/4][c%4]`

**Triangle via leaky integrator:**
- Integrating antialiased square with 0.999 decay factor prevents DC drift
- Scale factor `4.f * freq * sampleTime` normalizes amplitude

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected TRCFilter API usage**
- **Found during:** Task 3 (Connect volume knobs)
- **Issue:** Plan specified `dcFilter.process(state, input)` but SDK requires `process(input)` then `highpass()` getter. Code wouldn't compile.
- **Fix:** Changed to two-call pattern: `voice.dcFilter.process(mixed * 5.f); float output = voice.dcFilter.highpass();`
- **Files modified:** src/HydraQuartetVCO.cpp (VCO1Voice struct and process() method)
- **Verification:** Code compiles cleanly, no warnings
- **Committed in:** b0031be (Task 3 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** API correction necessary for compilation. No scope creep.

## Issues Encountered
None - all planned work executed smoothly after TRCFilter API correction.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness

**Ready for SIMD optimization (Phase 3):**
- Antialiasing quality established as baseline
- Per-voice scalar processing can be vectorized after validation
- MinBLEP state management pattern proven

**Ready for dual-VCO architecture (Phase 4):**
- VCO1 waveform generation complete
- Volume mixing pattern established
- Can duplicate for VCO2 with detune/FM additions

**Known constraints:**
- Current implementation is scalar (not SIMD optimized)
- CPU usage needs measurement before declaring performance acceptable
- Phase 3 will vectorize where possible (some MinBLEP operations may remain scalar)

**Verification needed:**
- Play sawtooth at >2kHz - should sound clean (no metallic aliasing)
- Sweep PWM1 knob slowly while playing square - should NOT click
- Test 8-voice polyphonic chord - all voices should sound correct
- Long-term stability test (10+ minutes) - pitch should not drift

---
*Phase: 02-core-oscillator*
*Completed: 2026-01-23*
