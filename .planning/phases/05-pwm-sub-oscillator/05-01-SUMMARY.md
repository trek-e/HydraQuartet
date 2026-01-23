---
phase: 05-pwm-sub-oscillator
plan: 01
subsystem: dsp
tags: [pwm, cv-input, attenuverter, led, simd, vcv-rack]

# Dependency graph
requires:
  - phase: 04-dual-vco-architecture
    provides: VcoEngine struct, dual VCO processing, PWM knobs
provides:
  - PWM CV inputs with polyphonic processing for both VCOs
  - Bipolar attenuverters for modulation depth and polarity control
  - LED activity indicators showing CV modulation
  - PWM clamping to 0.01-0.99 range for DC safety
affects: [06-through-zero-fm, 08-xor-waveshaping]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Bipolar attenuverter CV processing (knob + CV * att * scale)
    - LED brightness from peak CV detection
    - Per-channel PWM modulation via SIMD

key-files:
  created: []
  modified:
    - src/HydraQuartetVCO.cpp

key-decisions:
  - "PWM CV scaled at 0.1 factor: +/-5V * att gives +/-0.5 PWM contribution (full sweep)"
  - "PWM clamped to 0.01-0.99 to avoid DC offset at extremes"
  - "LED brightness based on peak CV across all channels"
  - "Trimpot used for attenuverters (smaller than main knobs)"

patterns-established:
  - "Bipolar CV with attenuverter: base + cv * att * scale, clamped to safe range"
  - "CV activity LED: peak detect across polyphonic channels, brightness = peak / 5V"

# Metrics
duration: 2.5min
completed: 2026-01-23
---

# Phase 5 Plan 1: PWM CV Inputs Summary

**Bipolar PWM CV modulation with attenuverters for both VCOs, polyphonic processing, and LED activity indicators**

## Performance

- **Duration:** 2.5 min
- **Started:** 2026-01-23T11:57:20Z
- **Completed:** 2026-01-23T11:59:53Z
- **Tasks:** 3
- **Files modified:** 1

## Accomplishments

- PWM CV inputs (PWM1_INPUT, PWM2_INPUT) now read polyphonic CV and modulate pulse width
- Bipolar attenuverters (PWM1_ATT_PARAM, PWM2_ATT_PARAM) control modulation depth and polarity
- LED activity indicators (PWM1_CV_LIGHT, PWM2_CV_LIGHT) show when CV modulation is active
- PWM clamped to 0.01-0.99 range to prevent DC offset at extremes (as per Phase 2 DC filtering design)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add PWM CV attenuverter params and wire CV inputs** - `b4b40d8` (feat)
2. **Task 2: Add attenuverter knobs and LED lights to widget** - `a158b14` (feat)
3. **Task 3: Test PWM CV modulation with LFO** - No commit (verification task, no code changes)

## Files Created/Modified

- `src/HydraQuartetVCO.cpp` - Added PWM1_ATT_PARAM, PWM2_ATT_PARAM, PWM1_CV_LIGHT, PWM2_CV_LIGHT; wired CV inputs with polyphonic processing and attenuverter scaling; added Trimpot widgets and SmallLight LED indicators

## Decisions Made

- **PWM CV scaling factor 0.1:** With +/-5V CV range and 0.1 factor, full attenuverter gives +/-0.5 PWM contribution - this allows sweeping the full 1-99% range from knob center (50%)
- **Trimpot for attenuverters:** Smaller than main knobs, appropriate visual hierarchy for secondary controls
- **SmallLight for LED indicators:** Standard VCV Rack pattern for activity indicators near ports
- **Peak detection for LED brightness:** Takes max absolute value across all polyphonic channels for consistent visual feedback regardless of voice count

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - implementation followed VCV Rack patterns and existing VcoEngine design.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- PWM CV modulation complete for both VCOs
- Ready for Plan 2: Sub-oscillator implementation
- PWM values are now per-channel float_4 (accessible for future XOR waveshaping in Phase 8)

---
*Phase: 05-pwm-sub-oscillator*
*Completed: 2026-01-23*
