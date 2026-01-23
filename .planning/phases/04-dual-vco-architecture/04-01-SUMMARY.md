---
phase: 04-dual-vco-architecture
plan: 01
subsystem: dsp
tags: [vco, simd, dual-oscillator, detune, octave-switch]

# Dependency graph
requires:
  - phase: 03-simd-polyphony
    provides: SIMD float_4 processing with MinBLEP antialiasing
provides:
  - VcoEngine struct encapsulating oscillator state
  - Dual oscillator architecture (vco1, vco2)
  - Independent pitch control (octave switches)
  - VCO1 detune for thickness/beating effect
  - Independent waveform mixing per VCO
affects: [05-tzfm, 06-waveshaping, 07-sync]

# Tech tracking
tech-stack:
  added: []
  patterns: [vco-engine-struct, dual-vco-mixing]

key-files:
  created: []
  modified: [src/HydraQuartetVCO.cpp]

key-decisions:
  - "VcoEngine struct extracts all per-oscillator state (phase, MinBLEP buffers, triState)"
  - "VCO1 receives detune, VCO2 is reference (0 detune = perfect unison)"
  - "Detune range 0-50 cents via V/Oct conversion (50/1200 volts max)"
  - "DC filters remain module members, not in VcoEngine (operate on mixed output)"

patterns-established:
  - "VcoEngine::process() signature: (g, freq, sampleTime, pwm, &saw, &sqr, &tri, &sine)"
  - "Dual VCO mixing: sum both VCO outputs with independent volume controls"

# Metrics
duration: 15min
completed: 2026-01-23
---

# Phase 4 Plan 01: Dual VCO Architecture Summary

**VcoEngine struct extracted from inline DSP, dual oscillators with independent octave switches and detune control for thickness/beating effects**

## Performance

- **Duration:** 15 min
- **Started:** 2026-01-23
- **Completed:** 2026-01-23
- **Tasks:** 3 (2 auto + 1 checkpoint)
- **Files modified:** 1

## Accomplishments
- Extracted VcoEngine struct encapsulating phase, MinBLEP buffers, and triangle state
- Dual VCO instances (vco1, vco2) with independent SIMD state
- Octave switches (-2 to +2) for both VCOs
- Detune knob on VCO1 (0-50 cents) creating beating/thickness against VCO2 reference
- Independent waveform volume controls for all 8 outputs (4 per VCO)

## Task Commits

Each task was committed atomically:

1. **Task 1: Extract VcoEngine struct and instantiate dual oscillators** - `9dcb7fb` (refactor)
2. **Task 2: Wire pitch controls (octave + detune) and VCO2 output** - `9f2e23b` (feat)
3. **Task 3: Human verification checkpoint** - approved (no commit needed)

## Files Created/Modified
- `src/HydraQuartetVCO.cpp` - VcoEngine struct, dual vco1/vco2 instances, pitch control wiring, dual mixing

## Decisions Made
- **VcoEngine boundary:** Encapsulates oscillator-specific state (phase, MinBLEP, triState). DC filters stay at module level since they operate on mixed output.
- **Detune on VCO1:** VCO1 gets detune so VCO2 acts as reference. At 0 detune = perfect unison.
- **Detune range:** 0-50 cents converted to V/Oct (0.0417V max) for musically useful thickness without going out of tune.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Dual VCO architecture complete and verified
- Ready for Phase 5: Through-Zero FM (VCO1 modulating VCO2 frequency)
- FM will use vco1 output to modulate vco2.freq in real-time
- Sync will reset vco2.phase when vco1 wraps

---
*Phase: 04-dual-vco-architecture*
*Completed: 2026-01-23*
