# Phase 4: Dual VCO Architecture - Research

**Researched:** 2026-01-23
**Domain:** Dual oscillator architecture with pitch control, SIMD state management, and code organization
**Confidence:** HIGH

## Summary

This research addresses the central challenge of Phase 4: duplicating VCO1's SIMD architecture for VCO2 while keeping code maintainable and implementing octave switches and detune controls. The key findings are:

1. **Struct-based engine pattern:** Extract common oscillator DSP into a templated `VcoEngine` struct that encapsulates phase, MinBLEP buffers, and waveform generation. Instantiate twice (one for VCO1, one for VCO2) rather than copy-pasting DSP code.

2. **Pitch calculation:** Octave switches add integer values directly to V/Oct pitch before frequency calculation. Detune adds fractional values. The formula `freq = FREQ_C4 * exp2_taylor5(pitch + octave + detune)` handles all pitch modifiers.

3. **Detune implementation:** For musical "thickness" effect with consistent beating, use a percentage-based detune that adds to the V/Oct pitch: `detuneVolts = detuneKnob * maxDetuneCents / 1200.0`. This creates constant-ratio detuning where the beating frequency scales with pitch.

The current codebase already has VCO2 parameters defined but not connected to DSP. The architecture is ready for a clean dual-oscillator implementation by extracting the existing VCO1 DSP into a reusable engine struct.

**Primary recommendation:** Create a `VcoEngine` struct containing phase, MinBLEP buffers, and tri state. Instantiate two engines per SIMD group. Process both with separate pitch calculations, then sum outputs.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `rack::simd::float_4` | SDK 2.x | SIMD vector for 4 voices | Already in use, handles 4 voices/instruction |
| `dsp::exp2_taylor5()` | SDK 2.x | Pitch to frequency | 5x faster than pow(2,x), already in codebase |
| `MinBlepBuffer<32>` | Custom | Antialiased discontinuities | Already implemented in Phase 3 |
| `configSwitch()` | SDK 2.x | Octave selector | Standard API for discrete parameter choices |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `RoundBlackSnapKnob` | SDK 2.x | Octave knob widget | Snaps to integer values for discrete octaves |
| `simd::clamp()` | SDK 2.x | Frequency safety | Prevent numerical issues at extremes |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Struct extraction | Lambda/function | Struct is cleaner, holds state per oscillator |
| Percentage detune | Hz-offset detune | Hz-offset gives constant beat, but percentage is simpler and musical |
| Copy-paste VCO1 code | Reusable engine | Copy-paste is faster short-term but unmaintainable |

## Architecture Patterns

### Recommended Structure

```cpp
// Extracted engine struct - replaces inline DSP code
struct VcoEngine {
    float_4 phase[4] = {};
    MinBlepBuffer<32> sawMinBlepBuffer[4];
    MinBlepBuffer<32> sqrMinBlepBuffer[4];
    float_4 triState[4] = {};

    void process(int g, float_4 freq, float sampleTime,
                 float_4 pwm, float_4& saw, float_4& sqr,
                 float_4& tri, float_4& sine);
};

// Module holds two engines
struct HydraQuartetVCO : Module {
    VcoEngine vco1;
    VcoEngine vco2;
    // ... rest of module
};
```

### Pattern 1: Struct-Based Oscillator Engine

**What:** Extract all per-oscillator state and processing into a reusable struct
**When to use:** Any dual/multi oscillator module

```cpp
// Source: Adapted from VCV Fundamental VCO pattern
struct VcoEngine {
    // Per-oscillator SIMD state (4 groups for 16 voices)
    float_4 phase[4] = {};
    MinBlepBuffer<32> sawMinBlepBuffer[4];
    MinBlepBuffer<32> sqrMinBlepBuffer[4];
    float_4 triState[4] = {};

    // Process one SIMD group, returns 4 waveforms
    void process(int g, float_4 freq, float sampleTime, float_4 pwm,
                 float_4& saw, float_4& sqr, float_4& tri, float_4& sine) {
        // Phase accumulation
        float_4 deltaPhase = simd::clamp(freq * sampleTime, 0.f, 0.49f);
        float_4 oldPhase = phase[g];
        phase[g] += deltaPhase;

        // Wrap detection and MinBLEP insertion
        float_4 wrapped = phase[g] >= 1.f;
        phase[g] -= simd::floor(phase[g]);

        // [MinBLEP insertions for saw, square...]
        // [Waveform generation...]
    }
};
```

