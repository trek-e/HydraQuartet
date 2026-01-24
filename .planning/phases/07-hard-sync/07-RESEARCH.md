# Phase 7: Hard Sync - Research

**Researched:** 2026-01-23
**Domain:** Hard oscillator sync with MinBLEP antialiasing for dual VCO
**Confidence:** HIGH

## Summary

Phase 7 implements hard oscillator sync where VCO1 can reset VCO2's phase (and vice versa) at the moment the master oscillator's phase wraps from 1.0 to 0.0. Hard sync is a classic synthesizer technique that forces the slave oscillator to restart its waveform at each master cycle, creating harmonic-rich timbres that sweep in tune with the master frequency.

**Technical challenge:** Hard sync introduces discontinuities in the slave oscillator's waveform when its phase is forcibly reset mid-cycle. Each waveform (sawtooth, square, triangle, sine) experiences a different amplitude jump depending on where in its cycle the reset occurs. These discontinuities must be antialiased using MinBLEP to prevent aliasing artifacts.

**Key findings:**
- Hard sync is implemented by detecting phase wrap on the master oscillator (phase >= 1.0) and resetting the slave oscillator's phase to a subsample-accurate position
- MinBLEP discontinuities must be inserted for each waveform based on the amplitude jump: `discontinuity = newValue - oldValue` where oldValue is the waveform output just before reset
- Subsample timing is critical: the slave phase should reset to `deltaPhase * subsampleOffset` (not zero) to maintain temporal accuracy
- Bidirectional sync (mutual reset) is non-standard but musically interesting; at identical frequencies they lock together, at different frequencies complex patterns emerge
- VCV Rack Fundamental VCO uses phase wrap detection with `syncSubsample = -lastSync / deltaSync` to find exact sync timing
- Triangle waves with hard sync require both BLEP (amplitude discontinuity) and BLAMP (slope discontinuity), but square integration method handles this automatically
- Processing order matters: detect both VCOs' wraps, calculate resets, then apply MinBLEP corrections

**Primary recommendation:** Implement hard sync by detecting phase wrap on master VCO (phase >= 1.0), calculating subsample-accurate reset position, storing slave waveform values before reset, forcing phase reset to subsample position, calculating new waveform values, and inserting MinBLEP discontinuity of magnitude `(newValue - oldValue)` for each waveform. Use existing MinBlepBuffer infrastructure with per-lane insertion. For bidirectional sync, process both VCOs' old phases first, then apply cross-resets, then generate waveforms.

## Standard Stack

The established libraries/tools for hard sync in VCV Rack:

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| VCV Rack SDK | 2.6.6 | MinBLEP antialiasing, SIMD processing | Already in use, native hard sync support |
| MinBlepBuffer<32> | Custom | Per-waveform discontinuity buffers | Already implemented in Phase 2, supports per-lane insertion |
| simd::float_4 | SDK 2.x | SIMD phase and frequency processing | 4x parallel processing, existing architecture |
| simd::movemask() | SDK 2.x | Detect which lanes wrapped | Efficient bitmask for conditional MinBLEP insertion |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| simd::floor() | SDK 2.x | Phase wrapping detection | Handles large phase jumps (relevant if FM + sync combined) |
| simd::ifelse() | SDK 2.x | Conditional waveform calculation | Calculate pre-reset and post-reset values |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| MinBLEP antialiasing | Oversampling | VCV Fundamental switched FROM oversampling TO MinBLEP (lower CPU) |
| Phase wrap trigger | External sync input | Phase wrap is classic hard sync, matches analog behavior |
| Per-waveform MinBLEP | Single shared MinBLEP | Per-waveform is more accurate (different discontinuity magnitudes) |
| Unidirectional sync | Bidirectional mutual sync | Decision already made: allow bidirectional (musical chaos) |

**Installation:**
```bash
# No additional libraries needed - all in VCV Rack SDK 2.6.6
# MinBlepBuffer<32> already implemented in HydraQuartetVCO.cpp
```

## Architecture Patterns

