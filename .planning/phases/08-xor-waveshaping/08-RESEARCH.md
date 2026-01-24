# Phase 8: XOR Waveshaping - Research

**Researched:** 2026-01-23
**Domain:** Ring modulation via square wave multiplication, polyphonic CV inputs for waveform volume, soft clipping saturation
**Confidence:** HIGH

## Summary

Phase 8 implements the final v1 feature set: XOR output via ring modulation (multiplying VCO1 and VCO2 square waves), 6 polyphonic CV inputs for waveform volumes, and soft clipping for output headroom management. The "XOR" is actually analog-style ring modulation (sqr1 * sqr2) rather than digital XOR, producing classic ring modulator timbres with full MinBLEP antialiasing.

**Technical approach:** Ring modulation of two square waves produces a square wave at the sum and difference frequencies. When both inputs are MinBLEP-antialiased, the multiplication operation itself doesn't introduce new aliasing (the result stays within Nyquist), but the output still needs MinBLEP tracking because the XOR transitions occur whenever EITHER VCO1 OR VCO2 transitions. This requires a dedicated xorMinBlepBuffer that tracks edges from both oscillators.

**Key findings:**
- Ring modulation (sqr1 * sqr2) produces output transitions whenever either input transitions, requiring MinBLEP tracking of both oscillators' edges
- XOR becomes VCO2's fifth waveform, mixed with tri2/sqr2/sin2/saw2 and controlled by volume knob
- Polyphonic CV inputs use the same getPolyVoltageSimd() pattern as PWM CV, with 0-10V direct voltage mapping to 0-10 volume
- CV replaces knob when patched (not additive), standard VCV Rack pattern for consistent behavior
- Soft clipping prevents harsh digital clipping when many waveforms sum; tanh() is the eurorack-standard approach
- DC filtering already exists on mixed output and handles XOR DC content when oscillators lock

**Primary recommendation:** Add xorMinBlepBuffer to VcoEngine, track sqr1 and sqr2 transitions to insert XOR discontinuities, pass sqr1 to VCO2 engine via method parameter, implement 6 CV inputs with direct voltage-to-volume mapping (CV replaces knob), apply tanh() soft clipping to output, and expand panel to 40HP to accommodate new CV jacks.

## Standard Stack

The established libraries/tools for this phase:

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| VCV Rack SDK | 2.6.6 | MinBLEP, SIMD, polyphonic CV | Already in use, native support for all requirements |
| MinBlepBuffer<32> | Custom | XOR discontinuity tracking | Proven pattern from phases 2-7, supports per-lane insertion |
| simd::float_4 | SDK 2.x | SIMD processing | 4x parallel, existing architecture |
| std::tanh() | C++ std | Soft clipping saturation | Industry standard, eurorack-proven |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| dsp::TRCFilter | SDK 2.x | DC blocking on XOR | Already applied to mixed output, handles XOR DC |
| simd::clamp() | SDK 2.x | Clamp CV to valid range | Before voltage-to-volume mapping |
| getPolyVoltageSimd() | SDK 2.x | Read polyphonic CV | All 6 waveform CV inputs |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| tanh() soft clip | Cubic soft clip | tanh is eurorack standard, smoother saturation curve |
| CV replaces knob | CV + knob additive | Replaces is VCV standard, more predictable |
| Pass sqr1 as parameter | Separate processXOR() method | Parameter is simpler, less architectural churn |
| XOR MinBLEP in VCO2 | Module-level buffer | VCO2 owns XOR output, cleaner encapsulation |

**Installation:**
```bash
# No additional libraries needed - all in VCV Rack SDK 2.6.6
# std::tanh() is C++ standard library
```

## Architecture Patterns