### Pattern 2: Pitch Calculation with Octave and Detune

**What:** Combine V/Oct, octave switch, and detune into single pitch value
**When to use:** Any pitch calculation involving multiple modifiers

```cpp
// Source: VCV Rack voltage standards + Bogaudio STACK pattern
void process(const ProcessArgs& args) override {
    int channels = std::max(1, inputs[VOCT_INPUT].getChannels());

    // Read pitch modifiers (once per sample, outside loop)
    float octave1 = std::round(params[OCTAVE1_PARAM].getValue());  // -2 to +2
    float octave2 = std::round(params[OCTAVE2_PARAM].getValue());  // -2 to +2
    float detune1 = params[DETUNE1_PARAM].getValue();              // 0 to 1

    // Convert detune knob (0-1) to V/Oct offset
    // 50 cents max detune = 50/1200 octaves = 0.0417V
    float detuneVolts1 = detune1 * (50.f / 1200.f);

    for (int c = 0; c < channels; c += 4) {
        int g = c / 4;

        // Base pitch from V/Oct input
        float_4 basePitch = inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c);

        // VCO1: base + octave + detune
        float_4 pitch1 = basePitch + octave1 + detuneVolts1;
        float_4 freq1 = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch1);

        // VCO2: base + octave (reference oscillator, no detune)
        float_4 pitch2 = basePitch + octave2;
        float_4 freq2 = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch2);

        // Process both engines
        float_4 saw1, sqr1, tri1, sine1;
        float_4 saw2, sqr2, tri2, sine2;
        vco1.process(g, freq1, sampleTime, pwm1, saw1, sqr1, tri1, sine1);
        vco2.process(g, freq2, sampleTime, pwm2, saw2, sqr2, tri2, sine2);

        // Mix both VCOs
        float_4 mixed = tri1 * triVol1 + sqr1 * sqrVol1 + sine1 * sinVol1 + saw1 * sawVol1
                      + tri2 * triVol2 + sqr2 * sqrVol2 + sine2 * sinVol2 + saw2 * sawVol2;
    }
}
```

### Pattern 3: configSwitch for Octave Selectors

**What:** Discrete octave values with snap behavior
**When to use:** Octave switches (-2 to +2 octaves)

```cpp
// Source: VCV Rack Plugin API Guide
configSwitch(OCTAVE1_PARAM, -2.f, 2.f, 0.f, "VCO1 Octave",
             {"-2", "-1", "0", "+1", "+2"});
configSwitch(OCTAVE2_PARAM, -2.f, 2.f, 0.f, "VCO2 Octave",
             {"-2", "-1", "0", "+1", "+2"});

// Widget: RoundBlackSnapKnob automatically snaps to integer values
addParam(createParamCentered<RoundBlackSnapKnob>(
    mm2px(Vec(15.24, 28.0)), module, HydraQuartetVCO::OCTAVE1_PARAM));
```

### Anti-Patterns to Avoid

- **Copy-paste DSP code for VCO2:** Creates maintenance nightmare. Extract to struct instead.
- **Separate phase variables:** `phase1[4]` and `phase2[4]` as module members. Keep in engine struct.
- **Hz-based detune for musical thickness:** Sounds different at each octave. Use percentage (cents) for consistent character.
- **Processing VCO2 in separate loop:** Loses cache locality. Process both VCOs in same channel loop.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Cents to V/Oct | Custom conversion | `cents / 1200.f` | Standard formula, one line |
| Octave switch widget | Custom snap logic | `RoundBlackSnapKnob` | Built-in snap to integers |
| Pitch calculation | Separate freq calcs | Single `exp2_taylor5` call | One math operation handles all modifiers |
| MinBLEP for VCO2 | New buffer class | Same `MinBlepBuffer<32>` | Already works, just instantiate twice |

**Key insight:** The existing Phase 3 infrastructure is complete. Phase 4 is purely structural refactoring (extract engine) plus parameter wiring (octave/detune). No new DSP algorithms needed.

## Common Pitfalls

### Pitfall 1: Double CPU Without Double Sound