### Recommended Project Structure
```
src/HydraQuartetVCO.cpp
├── VcoEngine (existing)
│   ├── phase[4]                     # Phase accumulators (already exists)
│   ├── sawMinBlepBuffer[4]         # Per-waveform MinBLEP (already exists)
│   ├── sqrMinBlepBuffer[4]         # Per-waveform MinBLEP (already exists)
│   └── process() signature          # MODIFY: return wrap mask, accept reset mask
└── HydraQuartetVCO::process()
    ├── Sync switch state reading    # NEW: Read SYNC1_PARAM, SYNC2_PARAM
    ├── Cross-VCO wrap detection     # NEW: Store vco1 wrapped, vco2 wrapped
    ├── Cross-VCO phase reset        # NEW: Calculate reset masks and subsample positions
    └── VcoEngine calls              # MODIFY: Pass reset parameters to process()
```

### Pattern 1: Sync Trigger Detection via Phase Wrap
**What:** Detect when master oscillator's phase wraps from >=1.0 to <1.0
**When to use:** Classic hard sync behavior (analog emulation)

**Example:**
```cpp
// Source: VCV Fundamental VCO v2 sync detection pattern
// https://github.com/VCVRack/Fundamental/blob/v2/src/VCO.cpp

// In VcoEngine::process(), already done for sawtooth MinBLEP:
float_4 oldPhase = phase[g];
phase[g] += deltaPhase;

// Detect phase wrap
float_4 wrapped = phase[g] >= 1.f;
phase[g] -= simd::floor(phase[g]);  // Wrap phase

// Extract wrap mask for conditional processing
int wrapMask = simd::movemask(wrapped);

// This wrapMask becomes the sync trigger signal for the other VCO
```

### Pattern 2: Subsample-Accurate Phase Reset
**What:** Reset slave phase to subsample-accurate position based on master's wrap timing
**When to use:** All hard sync implementations (prevents aliasing from incorrect timing)

**Example:**
```cpp
// Source: Digital hard sync subsample accuracy pattern
// https://forum.pdpatchrepo.info/topic/10192/vphasor-and-vphasor2-subsample-accurate-phasors

// Master oscillator wrapped at subsample position:
float subsample = (1.f - masterOldPhase) / masterDeltaPhase - 1.f;
// subsample is in range (-1, 0] relative to current sample

// Slave phase should reset to where it "would have been" at that subsample:
// Option A: Reset to zero (traditional, simpler)
slavePhase[g] = float_4(0.f);

// Option B: Reset to subsample-accurate position (more accurate)
slavePhase[g] = slaveDeltaPhase * (-subsample);
// Negative subsample means "how far back in time", multiply by deltaPhase
```

### Pattern 3: MinBLEP Discontinuity Insertion for Sync
**What:** Insert MinBLEP correction for amplitude jump caused by phase reset
**When to use:** All waveforms except sine when hard synced

**Example:**
```cpp
// Source: VCV Fundamental VCO sync discontinuity pattern
// Adapted from https://github.com/VCVRack/Fundamental/blob/v2/src/VCO.cpp

// For each lane that needs sync reset:
for (int i = 0; i < 4; i++) {
    if (syncResetMask & (1 << i)) {
        // Calculate old waveform value (before reset)
        float oldSaw = 2.f * slaveOldPhase[i] - 1.f;

        // Calculate new waveform value (after reset to new phase)
        float newSaw = 2.f * slaveNewPhase[i] - 1.f;

        // Discontinuity magnitude
        float discontinuity = newSaw - oldSaw;

        // Subsample position of sync event
        float subsample = (1.f - masterOldPhase[i]) / masterDeltaPhase[i] - 1.f;

        // Insert MinBLEP correction
        sawMinBlepBuffer[g].insertDiscontinuity(subsample, discontinuity, i);
    }
}
```

### Pattern 4: Per-Waveform Sync Discontinuity Calculation
**What:** Each waveform has different discontinuity magnitude at sync reset
**When to use:** Multi-waveform VCO with hard sync

