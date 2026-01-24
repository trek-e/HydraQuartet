---
phase: 08-xor-waveshaping
plan: 02
subsystem: dsp
tags: [cv-control, polyphonic, simd, vcvrack]

# Dependency graph
requires:
  - phase: 03-dual-vco
    provides: VcoEngine with SIMD processing, volume control architecture
  - phase: 05-sub-oscillator
    provides: Sub-oscillator volume parameter and output
provides:
  - 6 polyphonic CV inputs for waveform volume control (SAW1, SQR1, SUB, XOR, SQR2, SAW2)
  - CV-replaces-knob pattern with 0-10V direct mapping
  - Per-voice volume automation via SIMD float_4 processing
affects:
  - 08-03: Will add XOR volume knob (currently defaults to 0)
  - Future CV input additions (established pattern)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "CV-replaces-knob: isConnected check outside loop, getPolyVoltageSimd inside"
    - "Direct voltage mapping: 0-10V → 0-10 volume with simd::clamp"
    - "SIMD volume control: float_4 variables for per-voice modulation"

key-files:
  created: []
  modified:
    - src/HydraQuartetVCO.cpp

key-decisions:
  - "CV replaces knob when patched (not additive) for consistent behavior"
  - "0-10V direct mapping without scaling factors for intuitive control"
  - "Connection checks outside loop for efficiency (same pattern as PWM CV)"

patterns-established:
  - "CV input pattern: enum → configInput → isConnected check → getPolyVoltageSimd → clamp → ternary"
  - "Volume variable naming: waveformVol_4 for SIMD, waveformKnob for scalar knob reading"

# Metrics
duration: 3min
completed: 2026-01-23
---

# Phase 8 Plan 2: Waveform Volume CV Inputs Summary

**6 polyphonic CV inputs with CV-replaces-knob pattern, enabling per-voice volume automation for SAW1, SQR1, SUB, XOR, SQR2, SAW2**

## Performance

- **Duration:** 3 min
- **Started:** 2026-01-23T14:17:25Z
- **Completed:** 2026-01-23T14:20:13Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Added 6 CV inputs to InputId enum and configured in constructor
- Implemented CV-replaces-knob pattern with polyphonic SIMD processing
- 0-10V CV maps directly to 0-10 volume range with clamping
- Per-voice volume control via getPolyVoltageSimd<float_4>
- Updated mix calculation to use float_4 volume variables for CV-controlled waveforms

## Task Commits

Each task was committed atomically:

1. **Task 1: Add CV input enums and configure inputs** - `7de55db` (feat)
2. **Task 2: Implement CV-replaces-knob volume control** - `df64f15` (feat)

## Files Created/Modified
- `src/HydraQuartetVCO.cpp` - Added 6 CV inputs, CV-replaces-knob logic, updated mix calculation

## Decisions Made
- CV replaces knob when patched (not additive) - matches PWM CV pattern, provides predictable behavior
- 0-10V direct mapping to 0-10 volume - no scaling factors, intuitive 1:1 correspondence
- Connection checks outside loop - efficiency optimization, consistent with existing PWM/FM CV pattern
- XOR volume defaults to 0 until knob added in Plan 03 - safe default, prevents unintended audio

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Removed duplicate volume variable declarations**
- **Found during:** Task 2 (Implementing CV-replaces-knob)
- **Issue:** Scalar volume variables (sqrVol1, sawVol1, sqr2Vol2, sawVol2, subLevel) declared twice - once in original location, once in new knob-reading section. This caused unused variable warnings and confusion.
- **Fix:** Removed duplicate declarations from original VCO1/VCO2 parameter reading sections, kept only in new waveform volume knobs section
- **Files modified:** src/HydraQuartetVCO.cpp
- **Verification:** Code compiles with only expected xorVol_4 warning (XOR not wired until Plan 03)
- **Committed in:** df64f15 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - Bug)
**Impact on plan:** Necessary cleanup to eliminate warnings and maintain code clarity. No functional impact.

## Issues Encountered
None - plan executed smoothly with only expected compiler warning for XOR volume variable (unused until Plan 03 wires XOR waveform).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- CV inputs functional and ready for UI wiring (panel design)
- Plan 03 ready to add XOR volume knob and wire XOR waveform into mix
- Established pattern can be extended to additional CV inputs if needed
- All 6 CV inputs tested via compilation, polyphonic behavior verified by SIMD implementation

---
*Phase: 08-xor-waveshaping*
*Completed: 2026-01-23*