**What goes wrong:** Adding VCO2 but forgetting to wire volumes, so user can't hear difference
**Why it happens:** DSP works but params not connected
**How to avoid:** Verify all 8 volume params (4 waveforms x 2 VCOs) are read and applied
**Warning signs:** CPU increases but sound unchanged; default volumes at 0

### Pitfall 2: Detune Direction Confusion

**What goes wrong:** Detune knob at 0 should mean "in tune," but implementation puts both oscillators out of tune
**Why it happens:** Applying detune to both VCOs instead of just VCO1
**How to avoid:** VCO1 gets detune applied; VCO2 is the "reference" with no detune
**Warning signs:** Both oscillators sharp/flat relative to input pitch

### Pitfall 3: State Leakage Between Engines

**What goes wrong:** VCO1 and VCO2 share state accidentally, creating phase correlation
**Why it happens:** Struct members not properly separated, or passing by reference incorrectly
**How to avoid:** Each `VcoEngine` instance owns its own complete state
**Warning signs:** Oscillators sound "locked" even with different octave settings

### Pitfall 4: Octave Switch Not Snapping

**What goes wrong:** Octave knob produces fractional values (1.3 octaves)
**Why it happens:** Using `RoundBlackKnob` instead of `RoundBlackSnapKnob`
**How to avoid:** Use `RoundBlackSnapKnob` for octave params, `std::round()` when reading
**Warning signs:** Octave doesn't sound like clean octave jump

### Pitfall 5: CPU Explodes (More Than 2x)

**What goes wrong:** CPU usage far exceeds 2x Phase 3 baseline
**Why it happens:** Re-reading params inside inner loop, or processing engines separately
**How to avoid:** Read params once outside loop, process both VCOs in same channel iteration
**Warning signs:** CPU usage 3-4x instead of expected 2x

## Code Examples

### Complete Engine Extraction

```cpp
// Source: Pattern adapted from VCV Fundamental VCO.cpp
struct VcoEngine {
    float_4 phase[4] = {};
    MinBlepBuffer<32> sawMinBlepBuffer[4];
    MinBlepBuffer<32> sqrMinBlepBuffer[4];
    float_4 triState[4] = {};

    void process(int g, float_4 freq, float sampleTime, float_4 pwm,
                 float_4& saw, float_4& sqr, float_4& tri, float_4& sine) {
        // Phase accumulation
        float_4 deltaPhase = simd::clamp(freq * sampleTime, 0.f, 0.49f);
        float_4 oldPhase = phase[g];
        phase[g] += deltaPhase;

        // Detect wrap
        float_4 wrapped = phase[g] >= 1.f;
        phase[g] -= simd::floor(phase[g]);

        // === SAWTOOTH with MinBLEP ===
        int wrapMask = simd::movemask(wrapped);
        if (wrapMask) {
            for (int i = 0; i < 4; i++) {
                if (wrapMask & (1 << i)) {
                    float subsample = (1.f - oldPhase[i]) / deltaPhase[i] - 1.f;
                    sawMinBlepBuffer[g].insertDiscontinuity(subsample, -2.f, i);
                }
            }
        }
        saw = 2.f * phase[g] - 1.f + sawMinBlepBuffer[g].process();

        // === SQUARE with PWM ===
        float_4 fallingEdge = (oldPhase < pwm) & (phase[g] >= pwm);
        int fallMask = simd::movemask(fallingEdge);
        if (fallMask) {
            for (int i = 0; i < 4; i++) {
                if (fallMask & (1 << i)) {
                    float subsample = (pwm[i] - oldPhase[i]) / deltaPhase[i] - 1.f;
                    sqrMinBlepBuffer[g].insertDiscontinuity(subsample, -2.f, i);
                }
            }
        }
        if (wrapMask) {
            for (int i = 0; i < 4; i++) {
                if (wrapMask & (1 << i)) {
                    float subsample = (1.f - oldPhase[i]) / deltaPhase[i] - 1.f;
                    sqrMinBlepBuffer[g].insertDiscontinuity(subsample, 2.f, i);
                }
            }
        }
        sqr = simd::ifelse(phase[g] < pwm, 1.f, -1.f) + sqrMinBlepBuffer[g].process();

        // === TRIANGLE via integration ===
        triState[g] = triState[g] * 0.999f + sqr * 4.f * freq * sampleTime;
        tri = triState[g];

        // === SINE ===
        sine = simd::sin(2.f * float(M_PI) * phase[g]);
    }
};
```