**Example:**
```cpp
// Source: Hard sync antialiasing research
// http://www.cs.cmu.edu/~eli/papers/icmc01-hardsync.pdf

// Given: slaveOldPhase, slaveNewPhase, subsample position

// SAWTOOTH: Linear with phase
float oldSaw = 2.f * slaveOldPhase - 1.f;
float newSaw = 2.f * slaveNewPhase - 1.f;
sawMinBlepBuffer[g].insertDiscontinuity(subsample, newSaw - oldSaw, lane);

// SQUARE: Depends on PWM threshold
float oldSqr = (slaveOldPhase < pwm) ? 1.f : -1.f;
float newSqr = (slaveNewPhase < pwm) ? 1.f : -1.f;
sqrMinBlepBuffer[g].insertDiscontinuity(subsample, newSqr - oldSqr, lane);

// TRIANGLE: Piecewise linear
float oldTri = (slaveOldPhase < 0.5f)
    ? (4.f * slaveOldPhase - 1.f)
    : (3.f - 4.f * slaveOldPhase);
float newTri = (slaveNewPhase < 0.5f)
    ? (4.f * slaveNewPhase - 1.f)
    : (3.f - 4.f * slaveNewPhase);
// Triangle also has slope discontinuity - handled by square integration method

// SINE: No discontinuity needed (continuous waveform)
// Sine hard sync is mathematically continuous, no MinBLEP needed
```

### Pattern 5: Bidirectional Sync Processing Order
**What:** Process both VCOs' phases before applying cross-resets to avoid race condition
**When to use:** Mutual sync capability (both VCOs can reset each other)

**Example:**
```cpp
// Source: Phase 7 CONTEXT.md - Processing Order section

// STEP 1: Store both VCOs' old phases (before increment)
float_4 vco1OldPhase = vco1.phase[g];
float_4 vco2OldPhase = vco2.phase[g];

// STEP 2: Increment both phases
vco1.phase[g] += vco1DeltaPhase;
vco2.phase[g] += vco2DeltaPhase;

// STEP 3: Detect wrap events
float_4 vco1Wrapped = vco1.phase[g] >= 1.f;
float_4 vco2Wrapped = vco2.phase[g] >= 1.f;

// STEP 4: Wrap phases
vco1.phase[g] -= simd::floor(vco1.phase[g]);
vco2.phase[g] -= simd::floor(vco2.phase[g]);

// STEP 5: Apply cross-resets based on sync switches
if (sync1Enabled) {
    // VCO1 syncs to VCO2 (VCO2 is master)
    vco1.phase[g] = simd::ifelse(vco2Wrapped, 0.f, vco1.phase[g]);
}
if (sync2Enabled) {
    // VCO2 syncs to VCO1 (VCO1 is master)
    vco2.phase[g] = simd::ifelse(vco1Wrapped, 0.f, vco2.phase[g]);
}

// STEP 6: Generate waveforms with MinBLEP corrections
// (Discontinuities inserted based on old vs new phase differences)
```

### Pattern 6: VcoEngine Signature Modification for Sync
**What:** Modify VcoEngine::process() to accept external sync signals and return wrap status
**When to use:** Hard sync implementation in dual-VCO architecture

**Example:**
```cpp
// Current signature (Phase 2-6):
void process(int g, float_4 freq, float sampleTime, float_4 pwm,
             float_4& saw, float_4& sqr, float_4& tri, float_4& sine);

// Proposed signature for Phase 7:
void process(int g, float_4 freq, float sampleTime, float_4 pwm,
             float_4& saw, float_4& sqr, float_4& tri, float_4& sine,
             float_4 syncReset = 0.f,        // NEW: external sync trigger
             float_4* wrappedOut = nullptr);  // NEW: output wrap mask

// Alternative: Keep signature, use separate sync() method:
void process(/* existing params */);  // Normal processing
int getWrapMask(int g);               // Returns wrap mask from last process
void applySync(int g, float_4 resetMask, float_4 masterOldPhase,
               float_4 masterDeltaPhase);  // Apply external sync
```

### Anti-Patterns to Avoid

- **Resetting phase to exactly zero without subsample offset:** Creates systematic phase error, causes audible beating or inharmonicity. Use subsample-accurate position.

- **Inserting sync discontinuity before phase wrapping:** Causes incorrect MinBLEP timing. Always wrap phase first, then calculate discontinuities based on wrapped phase.

- **Using single MinBLEP buffer for all waveforms:** Different waveforms have different discontinuity magnitudes at sync. Per-waveform buffers are more accurate.

- **Forgetting to handle triangle slope discontinuity:** Triangle has both amplitude AND slope discontinuity at sync point. Use square integration method (existing implementation) which handles this automatically.

