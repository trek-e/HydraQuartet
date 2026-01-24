---
phase: 08-xor-waveshaping
plan: 01
subsystem: dsp
tags: [ring-modulation, xor, minblep, antialiasing, simd]

# Dependency graph
requires:
  - phase: 03-dual-vco
    provides: VcoEngine struct with MinBLEP buffers and SIMD processing
  - phase: 02-waveforms-antialiasing
    provides: MinBLEP antialiasing infrastructure and patterns
provides:
  - XOR ring modulation output (sqr1 * sqr2)
  - Full MinBLEP antialiasing for XOR with 4 edge types tracked
  - xorMinBlepBuffer in VcoEngine for VCO2 edge tracking
  - xorFromVco1MinBlep module-level buffer for VCO1 edge tracking
affects: [08-02-xor-output-mixing, future-waveshaping-features]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Ring modulation via square wave multiplication (sqr1 * sqr2)"
    - "Dual MinBLEP tracking: VcoEngine buffer + module-level buffer"
    - "XOR discontinuity magnitude is 2 * other_sqr (edge change is ±2)"

key-files:
  created: []
  modified:
    - src/HydraQuartetVCO.cpp

key-decisions:
  - "XOR calculated as raw multiplication (sqr1 * sqr2) with MinBLEP antialiasing"
  - "Split edge tracking: VCO2 edges in VcoEngine, VCO1 edges at module level"
  - "VcoEngine::process() extended with optional sqr1Input and xorOut parameters"
  - "Variable named 'xorOut' not 'xor' (xor is C++ reserved keyword)"

patterns-established:
  - "Optional output pattern: nullptr check enables XOR generation only when needed"
  - "Module-level MinBLEP buffers for cross-oscillator interactions"
  - "Four edge types tracked: VCO1 wrap, VCO1 PWM fall, VCO2 wrap, VCO2 PWM fall"

# Metrics
duration: 4min
completed: 2026-01-23
---

# Phase 08 Plan 01: XOR Waveshaping DSP Summary

**XOR ring modulation (sqr1 * sqr2) with full MinBLEP antialiasing tracking all 4 edge types from both oscillators**

## Performance

- **Duration:** 3m 56s
- **Started:** 2026-01-23T15:24:04Z
- **Completed:** 2026-01-23T15:28:00Z
- **Tasks:** 3/3
- **Files modified:** 1

## Accomplishments
- XOR output calculated via ring modulation (square wave multiplication)
- Full MinBLEP antialiasing with 4 edge types tracked (VCO1 wrap/fall, VCO2 wrap/fall)
- VcoEngine extended with xorMinBlepBuffer and optional XOR output parameters
- Module-level xorFromVco1MinBlep buffer for VCO1 edge tracking
- XOR discontinuity magnitude correctly calculated as 2 * other_sqr

## Task Commits

Each task was committed atomically:

1. **Task 1: Add xorMinBlepBuffer and modify VcoEngine signature** - `863dc74` (feat)
2. **Task 2: Implement XOR calculation with MinBLEP tracking** - `638fa2a` (feat)
3. **Task 3: Track XOR edges from VCO1 in main process loop** - `e00aab5` (feat)

## Files Created/Modified
- `src/HydraQuartetVCO.cpp` - Added XOR ring modulation DSP with full MinBLEP antialiasing

## Decisions Made

1. **XOR calculation pattern: sqr1 * sqr2**
   - Raw multiplication produces ring modulation effect
   - When VCOs at same pitch: DC-coupled waveform (changes with PWM)
   - When detuned: classic ring modulation timbres

2. **Split MinBLEP tracking architecture**
   - VCO2 edges tracked in VcoEngine::xorMinBlepBuffer (local to VCO2)
   - VCO1 edges tracked in module-level xorFromVco1MinBlep buffer
   - Rationale: VCO2 process() generates XOR, but needs sqr2 values for VCO1 edge tracking (dependency order)

3. **Optional XOR output via nullptr check**
   - VcoEngine::process() accepts xorOut pointer, defaults to nullptr
   - VCO1 call: passes nullptr (no XOR calculation)
   - VCO2 call: passes &xorOut (enables XOR generation)
   - Clean pattern for optional waveform generation

4. **Discontinuity magnitude: 2 * other_sqr**
   - When sqr1 transitions: XOR changes by 2 * sqr2 (sqr1 changes by ±2)
   - When sqr2 transitions: XOR changes by 2 * sqr1 (sqr2 changes by ±2)
   - Correctly accounts for square wave amplitude (±1 range)

5. **Variable naming: xorOut not xor**
   - 'xor' is C++ reserved keyword (bitwise XOR operator)
   - Renamed to 'xorOut' to avoid compilation errors

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Renamed variable from 'xor' to 'xorOut'**
- **Found during:** Task 3 (compilation)
- **Issue:** 'xor' is reserved C++ keyword, caused compilation errors
- **Fix:** Renamed all instances to 'xorOut'
- **Files modified:** src/HydraQuartetVCO.cpp
- **Verification:** Code compiles cleanly
- **Committed in:** e00aab5 (part of Task 3 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Essential for compilation. No scope creep.

## Issues Encountered

None - plan executed smoothly after variable rename.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

**Ready for Phase 08 Plan 02 (XOR Output Mixing):**
- XOR waveform (xorOut) generated and antialiased
- Available in main process loop for mixing with other waveforms
- xorVol_4 variable already exists (from 08-02 pre-work) for volume control

**Technical context for next plan:**
- XOR output is float_4 type (SIMD-compatible)
- Needs to be added to mixer alongside other waveforms (tri1, sqr1, saw1, tri2, sqr2, saw2, sub)
- Already has CV input infrastructure (XOR_CV_INPUT) for future volume CV control

---
*Phase: 08-xor-waveshaping*
*Completed: 2026-01-23*