### Recommended Project Structure
```
src/HydraQuartetVCO.cpp
├── VcoEngine (existing)
│   ├── sawMinBlepBuffer[4]         # Already exists
│   ├── sqrMinBlepBuffer[4]         # Already exists
│   ├── triMinBlepBuffer[4]         # Already exists
│   ├── xorMinBlepBuffer[4]         # NEW: XOR discontinuity tracking
│   ├── process() signature          # MODIFY: add sqr1Input parameter, xor output
│   └── trackXorEdge()               # NEW: Insert XOR MinBLEP on sqr1/sqr2 transitions
└── HydraQuartetVCO::process()
    ├── Waveform CV reading          # NEW: 6 polyphonic CV inputs
    ├── Voltage-to-volume mapping    # NEW: 0-10V → 0-10 volume (CV replaces knob)
    ├── VCO1/VCO2 processing         # MODIFY: pass sqr1 to VCO2 for XOR
    ├── XOR mixing                   # NEW: xor * xorVol in VCO2 mix
    ├── Soft clipping                # NEW: tanh() on mixed output
    └── Panel expansion              # MODIFY: Widget positions for 40HP
```

### Pattern 1: Ring Modulation via Square Wave Multiplication
**What:** Multiply two square waves to produce ring modulation effect
**When to use:** XOR/ring modulator output in dual VCO

**Example:**
```cpp
// Source: Analog ring modulation principle
// https://en.wikipedia.org/wiki/Ring_modulation

// Both square waves are ±1 (MinBLEP-antialiased)
// Multiplication gives ring modulation:
float_4 xor = sqr1 * sqr2;

// Result is also ±1:
// +1 * +1 = +1  (both high)
// +1 * -1 = -1  (one high, one low)
// -1 * +1 = -1  (one low, one high)
// -1 * -1 = +1  (both low)

// This is equivalent to XOR logic but in analog domain
```

### Pattern 2: XOR MinBLEP Tracking
**What:** Track edges from BOTH oscillators to antialias XOR output
**When to use:** Ring modulation of two antialiased signals

**Example:**
```cpp
// Source: Existing MinBlepBuffer pattern from Phase 2
// Adapted for dual-input tracking

// XOR transitions occur when EITHER input transitions
// Track sqr1 edges (detected in VCO1):
if (vco1WrapMask) {  // sqr1 rising edge on wrap
    for (int i = 0; i < 4; i++) {
        if ((vco1WrapMask & (1 << i)) && vco1.deltaPhase[g][i] > 0.f) {
            float subsample = (1.f - vco1.oldPhase[g][i]) / vco1.deltaPhase[g][i] - 1.f;
            // XOR discontinuity = 2 * sqr2 value (sqr1 flips from -1 to +1)
            float xorDisc = 2.f * sqr2[i];
            xorMinBlepBuffer[g].insertDiscontinuity(subsample, xorDisc, i);
        }
    }
}

// Track sqr1 falling edges (PWM threshold crossing):
if (vco1FallMask) {  // sqr1 falling edge
    for (int i = 0; i < 4; i++) {
        if ((vco1FallMask & (1 << i)) && vco1.deltaPhase[g][i] > 0.f) {
            float subsample = (pwm1[i] - vco1.oldPhase[g][i]) / vco1.deltaPhase[g][i] - 1.f;
            // XOR discontinuity = -2 * sqr2 value (sqr1 flips from +1 to -1)
            float xorDisc = -2.f * sqr2[i];
            xorMinBlepBuffer[g].insertDiscontinuity(subsample, xorDisc, i);
        }
    }
}

// Track sqr2 edges (same pattern for VCO2 transitions):
// ... repeat for vco2WrapMask and vco2FallMask

// Final XOR output:
float_4 xor = sqr1 * sqr2 + xorMinBlepBuffer[g].process();
```

### Pattern 3: Polyphonic CV Input for Waveform Volume
**What:** Read polyphonic CV, map 0-10V to 0-10 volume, CV replaces knob
**When to use:** All 6 waveform CV inputs (SAW1, SQR1, SUB, XOR, SQR2, SAW2)