- **Not clamping subsample position to (-1, 0] range:** MinBLEP expects subsample in this range. Out-of-range values cause artifacts or silence.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Sync trigger detection | External CV input with schmitt trigger | Phase wrap detection (phase >= 1.0) | Classic analog hard sync uses phase reset, not gate |
| Subsample position calculation | Linear interpolation formula | VCV Fundamental pattern: `(1.f - oldPhase) / deltaPhase - 1.f` | Handles edge cases, proven in production |
| Per-waveform discontinuity | Manual switch/case per waveform | Systematic calculation: `newValue - oldValue` | Generalizes to any waveform shape |
| Bidirectional sync safety guards | Phase locking detection and disabling | Allow mutual sync (musical chaos) | Decision already made: bidirectional allowed |
| BLAMP for triangle slopes | Custom derivative discontinuity tracking | Square integration method (existing) | Already implemented, handles slopes automatically |

**Key insight:** Hard sync is well-studied in digital synthesis literature. The MinBLEP method (insert discontinuity for amplitude jump) is standard. Subsample accuracy is critical for clean sound. VCV Fundamental's approach is battle-tested and should be followed closely.

## Common Pitfalls

### Pitfall 1: Incorrect Subsample Position Calculation
**What goes wrong:** Sync sounds "clicky" or has audible aliasing despite MinBLEP
**Why it happens:** Subsample position is off-by-one or calculated from wrong reference point
**How to avoid:**
```cpp
// WRONG: Using current phase
float subsample = phase[i] / deltaPhase[i];  // Incorrect reference

// CORRECT: Using old phase (before wrap)
float subsample = (1.f - oldPhase[i]) / deltaPhase[i] - 1.f;
// This gives position in range (-1, 0] where wrap occurred
```
**Warning signs:** Spectrum analyzer shows high-frequency content even with MinBLEP, sync sweep sounds harsh

### Pitfall 2: Race Condition in Bidirectional Sync
**What goes wrong:** At certain frequency ratios, oscillators become silent or produce glitches
**Why it happens:** VCO1's reset affects VCO2's phase before VCO2's wrap is detected, or vice versa
**How to avoid:**
```cpp
// WRONG: Process VCO1, then process VCO2 with sync
vco1.process(g, freq1, ...);
if (vco1Wrapped) vco2.applySync(...);  // Too late! VCO2 already processed
vco2.process(g, freq2, ...);

// CORRECT: Detect both wraps first, THEN apply resets
float_4 vco1OldPhase = vco1.phase[g];
float_4 vco2OldPhase = vco2.phase[g];
// Increment both
// Detect both wraps
// Apply cross-resets
// Generate waveforms
```
**Warning signs:** Bidirectional sync produces silence or stuttering at specific frequency ratios

### Pitfall 3: Forgetting PWM Interaction with Sync
**What goes wrong:** Square wave sync produces unexpected output or missing discontinuities
**Why it happens:** Square wave has TWO discontinuities per cycle (rising/falling edge). Sync can occur before, between, or after these edges.
**How to avoid:**
```cpp
// When sync resets phase:
// 1. Check if falling edge occurred BEFORE sync in this sample
if (oldPhase < pwm && syncPhase >= pwm) {
    sqrMinBlepBuffer[g].insertDiscontinuity(fallingSubsample, -2.f, lane);
}

// 2. Calculate square value at sync point
float oldSqr = (oldPhase < pwm) ? 1.f : -1.f;
float newSqr = (syncPhase < pwm) ? 1.f : -1.f;

// 3. Insert sync discontinuity
if (oldSqr != newSqr) {
    sqrMinBlepBuffer[g].insertDiscontinuity(syncSubsample, newSqr - oldSqr, lane);
}
```
**Warning signs:** Square wave sync sounds different from sawtooth sync at same settings

### Pitfall 4: Sync Discontinuity Magnitude Sign Error
**What goes wrong:** Sync produces inverted or doubled amplitude artifacts
**Why it happens:** Discontinuity magnitude is `oldValue - newValue` instead of `newValue - oldValue`
**How to avoid:**
```cpp
// WRONG: Inverted sign
float discontinuity = oldSaw - newSaw;  // Backwards

// CORRECT: New minus old
float discontinuity = newSaw - oldSaw;  // Correct direction

// Verification: If oldSaw = 0.5 and newSaw = -0.8 (reset to beginning),
// discontinuity = -0.8 - 0.5 = -1.3 (negative jump, insert -1.3 BLEP)
```
**Warning signs:** Sync sweep sounds "inside-out" or produces +10dB peaks instead of smooth timbre change