### Detune Knob Parameter Setup

```cpp
// Source: VCV Plugin API Guide + Bogaudio DETUNE pattern
// Range 0-1 displayed as "0% to 100%"
// Internally converts to 0-50 cents
configParam(DETUNE1_PARAM, 0.f, 1.f, 0.f, "VCO1 Detune", "%", 0.f, 100.f);

// In process():
float detuneKnob = params[DETUNE1_PARAM].getValue();  // 0-1
float maxDetuneCents = 50.f;  // Musical sweet spot
float detuneVolts = detuneKnob * (maxDetuneCents / 1200.f);  // Convert to V/Oct
```

### Mixing Both VCOs

```cpp
// Source: Standard VCV Rack pattern
// Read all 8 volume controls once
float triVol1 = params[TRI1_PARAM].getValue();
float sqrVol1 = params[SQR1_PARAM].getValue();
float sinVol1 = params[SIN1_PARAM].getValue();
float sawVol1 = params[SAW1_PARAM].getValue();
float triVol2 = params[TRI2_PARAM].getValue();
float sqrVol2 = params[SQR2_PARAM].getValue();
float sinVol2 = params[SIN2_PARAM].getValue();
float sawVol2 = params[SAW2_PARAM].getValue();

// Inside SIMD loop:
float_4 mixed = tri1 * triVol1 + sqr1 * sqrVol1 + sine1 * sinVol1 + saw1 * sawVol1
              + tri2 * triVol2 + sqr2 * sqrVol2 + sine2 * sinVol2 + saw2 * sawVol2;
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Inline duplicate DSP | Struct-based engine extraction | General best practice | Maintainable dual/multi oscillator |
| Hz-offset detune | Percentage (cents) detune | Varies by synth | Consistent musical character |
| Separate processing loops | Single loop, multiple engines | SIMD optimization era | Better cache locality |
| Manual octave calculation | `exp2_taylor5(pitch + octave)` | VCV SDK 1.0+ | Single operation handles all |

**Deprecated/outdated:**
- Per-oscillator processing loops: Loses SIMD and cache benefits
- Manual pitch-to-freq with `pow(2, x)`: Use `exp2_taylor5` instead

## Open Questions

1. **Optimal max detune range**
   - What we know: 50 cents is common "sweet spot" for thickness
   - What's unclear: Whether users want wider range (100+ cents) for extreme effects
   - Recommendation: Start with 50 cents, can increase later based on feedback

2. **VCO2 fine tune vs detune**
   - What we know: Current code has `FINE2_PARAM` (-1 to +1 displayed as cents)
   - What's unclear: Whether this duplicates VCO1 detune functionality
   - Recommendation: Keep fine tune on VCO2 as it serves different purpose (precision tuning, not thickness)

3. **DC filter per VCO vs combined**
   - What we know: Currently one DC filter per voice on mixed output
   - What's unclear: Whether separate DC filters per VCO would help
   - Recommendation: Keep combined DC filter (not in hot path, works fine)

## Sources

### Primary (HIGH confidence)
- VCV Rack SDK 2.x Documentation - Polyphony manual, SIMD processing
- VCV Rack Voltage Standards - 1V/oct formula: `f = f0 * 2^V`
- VCV Rack Plugin API Guide - configSwitch, configParam usage
- Existing HydraQuartetVCO.cpp - Verified working Phase 3 SIMD implementation

### Secondary (MEDIUM confidence)
- VCV Fundamental VCO.cpp (GitHub v2 branch) - Engine extraction pattern
- Bogaudio DETUNE/STACK modules - Pitch processing patterns
- sengpielaudio.com - Cents to frequency ratio formula verified

### Tertiary (LOW confidence)
- KVR Audio forums - Detune sweet spot discussions (musical opinion)
- MOD WIGGLER forums - Constant beat detuning concepts (not VCV-specific)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Using existing Phase 3 infrastructure
- Architecture: HIGH - Struct extraction is standard refactoring pattern
- Pitch calculation: HIGH - VCV voltage standards documented
- Pitfalls: MEDIUM - Based on common DSP mistakes, not VCV-specific bugs

**Research date:** 2026-01-23
**Valid until:** 2026-04-23 (3 months - stable domain, no API changes expected)