**Example:**
```cpp
// Source: VCV Rack polyphonic CV standard + Context decision
// https://vcvrack.com/manual/Polyphony

// Outside SIMD loop - read knob defaults
float saw1Vol = params[SAW1_PARAM].getValue();  // 0-10
float sqr1Vol = params[SQR1_PARAM].getValue();  // 0-10
// ... etc

// Inside SIMD loop - read polyphonic CV
for (int c = 0; c < channels; c += 4) {
    int g = c / 4;

    // Read polyphonic CV (auto-broadcasts if mono input to poly VCO)
    float_4 saw1CV = inputs[SAW1_CV_INPUT].getPolyVoltageSimd<float_4>(c);

    // Check if CV is connected (per-voice check)
    bool saw1CVConnected = inputs[SAW1_CV_INPUT].isConnected();

    // CV replaces knob when patched (not additive)
    float_4 saw1Volume;
    if (saw1CVConnected) {
        // Map 0-10V to 0-10 volume (direct 1:1)
        saw1Volume = simd::clamp(saw1CV, 0.f, 10.f);
    } else {
        // Use knob value
        saw1Volume = float_4(saw1Vol);
    }

    // Apply to waveform mix
    float_4 vco1Mix = saw1 * saw1Volume + sqr1 * sqr1Volume + ...;
}
```

### Pattern 4: Soft Clipping with tanh()
**What:** Apply hyperbolic tangent saturation to prevent harsh clipping
**When to use:** Output stage when many waveforms can sum to exceed ±5V range

**Example:**
```cpp
// Source: Eurorack soft clipping standard
// https://www.instruomodular.com/product/tanh/
// https://ccrma.stanford.edu/~jos/pasp/Soft_Clipping.html

// After mixing all waveforms
float_4 mixed = (tri1 * tri1Vol + sqr1 * sqr1Vol + ... + xor * xorVol) * outputScale;

// Apply soft clipping per-voice
for (int i = 0; i < 4; i++) {
    // tanh() saturates to ±1 for large inputs, smooth for small
    // Scale input for desired saturation threshold
    // For ±5V eurorack: divide by 3 before tanh, multiply by 3 after
    mixed[i] = 3.f * std::tanh(mixed[i] / 3.f);

    // Alternative: tanh without scaling (saturates at ±1V)
    // mixed[i] = std::tanh(mixed[i]);
}

// tanh characteristics:
// - Input ±1V → output ±0.76V (gentle compression)
// - Input ±3V → output ±0.995V (strong compression)
// - Input ±5V → output ±0.9999V (hard limit)
// - Smooth, no harsh edges
```

### Pattern 5: Pass Square Wave Between VCO Engines
**What:** VCO2 needs sqr1 from VCO1 to calculate XOR
**When to use:** Cross-VCO waveform interactions

**Example:**
```cpp
// Source: Existing applySync() pattern from Phase 7
// Modified for continuous parameter passing

// OPTION A: Pass as process() parameter (recommended - simpler)
struct VcoEngine {
    void process(int g, float_4 freq, float sampleTime, float_4 pwm,
                 float_4& saw, float_4& sqr, float_4& tri, float_4& sine,
                 int& wrapMask,
                 float_4 sqr1Input = float_4(0.f),  // NEW: optional sqr1 input
                 float_4* xor = nullptr) {          // NEW: optional XOR output

        // ... existing process logic

        // If xor output requested
        if (xor != nullptr) {
            // Calculate XOR as sqr1Input * sqr
            *xor = sqr1Input * sqr + xorMinBlepBuffer[g].process();

            // Track XOR edges from this VCO's square transitions
            // (sqr1Input edges tracked separately in VCO1)
        }
    }
};

// In main process():
float_4 saw1, sqr1, tri1, sine1, xor;
int vco1WrapMask, vco2WrapMask;

vco1.process(g, freq1, sampleTime, pwm1,
             saw1, sqr1, tri1, sine1, vco1WrapMask);

vco2.process(g, freq2, sampleTime, pwm2,
             saw2, sqr2, tri2, sine2, vco2WrapMask,
             sqr1,      // Pass sqr1 to VCO2
             &xor);     // Request XOR output

// OPTION B: Separate processXOR() method (more code, clearer separation)
// xor = vco2.processXOR(g, sqr1, sqr2);
```

### Anti-Patterns to Avoid

- **Not tracking XOR edges from both VCOs:** XOR transitions occur when either input changes. Missing edges from either VCO causes aliasing.