### Pitfall 5: Not Handling Zero or Negative Frequency with Sync
**What goes wrong:** Crashes, NaN, or unexpected behavior when FM drives frequency negative
**Why it happens:** Sync detection assumes positive deltaPhase; negative phase increment breaks wrap detection
**How to avoid:**
```cpp
// Through-zero FM (Phase 6) can make freq negative
// Hard sync should only trigger on POSITIVE phase progression

if (deltaPhase[i] > 0.f && oldPhase[i] < 1.f && phase[i] >= 1.f) {
    // Valid forward wrap
    float subsample = (1.f - oldPhase[i]) / deltaPhase[i] - 1.f;
    // Insert sync...
}

// For negative deltaPhase, phase decrements and wraps backwards
// Classic hard sync doesn't apply (no analog equivalent)
```
**Warning signs:** FM + sync combination produces clicks, silence, or NaN when FM depth is high

### Pitfall 6: Sine Wave Sync Discontinuity Insertion
**What goes wrong:** Sine wave with hard sync sounds buzzy or aliased
**Why it happens:** Incorrectly inserting MinBLEP discontinuity for sine (which is continuous, not discontinuous)
**How to avoid:**
```cpp
// Sine wave is mathematically continuous at sync point
// The waveform smoothly changes from sin(oldPhase) to sin(newPhase)
// NO step discontinuity exists

// WRONG: Insert BLEP for sine
float oldSine = sin(2*pi*oldPhase);
float newSine = sin(2*pi*newPhase);
sineMinBlepBuffer[g].insertDiscontinuity(subsample, newSine - oldSine, lane);

// CORRECT: No MinBLEP for sine hard sync
// Just reset phase, sine is continuous
// (Advanced: Sine has infinite-order discontinuity, special antialiasing needed)
```
**Warning signs:** Sine sync sounds harsh or clicky, spectrum shows high-frequency aliases

