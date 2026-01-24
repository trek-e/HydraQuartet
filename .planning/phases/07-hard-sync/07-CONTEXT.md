# Phase 7 Context: Hard Sync

**Created:** 2026-01-24
**Phase Goal:** Cross-sync between oscillators with proper antialiasing

---

## Decisions Made

### 1. Sync Trigger Detection
**Decision:** Phase wrap (1.0 → 0.0)

Trigger sync reset when master oscillator's phase wraps from 1 to 0. This is classic hard sync behavior and integrates naturally with our existing phase accumulator architecture. The wrap detection already exists for MinBLEP insertion on sawtooth.

**Implementation:** Reuse the `wrapped` mask already computed in VcoEngine::process().

### 2. Sync Antialiasing Strategy
**Decision:** MinBLEP at sync point

Insert MinBLEP discontinuity for the amplitude jump caused by phase reset. When the slave oscillator's phase is forcibly reset, each waveform experiences a step discontinuity. We'll calculate the amplitude difference before/after reset and insert appropriate MinBLEP correction.

**Implementation details:**
- For sawtooth: Calculate `oldValue = 2*oldPhase - 1`, `newValue = 2*newPhase - 1`, insert MinBLEP for `(newValue - oldValue)`
- For square: Calculate based on PWM threshold crossing
- Sine and triangle: Calculate from phase, insert appropriate discontinuity

### 3. Bidirectional Sync Behavior
**Decision:** Mutual sync allowed (both reset each other)

When both sync switches are enabled, both VCOs can reset each other. This creates interesting phase-locked or chaotic behavior depending on frequency ratio. At identical frequencies, they'll lock together. At different frequencies, complex patterns emerge.

**Caution:** This could create instability at certain ratios. Accept this as musical behavior rather than adding safety guards.

### 4. Sync vs FM Interaction
**Decision:** Independent operation

Sync and FM operate on actual running frequencies. If VCO2 is being FM'd by VCO1, the sync detection still uses VCO1's actual phase progression (which is unaffected by FM since VCO1 is the modulator, not modulated).

### 5. Per-Waveform Discontinuity Handling
**Decision:** Individual MinBLEP per waveform

Each waveform has its own MinBLEP buffer. When sync resets phase:
1. Store old waveform values (before reset)
2. Reset phase to fractional position based on subsample timing
3. Calculate new waveform values
4. Insert MinBLEP discontinuity = (new - old) for each waveform

This is the most accurate approach and leverages our existing per-waveform MinBLEP infrastructure.

---

## Current State

**Already implemented:**
- SYNC1_PARAM: Switch (Off/Hard) at line 152, configured at line 233
- SYNC2_PARAM: Switch (Off/Hard) at line 164, configured at line 246
- Widget controls for both switches (lines 512, 543)

**Needs implementation:**
- Sync trigger detection in process loop
- Phase reset logic in VcoEngine
- MinBLEP insertion for sync discontinuities
- Cross-VCO state access (VCO1 needs to see VCO2's phase wrap and vice versa)

---

## Technical Notes

### VcoEngine Modification
The current VcoEngine::process() is self-contained. For sync, we need to:
1. Return whether phase wrapped (bool or mask)
2. Accept external "force reset" signal
3. Handle subsample-accurate phase reset

### Subsample Timing
When VCO1's phase wraps at subsample position `p`, VCO2's phase should reset at that same subsample position. The fractional position calculation:
```cpp
float subsample = (1.f - masterOldPhase) / masterDeltaPhase - 1.f;
// Reset slave phase considering subsample offset
slaveNewPhase = slaveDeltaPhase * (-subsample);  // Amount slave would have advanced
```

### Processing Order
Since sync is bidirectional, we need both VCOs' old phases before processing either. Suggested approach:
1. Store both VCOs' current phases (pre-increment)
2. Detect wrap events on both
3. Calculate new phases for both (with sync resets)
4. Generate waveforms with MinBLEP

---

*Context gathered: 2026-01-24*