- **Additive CV + knob:** VCV Rack standard is CV replaces knob when patched, not CV + knob. Additive leads to unexpected volume stacking.

- **Hard clipping at ±5V:** Using `clamp(mixed, -5.f, 5.f)` creates harsh digital clipping. Always use soft clipping (tanh) for musical saturation.

- **Shared MinBLEP buffer for XOR and sqr2:** XOR has different discontinuities than sqr2. They need separate buffers for correct antialiasing.

- **XOR without DC filter:** When oscillators are at same frequency or locked by sync, XOR can produce DC. DC filter on mixed output handles this.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Polyphonic CV reading | Manual channel loop | getPolyVoltageSimd<float_4>() | Handles mono→poly broadcast, SIMD-optimized |
| Soft clipping algorithm | Custom polynomial | std::tanh() | Industry standard, proven smooth saturation |
| DC blocking | Manual high-pass | dsp::TRCFilter (existing) | Already applied to mixed output |
| MinBLEP insertion | Custom convolution | MinBlepBuffer (existing) | Per-lane insertion, proven in phases 2-7 |
| Edge detection | Manual phase comparison | simd::movemask() + existing patterns | SIMD-optimized, already used for sync |

**Key insight:** All infrastructure exists from previous phases. Phase 8 is primarily wiring: connect CV inputs, add XOR calculation to existing MinBLEP machinery, apply tanh() to output. No new DSP primitives needed.

## Common Pitfalls

### Pitfall 1: Missing XOR Edges from One VCO
**What goes wrong:** XOR output has aliasing artifacts, especially at high frequencies
**Why it happens:** Only tracking sqr2 transitions, forgetting that sqr1 transitions also change XOR
**How to avoid:**
```cpp
// Track edges from BOTH oscillators
// VCO1 edges (wrap and PWM fall):
if (vco1WrapMask) { /* insert XOR discontinuity */ }
if (vco1FallMask) { /* insert XOR discontinuity */ }

// VCO2 edges (wrap and PWM fall):
if (vco2WrapMask) { /* insert XOR discontinuity */ }
if (vco2FallMask) { /* insert XOR discontinuity */ }
```
**Warning signs:** Metallic aliasing in XOR output, especially when oscillators at different frequencies

### Pitfall 2: XOR Discontinuity Magnitude Errors
**What goes wrong:** XOR MinBLEP overcorrects or undercorrects, causing ripples or persistent aliasing
**Why it happens:** Incorrect calculation of discontinuity magnitude
**How to avoid:**
```cpp
// When sqr1 transitions (e.g., rising edge +1 → -1):
// XOR before: sqr1_old * sqr2 = (-1) * sqr2
// XOR after:  sqr1_new * sqr2 = (+1) * sqr2
// Discontinuity: (+1) * sqr2 - (-1) * sqr2 = 2 * sqr2

// General formula:
float xorDisc = (sqr1_new - sqr1_old) * sqr2;

// For rising edge (sqr1: -1 → +1):
float xorDisc = 2.f * sqr2[i];

// For falling edge (sqr1: +1 → -1):
float xorDisc = -2.f * sqr2[i];
```
**Warning signs:** XOR waveform has visible MinBLEP ripples in oscilloscope

### Pitfall 3: CV Voltage Range Mismatch
**What goes wrong:** CV inputs don't produce expected volume changes
**Why it happens:** Expecting ±5V bipolar CV, but design uses 0-10V unipolar
**How to avoid:**
```cpp
// Context decision: 0-10V = 0-10 volume (direct 1:1 mapping)
// NOT bipolar ±5V like PWM CV

// WRONG:
float_4 volume = 5.f + saw1CV * 0.5f;  // Bipolar mapping

// CORRECT:
float_4 volume = simd::clamp(saw1CV, 0.f, 10.f);  // Direct unipolar
```
**Warning signs:** User reports "5V CV produces half volume, not full volume"