Reference: [A General Antialiasing Method for Sine Hard Sync (2022)](https://dafx.de/paper-archive/2022/papers/DAFx20in22_paper_3.pdf) notes that sine hard sync is significantly more difficult and requires specialized methods beyond MinBLEP.

## Code Examples

Verified patterns from official sources:

### Complete Hard Sync Implementation (Unidirectional)
```cpp
// Source: VCV Fundamental VCO sync pattern
// https://github.com/VCVRack/Fundamental/blob/v2/src/VCO.cpp

// In HydraQuartetVCO::process(), SIMD loop:

// Read sync switches
bool sync1Enabled = params[SYNC1_PARAM].getValue() > 0.5f;  // VCO1 syncs to VCO2
bool sync2Enabled = params[SYNC2_PARAM].getValue() > 0.5f;  // VCO2 syncs to VCO1

// Store old phases
float_4 vco1OldPhase = vco1.phase[g];
float_4 vco2OldPhase = vco2.phase[g];

// Process VCO1 (modulator, with detune)
float_4 vco1DeltaPhase = simd::clamp(freq1 * sampleTime, 0.f, 0.49f);
vco1.phase[g] += vco1DeltaPhase;
float_4 vco1Wrapped = vco1.phase[g] >= 1.f;
vco1.phase[g] -= simd::floor(vco1.phase[g]);

// Process VCO2 (carrier, with FM)
float_4 vco2DeltaPhase = simd::clamp(freq2 * sampleTime, 0.f, 0.49f);
vco2.phase[g] += vco2DeltaPhase;
float_4 vco2Wrapped = vco2.phase[g] >= 1.f;
vco2.phase[g] -= simd::floor(vco2.phase[g]);

// Apply sync resets (before generating waveforms)
if (sync2Enabled) {
    // VCO2 syncs to VCO1: when VCO1 wraps, reset VCO2 phase
    int vco1WrapMask = simd::movemask(vco1Wrapped);
    if (vco1WrapMask) {
        for (int i = 0; i < 4; i++) {
            if (vco1WrapMask & (1 << i)) {
                // Calculate subsample position of VCO1 wrap
                float subsample = (1.f - vco1OldPhase[i]) / vco1DeltaPhase[i] - 1.f;

                // Calculate VCO2 waveform values before reset
                float oldPhase2 = vco2OldPhase[i] + vco2DeltaPhase[i];  // Before wrap
                float oldSaw2 = 2.f * oldPhase2 - 1.f;

                // Reset VCO2 phase (subsample-accurate)
                vco2.phase[g][i] = vco2DeltaPhase[i] * (-subsample);

                // Calculate new waveform values after reset
                float newSaw2 = 2.f * vco2.phase[g][i] - 1.f;

                // Insert discontinuity for sawtooth
                vco2.sawMinBlepBuffer[g].insertDiscontinuity(
                    subsample, newSaw2 - oldSaw2, i);

                // Repeat for square, triangle (if not using square integration)
            }
        }
    }
}

// Generate waveforms (existing VcoEngine::process pattern)
vco1.process(g, freq1, sampleTime, pwm1_4, saw1, sqr1, tri1, sine1);
vco2.process(g, freq2, sampleTime, pwm2_4, saw2, sqr2, tri2, sine2);
```

### Bidirectional Sync with Mutual Reset
```cpp
// Source: Phase 7 CONTEXT.md - Bidirectional sync decision

// Both sync switches can be enabled simultaneously
bool sync1Enabled = params[SYNC1_PARAM].getValue() > 0.5f;
bool sync2Enabled = params[SYNC2_PARAM].getValue() > 0.5f;

// Store old phases before any modifications
float_4 vco1OldPhase = vco1.phase[g];
float_4 vco2OldPhase = vco2.phase[g];

// Increment and wrap both
vco1.phase[g] += vco1DeltaPhase;
vco2.phase[g] += vco2DeltaPhase;
float_4 vco1Wrapped = vco1.phase[g] >= 1.f;
float_4 vco2Wrapped = vco2.phase[g] >= 1.f;
vco1.phase[g] -= simd::floor(vco1.phase[g]);
vco2.phase[g] -= simd::floor(vco2.phase[g]);

// Apply BOTH sync resets (order doesn't matter since both phases are saved)
if (sync1Enabled && sync2Enabled) {
    // Mutual sync: both can reset each other
    // At identical frequencies: phase lock
    // At different frequencies: complex chaotic patterns

    int vco1WrapMask = simd::movemask(vco1Wrapped);
    int vco2WrapMask = simd::movemask(vco2Wrapped);

    // VCO2 resets VCO1
    // ... (insert discontinuities, reset VCO1 phase)

    // VCO1 resets VCO2
    // ... (insert discontinuities, reset VCO2 phase)

    // If BOTH wrap in same sample: both reset to ~0, phase lock
}
```

### Square Wave Sync with PWM Interaction
```cpp
// Square wave has falling edge at PWM threshold
// Sync can occur before, at, or after falling edge

float oldPhase = vco2OldPhase[i] + vco2DeltaPhase[i];  // Phase before wrap
float pwm = pwm2_4[i];

// Check for falling edge BEFORE sync point
if (vco2OldPhase[i] < pwm && oldPhase >= pwm) {
    float fallingSubsample = (pwm - vco2OldPhase[i]) / vco2DeltaPhase[i] - 1.f;
    vco2.sqrMinBlepBuffer[g].insertDiscontinuity(fallingSubsample, -2.f, i);
}

// Calculate square values at sync point
float oldSqr = (oldPhase < pwm) ? 1.f : -1.f;

// After sync reset
float newPhase = 0.f;  // Or subsample-accurate position
float newSqr = (newPhase < pwm) ? 1.f : -1.f;

// Insert sync discontinuity (may be zero if both above/below PWM)
if (oldSqr != newSqr) {
    vco2.sqrMinBlepBuffer[g].insertDiscontinuity(syncSubsample, newSqr - oldSqr, i);
}
```

### Testing Hard Sync (Verification)
```cpp
// Source: Requirement SYNC-03 verification

// Test case 1: Classic sync sweep (VCO2 syncs to VCO1)
// - Set SYNC2_PARAM = 1 (Hard sync enabled)
// - VCO1 at C4 (261.63 Hz), VCO2 starts at C4
// - Slowly increase VCO2 frequency (OCTAVE2_PARAM from 0 to +2)
// Expected: Pitch stays at C4 (VCO1 frequency), timbre becomes brighter
//           Harmonic series extends with each octave increase

// Test case 2: Reverse sync (VCO1 syncs to VCO2)
// - Set SYNC1_PARAM = 1, SYNC2_PARAM = 0
// - VCO2 at C4, sweep VCO1 frequency
// Expected: Pitch stays at C4 (VCO2 frequency), VCO1 timbre changes

// Test case 3: Mutual sync at unison
// - Set SYNC1_PARAM = 1, SYNC2_PARAM = 1
// - Both at C4, small detune on VCO1 (DETUNE1_PARAM = 0.1)
// Expected: Oscillators phase-lock, minimal beating, stable tone

// Test case 4: Mutual sync at different octaves
// - SYNC1_PARAM = 1, SYNC2_PARAM = 1
// - VCO1 at C4, VCO2 at C5 (OCTAVE2_PARAM = +1)
// Expected: Complex pattern, VCO2 resets VCO1 twice per VCO1 cycle
//           Chaotic but musically interesting timbre

// Spectrum analyzer verification (SYNC-05)
// - Use Bogaudio Analyzer-XL on synced output
// - Sweep slave frequency from unison to +2 octaves
// Expected: Clean harmonic series, no inharmonic aliases
//           Harmonics should be integer multiples of master frequency
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Naive sync (no antialiasing) | MinBLEP sync discontinuities | ~2001 (Eli Brandt paper) | Clean sync sweeps without aliasing |
| External sync CV input | Phase wrap detection | Classic analog behavior | Matches Prophet-5, Minimoog sync topology |
| Zero-reset only | Subsample-accurate reset | Digital synthesis best practices | Eliminates systematic phase error |
| Shared MinBLEP buffer | Per-waveform buffers | Production VCV modules | Different waveforms = different discontinuities |
| Unidirectional sync only | Bidirectional/mutual sync | Modular synthesizers (~2010s) | Creative sound design possibilities |

**Deprecated/outdated:**
- **Naive sync without antialiasing:** Produces severe aliasing. MinBLEP is standard.
- **External sync CV input:** Adds latency and requires zero-crossing detection. Phase wrap is more accurate.
- **BLAMP for all waveforms:** Triangle via square integration handles slope discontinuities automatically. No need for explicit BLAMP.
- **Sine hard sync with MinBLEP:** Sine is continuous at sync point (no step), requires specialized methods beyond scope.

## Open Questions

Things that couldn't be fully resolved:

1. **Sine hard sync antialiasing**
   - What we know: Sine wave hard sync is continuous (no step discontinuity), but has infinite-order discontinuity in derivatives. MinBLEP does not apply. Research paper (2022) proposes FIR lowpass filtering method.
   - What's unclear: Whether to implement specialized sine sync antialiasing or accept aliasing on sine output when synced
   - Recommendation: Skip sine antialiasing for Phase 7. Sine hard sync is rarely used (geometric waveforms are more typical for sync sweeps). If needed, document as known limitation. Advanced implementation could use FIR filtering from research paper.

2. **Optimal subsample reset position**
   - What we know: Two options: (A) reset to zero, (B) reset to `deltaPhase * (-subsample)` (subsample-accurate)
   - What's unclear: Audible difference between the two approaches, CPU cost of subsample calculation
   - Recommendation: Use subsample-accurate reset (option B) for maximum accuracy. Calculation is cheap (one multiply per synced lane). VCV Fundamental pattern suggests this is standard.

3. **Mutual sync instability**
   - What we know: Bidirectional sync can create phase locking (stable) or chaotic patterns (unstable) depending on frequency ratio
   - What's unclear: Whether to add safety guards (e.g., disable if both wrap in same sample) or embrace chaos as feature
   - Recommendation: Embrace chaos (already decided in CONTEXT.md). Mutual sync is experimental/creative feature. Document behavior in success criteria. If users report crashes, add guards in Phase 8+.

4. **FM + Sync interaction**
   - What we know: Phase 6 implements through-zero FM which can make frequency negative. Phase 7 adds sync. Negative frequency breaks sync wrap detection.
   - What's unclear: Should sync be disabled when frequency is negative? Or only trigger on positive wraps?
   - Recommendation: Only trigger sync on positive frequency wraps. Check `deltaPhase[i] > 0.f` before calculating subsample. Document that sync doesn't apply to negative frequencies (no analog equivalent). This prevents crashes while allowing FM + sync combination for normal FM depths.

5. **Triangle sync with square integration method**
   - What we know: Existing implementation uses square integration for triangle (Pattern 3 from Phase 2). Square has MinBLEP antialiasing. Integration preserves band-limiting.
   - What's unclear: Whether sync discontinuity needs special handling for integrated triangle, or if square's sync handling is sufficient
   - Recommendation: No special handling needed. When square resets due to sync, its MinBLEP discontinuity is inserted. Integration of that discontinuity automatically produces correct triangle sync. Verify with spectrum analyzer during testing.

## Sources

### Primary (HIGH confidence)
- [VCV Rack MinBlepGenerator API](https://vcvrack.com/docs-v2/structrack_1_1dsp_1_1MinBlepGenerator) - insertDiscontinuity() method, subsample range (-1, 0]
- [VCV Fundamental VCO.cpp v2](https://github.com/VCVRack/Fundamental/blob/v2/src/VCO.cpp) - Sync detection pattern with subsample calculation
- Existing HydraQuartetVCO.cpp - VcoEngine architecture, MinBlepBuffer<32> with per-lane insertion
- Phase 2 RESEARCH.md - MinBLEP patterns, subsample calculation, PWM edge detection
- Phase 6 RESEARCH.md - Through-zero FM interaction with phase wrap
- Phase 7 CONTEXT.md - Decisions: phase wrap trigger, bidirectional sync, per-waveform MinBLEP

### Secondary (MEDIUM confidence)
- [Hard Sync Without Aliasing (Eli Brandt, 2001)](http://www.cs.cmu.edu/~eli/papers/icmc01-hardsync.pdf) - Foundational paper on MinBLEP for hard sync
- [VCV Community: Hard Sync on XFX Wave](https://community.vcvrack.com/t/hard-sync-on-xfx-wave/20798) - VCV hard sync bug fixes (negative slope trigger, aliasing)
- [Pure Data Forum: Subsample-accurate phasors](https://forum.pdpatchrepo.info/topic/10192/vphasor-and-vphasor2-subsample-accurate-phasors) - Subsample reset calculation pattern
- [KVR: Hard Sync - Some Thoughts](https://www.kvraudio.com/forum/viewtopic.php?t=192829) - Subsample timing and discontinuity insertion
- [KVR: Triangle hard sync with BLEPs](https://www.kvraudio.com/forum/viewtopic.php?t=492537) - BLEP + BLAMP for triangle slope discontinuities
- [Wikipedia: Oscillator sync](https://en.wikipedia.org/wiki/Oscillator_sync) - Hard sync definition (master resets slave)

### Tertiary (LOW confidence)
- [A General Antialiasing Method for Sine Hard Sync (2022)](https://dafx.de/paper-archive/2022/papers/DAFx20in22_paper_3.pdf) - Sine hard sync requires specialized methods beyond MinBLEP
- [KVR: About BLEP minBLEP - polyBLEP](https://www.kvraudio.com/forum/viewtopic.php?t=248390) - BLAMP for derivative discontinuities
- [ModWiggler: waveshapes for oscillator sync](https://modwiggler.com/forum/viewtopic.php?t=244781) - Master-slave topology discussion
- [Learning Modular: Sync](https://learningmodular.com/glossary/sync/) - Hard sync behavior description

## Metadata

**Confidence breakdown:**
- Sync trigger detection (phase wrap): HIGH - Verified in VCV Fundamental source, standard analog behavior
- Subsample timing calculation: HIGH - Pattern extracted from Fundamental and confirmed in multiple sources
- MinBLEP discontinuity insertion: HIGH - Existing infrastructure (MinBlepBuffer<32>), proven pattern
- Per-waveform discontinuity calculation: MEDIUM - Formula is straightforward (new - old), but sine case is unclear
- Bidirectional sync behavior: MEDIUM - Non-standard feature, no reference implementation found, but mathematically sound

**Research date:** 2026-01-23
**Valid until:** 2026-02-23 (30 days - hard sync is mature technique, implementations stable)

**Critical recommendation for planner:**
The existing VcoEngine uses MinBLEP antialiasing for sawtooth and square, and triangle via square integration. This architecture is well-suited for hard sync. The key implementation challenge is modifying the VcoEngine signature to accept external sync signals while preserving the existing MinBLEP infrastructure. Recommend refactoring VcoEngine::process() to return wrap mask and accept optional sync reset mask, then implementing sync logic in HydraQuartetVCO::process() at the SIMD loop level. Sine hard sync is mathematically continuous and does not require MinBLEP, but specialized antialiasing methods exist if needed (deferred to future phase).
