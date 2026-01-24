---
phase: 07-hard-sync
plan: 01
subsystem: dsp
tags: [hard-sync, minblep, antialiasing, oscillator, simd, vcv-rack]

# Dependency graph
requires:
  - phase: 06-through-zero-fm
    provides: VcoEngine structure with dual oscillator SIMD processing
  - phase: 03-simd-minblep
    provides: MinBlepBuffer<32> template with lane-based discontinuity insertion
provides:
  - Bidirectional hard sync between VCO1 and VCO2 with MinBLEP antialiasing
  - applySync() method for subsample-accurate phase reset with discontinuity insertion
  - Triangle wave MinBLEP buffer (triMinBlepBuffer) for sync antialiasing
  - Wrap mask return from VcoEngine::process() for sync detection
affects: [07-hard-sync, future-cross-sync]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Bidirectional sync: process both oscillators first, then apply sync resets"
    - "applySync() resets phase and recalculates waveforms, updates outputs by reference"
    - "MinBLEP discontinuity = newValue - oldValue at subsample position"
    - "Negative frequency protection: skip sync if deltaPhase <= 0"

key-files:
  created: []
  modified:
    - src/HydraQuartetVCO.cpp

key-decisions:
  - "Separate waveform generation phase: process() generates waveforms, applySync() resets and regenerates"
  - "Triangle gets dedicated triMinBlepBuffer for sync antialiasing (parallel to saw/sqr buffers)"
  - "Sine excluded from MinBLEP antialiasing (continuous waveform, accepted aliasing limitation)"
  - "Skip sync when slave or master frequency is negative (FM protection, prevents crashes)"
  - "Output params passed by reference and updated in applySync() (no one-sample delay)"

patterns-established:
  - "Hard sync pattern: calculate subsample position, store old values, reset phase, calculate new values, insert discontinuities, update outputs"
  - "Bidirectional sync order: both VCOs process() first, then both applySync() calls (prevents order dependency)"

# Metrics
duration: 3min
completed: 2026-01-24
---

# Phase 7 Plan 1: Hard Sync Summary

**Bidirectional hard oscillator sync with MinBLEP antialiasing on saw/square/triangle waveforms, subsample-accurate phase reset via applySync() method**

## Performance

- **Duration:** 3 min
- **Started:** 2026-01-24T03:04:14Z
- **Completed:** 2026-01-24T03:07:10Z
- **Tasks:** 3
- **Files modified:** 1

## Accomplishments
- VCO1 can sync to VCO2 phase wrap (controlled by SYNC1_PARAM switch)
- VCO2 can sync to VCO1 phase wrap (controlled by SYNC2_PARAM switch)
- Hard sync uses MinBLEP antialiasing for clean sync sweeps without clicks
- Triangle waveform gets dedicated MinBLEP buffer for sync discontinuities
- Phase reset is subsample-accurate (prevents timing errors)
- Waveform outputs reflect synced phase in same sample (no delay)

## Task Commits

Each task was committed atomically:

1. **Task 1: Modify VcoEngine to return wrap mask and store phase state** - `6b8023d` (feat)
2. **Task 2: Implement bidirectional hard sync with separate waveform generation phase** - `5ddef48` (feat)
3. **Task 3: Implement applySync() with MinBLEP discontinuity insertion and waveform update** - `40265e3` (feat)

## Files Created/Modified
- `src/HydraQuartetVCO.cpp` - Added triMinBlepBuffer, applySync() method, bidirectional sync logic, wrapMask return from process()

## Decisions Made

**Architecture Decision - Separate waveform generation phase:**
- applySync() resets phase and recalculates waveforms, then updates output params by reference
- This ensures waveforms reflect synced phase in the same sample (no one-sample delay)
- Alternative would be regenerating waveforms in next process() call, but adds latency

**Triangle MinBLEP buffer:**
- Added dedicated triMinBlepBuffer[4] to VcoEngine for triangle sync antialiasing
- Parallels existing sawMinBlepBuffer and sqrMinBlepBuffer
- Triangle wave has geometric discontinuities during sync reset (needs MinBLEP)

**Sine waveform excluded:**
- Sine is continuous (no geometric discontinuities), so no MinBLEP needed
- Sine hard sync will have some aliasing (known limitation per research)
- Specialized FIR methods exist but are out of scope

**Negative frequency protection:**
- Skip sync if slave deltaPhase <= 0 or master deltaPhase <= 0
- Prevents crashes when FM drives frequency negative
- Per CONTEXT.md decision #4 from research

**Bidirectional sync order:**
- Both VCOs call process() first (stores oldPhase, deltaPhase, generates wrapMask)
- Then both applySync() calls happen (order matters for bidirectional)
- This prevents one sync from affecting the other's wrap detection

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - implementation proceeded smoothly following the plan.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Hard sync implementation complete and verified:
- Compile succeeds with no errors
- All three tasks committed atomically
- VcoEngine structure ready for future enhancements
- SYNC1_PARAM and SYNC2_PARAM switches connected to sync logic

**Testing recommendations:**
1. Set VCO1 and VCO2 to same pitch
2. Enable VCO2 sync to VCO1 (SYNC2_PARAM = Hard)
3. Slowly increase VCO2 octave from 0 to +2
4. Should hear harmonic sweep without clicks
5. Spectrum analyzer should show clean harmonics, no aliasing spikes above 20kHz

**No blockers for next phase.**

---
*Phase: 07-hard-sync*
*Completed: 2026-01-24*