### Pitfall 4: Soft Clipping Too Aggressive
**What goes wrong:** Output sounds squashed, loses dynamics
**Why it happens:** tanh() applied at wrong scale (saturates too early)
**How to avoid:**
```cpp
// Scale determines saturation threshold
// Eurorack audio is ±5V, but typical signal is ±2-3V

// Too aggressive (saturates at ±1V):
mixed[i] = std::tanh(mixed[i]);  // BAD

// Appropriate (saturates at ±3V):
mixed[i] = 3.f * std::tanh(mixed[i] / 3.f);  // GOOD

// Alternative: scale based on active waveform count
float scale = 2.f + activeWaveformCount * 0.5f;
mixed[i] = scale * std::tanh(mixed[i] / scale);
```
**Warning signs:** Output sounds compressed even with one waveform active

### Pitfall 5: XOR DC Coupling Issues
**What goes wrong:** XOR output drifts to DC when oscillators lock (same frequency)
**Why it happens:** sqr1 * sqr2 = constant when both oscillators in phase
**How to avoid:**
```cpp
// DC filter already exists on mixed output (Phase 2)
// Ensure XOR is included in mixed signal:
float_4 mixed = (...waveforms... + xor * xorVol) * outputScale;

// DC filter processes mixed output:
dcFilters[c + i].setCutoffFreq(10.f / sampleRate);
dcFilters[c + i].process(mixed[i]);
float out = dcFilters[c + i].highpass();

// DC content removed automatically
```
**Warning signs:** XOR output has DC offset when both VCOs at same pitch

### Pitfall 6: Panel Overcrowding
**What goes wrong:** 6 new CV jacks don't fit on 36HP panel
**Why it happens:** Underestimating space requirements
**How to avoid:**
```cpp
// Context decision: expand to 40HP if needed
// CV jacks positioned below corresponding knobs:

// VCO1 side (left):
// SAW1_CV below SAW1 knob (mm2px(Vec(55.88, 58.0)))
// SQR1_CV below SQR1 knob (mm2px(Vec(25.4, 58.0)))
// SUB_CV  below SUB knob  (mm2px(Vec(10.16, 98.0)))

// VCO2 side (right):
// XOR_CV  below XOR knob  (new position)
// SQR2_CV below SQR2 knob (mm2px(Vec(142.24, 58.0)))
// SAW2_CV below SAW2 knob (mm2px(Vec(172.72, 58.0)))

// Spacing: minimum 13mm between jacks (PJ301MPort = 9mm diameter)
```
**Warning signs:** SVG panel shows overlapping components

## Code Examples

Verified patterns from official sources and existing codebase:

### Complete XOR Generation with MinBLEP
```cpp
// Source: Ring modulation + existing MinBlepBuffer pattern

// In VcoEngine::process(), after sqr calculation:
sqr = simd::ifelse(phase[g] < pwm, 1.f, -1.f) + sqrMinBlepBuffer[g].process();

// Calculate XOR (if sqr1Input provided)
float_4 xor = sqr1Input * sqr;  // Raw multiplication

// Track XOR edges from THIS oscillator's square transitions
// (sqr1Input edges tracked separately in VCO1's VcoEngine)

// Falling edge detection (this VCO's PWM crossing)
float_4 fallingEdge = (oldPhase[g] < pwm) & (phase[g] >= pwm);
int fallMask = simd::movemask(fallingEdge);
if (fallMask) {
    for (int i = 0; i < 4; i++) {
        if ((fallMask & (1 << i)) && deltaPhase[g][i] > 0.f) {
            float subsample = (pwm[i] - oldPhase[g][i]) / deltaPhase[g][i] - 1.f;
            // XOR discontinuity: sqr transitions from +1 to -1
            // XOR changes by -2 * sqr1Input[i]
            float xorDisc = -2.f * sqr1Input[i];
            xorMinBlepBuffer[g].insertDiscontinuity(subsample, xorDisc, i);
        }
    }
}

// Rising edge on wrap
if (wrapMask) {
    for (int i = 0; i < 4; i++) {
        if ((wrapMask & (1 << i)) && deltaPhase[g][i] > 0.f) {
            float subsample = (1.f - oldPhase[g][i]) / deltaPhase[g][i] - 1.f;
            // XOR discontinuity: sqr transitions from -1 to +1
            // XOR changes by +2 * sqr1Input[i]
            float xorDisc = 2.f * sqr1Input[i];
            xorMinBlepBuffer[g].insertDiscontinuity(subsample, xorDisc, i);
        }
    }
}

// Apply MinBLEP correction
xor += xorMinBlepBuffer[g].process();
```

### Polyphonic CV Input Implementation
```cpp
// Source: VCV Rack polyphonic patterns + Context decision
// https://vcvrack.com/manual/Polyphony

// Add CV inputs to enum InputId:
enum InputId {
    // ... existing inputs
    SAW1_CV_INPUT,
    SQR1_CV_INPUT,
    SUB_CV_INPUT,
    XOR_CV_INPUT,
    SQR2_CV_INPUT,
    SAW2_CV_INPUT,
    INPUTS_LEN
};

// In constructor:
configInput(SAW1_CV_INPUT, "SAW1 Volume CV");
configInput(SQR1_CV_INPUT, "SQR1 Volume CV");
// ... etc

// In process(), before SIMD loop:
float saw1Knob = params[SAW1_PARAM].getValue();  // 0-10
float sqr1Knob = params[SQR1_PARAM].getValue();  // 0-10
// ... read all 6 knobs

bool saw1CVConnected = inputs[SAW1_CV_INPUT].isConnected();
bool sqr1CVConnected = inputs[SQR1_CV_INPUT].isConnected();
// ... check all 6 CV inputs

// In SIMD loop:
for (int c = 0; c < channels; c += 4) {
    int g = c / 4;

    // Read polyphonic CV for all 6 inputs
    float_4 saw1CV = inputs[SAW1_CV_INPUT].getPolyVoltageSimd<float_4>(c);
    float_4 sqr1CV = inputs[SQR1_CV_INPUT].getPolyVoltageSimd<float_4>(c);
    float_4 subCV  = inputs[SUB_CV_INPUT].getPolyVoltageSimd<float_4>(c);
    float_4 xorCV  = inputs[XOR_CV_INPUT].getPolyVoltageSimd<float_4>(c);
    float_4 sqr2CV = inputs[SQR2_CV_INPUT].getPolyVoltageSimd<float_4>(c);
    float_4 saw2CV = inputs[SAW2_CV_INPUT].getPolyVoltageSimd<float_4>(c);

    // CV replaces knob (not additive)
    float_4 saw1Vol = saw1CVConnected ? simd::clamp(saw1CV, 0.f, 10.f) : float_4(saw1Knob);
    float_4 sqr1Vol = sqr1CVConnected ? simd::clamp(sqr1CV, 0.f, 10.f) : float_4(sqr1Knob);
    float_4 subVol  = subCVConnected  ? simd::clamp(subCV,  0.f, 10.f) : float_4(subKnob);
    float_4 xorVol  = xorCVConnected  ? simd::clamp(xorCV,  0.f, 10.f) : float_4(xorKnob);
    float_4 sqr2Vol = sqr2CVConnected ? simd::clamp(sqr2CV, 0.f, 10.f) : float_4(sqr2Knob);
    float_4 saw2Vol = saw2CVConnected ? simd::clamp(saw2CV, 0.f, 10.f) : float_4(saw2Knob);

    // Apply to waveform mix
    float_4 mixed = (tri1 * tri1Vol + sqr1 * sqr1Vol + sine1 * sine1Vol + saw1 * saw1Vol + sub * subVol
                  +  tri2 * tri2Vol + sqr2 * sqr2Vol + sine2 * sine2Vol + saw2 * saw2Vol + xor * xorVol)
                  * outputScale;
}
```

### Soft Clipping Application
```cpp
// Source: Eurorack tanh saturation pattern
// https://www.instruomodular.com/product/tanh/

// After DC filtering, before output:
for (int i = 0; i < groupChannels; i++) {
    // DC filter (existing)
    dcFilters[c + i].setCutoffFreq(10.f / sampleRate);
    dcFilters[c + i].process(mixed[i]);
    float dcFiltered = dcFilters[c + i].highpass();

    // Soft clipping with tanh
    // Scale factor: 3.0 for ±3V saturation threshold
    float softClipped = 3.f * std::tanh(dcFiltered / 3.f);

    // Apply output scaling (±2V for testing, ±5V for production)
    float out = softClipped * 2.f;

    // Sanitize (existing pattern)
    mixed[i] = std::isfinite(out) ? out : 0.f;
}
```

### VcoEngine Modified Signature
```cpp
// Source: Existing VcoEngine + XOR requirements

struct VcoEngine {
    float_4 phase[4] = {};
    float_4 oldPhase[4] = {};
    float_4 deltaPhase[4] = {};
    MinBlepBuffer<32> sawMinBlepBuffer[4];
    MinBlepBuffer<32> sqrMinBlepBuffer[4];
    MinBlepBuffer<32> triMinBlepBuffer[4];
    MinBlepBuffer<32> xorMinBlepBuffer[4];  // NEW

    // Modified process() signature:
    // - Added sqr1Input parameter (default 0 for VCO1, sqr1 value for VCO2)
    // - Added xor output parameter (nullptr for VCO1, &xor for VCO2)
    void process(int g, float_4 freq, float sampleTime, float_4 pwm,
                 float_4& saw, float_4& sqr, float_4& tri, float_4& sine,
                 int& wrapMask,
                 float_4 sqr1Input = float_4(0.f),  // NEW
                 float_4* xor = nullptr) {          // NEW

        // ... existing waveform generation

        // XOR calculation (only if requested)
        if (xor != nullptr) {
            // Raw ring modulation
            *xor = sqr1Input * sqr;

            // Track XOR edges from sqr2 transitions (sqr1 edges tracked in VCO1)
            // ... insert MinBLEP discontinuities on wrap and PWM fall

            // Apply MinBLEP correction
            *xor += xorMinBlepBuffer[g].process();
        }
    }
};

// Usage in main process():
float_4 saw1, sqr1, tri1, sine1;
float_4 saw2, sqr2, tri2, sine2, xor;
int vco1WrapMask, vco2WrapMask;

// VCO1: no XOR output (sqr1Input unused, xor nullptr)
vco1.process(g, freq1, sampleTime, pwm1_4,
             saw1, sqr1, tri1, sine1, vco1WrapMask);

// VCO2: pass sqr1, receive XOR
vco2.process(g, freq2, sampleTime, pwm2_4,
             saw2, sqr2, tri2, sine2, vco2WrapMask,
             sqr1,      // sqr1 from VCO1
             &xor);     // XOR output
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Digital XOR logic | Analog ring modulation (multiplication) | Classic analog | Smoother timbres, no harsh transitions |
| Additive CV + knob | CV replaces knob | VCV Rack v1 | Predictable behavior, matches hardware |
| Hard clipping | Soft clipping (tanh) | Eurorack standard | Musical saturation vs digital harshness |
| Dedicated XOR jack | Mixed with VCO2 | Modern dense panels | Saves panel space, flexible routing |
| Unipolar 0-5V CV | Unipolar 0-10V CV | VCV Rack standard | Full 0-10 volume range accessible |

**Deprecated/outdated:**
- **Digital XOR gate:** True XOR logic (sqr1 XOR sqr2) produces harsh discontinuous jumps. Ring modulation (sqr1 * sqr2) is analog standard, smoother.
- **CV + knob additive:** VCV Rack standard is CV replaces knob. Additive was confusing for users.
- **Hard clipping at rail voltage:** Digital hard clipping at ±5V creates harsh distortion. Soft clipping with tanh is eurorack standard.

## Open Questions

Things that couldn't be fully resolved:

1. **XOR MinBLEP buffer location**
   - What we know: Could be in VcoEngine (VCO2 instance) or module-level
   - What's unclear: Performance implications of VcoEngine bloat vs architectural cleanliness
   - Recommendation: Place in VCO2's VcoEngine. XOR is VCO2's waveform, VCO2 owns the buffer. Consistent with saw/sqr/tri MinBLEP buffers. Minimal bloat (only 4 instances, not 16).

2. **Soft clipping scale factor**
   - What we know: tanh(x/scale) * scale controls saturation threshold
   - What's unclear: Optimal scale for this module (3.0? 4.0? 5.0?)
   - Recommendation: Start with 3.0 (saturates at ±3V input). Test with all waveforms active. Adjust if output sounds too compressed or clips too early. Can make configurable via context menu if users want adjustable headroom.

3. **XOR default level**
   - What we know: Context says "default level: Off (0)"
   - What's unclear: Should XOR param default be 0.0 or small value like 0.1?
   - Recommendation: Default to 0.0 (off). XOR is aggressive effect, not all patches use it. User enables when wanted. Matches sub-oscillator pattern (also defaults to 0).

4. **CV input jack ordering**
   - What we know: 6 CV jacks need positioning on 36-40HP panel
   - What's unclear: Optimal arrangement for user workflow
   - Recommendation: Position CV jacks directly below corresponding knobs. Vertical alignment makes patching intuitive. VCO1 side (left): SAW1-CV, SQR1-CV, SUB-CV. VCO2 side (right): XOR-CV, SQR2-CV, SAW2-CV. Mirrors knob layout.

5. **PWM interaction with XOR**
   - What we know: PWM on both VCOs changes XOR character (context decision: natural interaction)
   - What's unclear: Whether xorMinBlepBuffer correctly tracks PWM threshold crossings from both VCOs
   - Recommendation: Yes, track all 4 edge types: vco1Wrap, vco1Fall, vco2Wrap, vco2Fall. Each inserts XOR discontinuity proportional to other VCO's square value. Full dynamic response to PWM modulation.

## Sources

### Primary (HIGH confidence)
- VCV Rack SDK 2.6.6 headers:
  - `/include/dsp/filter.hpp` - dsp::TRCFilter implementation
  - `/include/simd/functions.hpp` - simd::clamp(), SIMD operations
  - `/include/engine/Port.hpp` - getPolyVoltageSimd() polyphonic CV
- Existing HydraQuartetVCO.cpp - MinBlepBuffer pattern, VcoEngine architecture, DC filtering
- [VCV Rack API: TRCFilter](https://vcvrack.com/docs-v2/structrack_1_1dsp_1_1TRCFilter) - DC filter implementation
- [VCV Rack Manual: Polyphony](https://vcvrack.com/manual/Polyphony) - Polyphonic CV patterns
- C++ std::tanh() documentation - Hyperbolic tangent for soft clipping

### Secondary (MEDIUM confidence)
- [Stanford CCRMA: Soft Clipping](https://ccrma.stanford.edu/~jos/pasp/Soft_Clipping.html) - Cubic and tanh soft clipping algorithms
- [Instruō tanh[3] Manual](https://www.instruomodular.com/product/tanh/) - Eurorack tanh saturation reference
- [KVR Audio: MinBLEP and Ring Modulation](https://www.kvraudio.com/forum/viewtopic.php?t=437116) - Community discussion on antialiasing multiplication
- Phase 5 RESEARCH.md - Polyphonic CV pattern from PWM CV implementation
- Phase 7 RESEARCH.md - Cross-VCO parameter passing pattern from hard sync

### Tertiary (LOW confidence)
- WebSearch: "ring modulation antialiasing" - Confirmation that multiplying two bandlimited signals doesn't introduce new aliasing (but XOR still needs edge tracking)
- WebSearch: "VCV Rack CV voltage standards" - 0-10V for control voltages confirmed

## Metadata

**Confidence breakdown:**
- Ring modulation implementation: HIGH - Multiply two existing signals, MinBLEP pattern proven
- Polyphonic CV inputs: HIGH - Same pattern as PWM CV (Phase 5), well-documented in SDK
- Soft clipping: HIGH - std::tanh() is C++ standard, eurorack-proven approach
- XOR MinBLEP tracking: MEDIUM - Dual-input edge tracking is novel for this codebase, but pattern is straightforward extension of existing MinBLEP

**Research date:** 2026-01-23
**Valid until:** 2026-03-23 (60 days - DSP patterns stable, SDK stable)
