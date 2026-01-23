# Phase 5: PWM & Sub-Oscillator - Research

**Researched:** 2026-01-23
**Domain:** Pulse width modulation CV control, sub-oscillator generation, and modulation visualization in VCV Rack
**Confidence:** HIGH

## Summary

Phase 5 implements PWM CV control with attenuverters and a sub-oscillator for VCO1. Research confirms that the existing codebase already has MinBLEP-antialiased PWM working correctly in VcoEngine - the PWM knobs function properly but CV inputs are not yet wired. The phase primarily requires wiring CV inputs with attenuverters, adding a sub-oscillator derived from VCO1's frequency, and implementing LED ring indicators for modulation visualization.

**Key findings:**
- PWM CV inputs exist (PWM1_INPUT, PWM2_INPUT) but are not connected to any logic - this is a wiring task
- Sub-oscillators are traditionally implemented via frequency division (half frequency = -1 octave) rather than phase division
- Bipolar CV modulation (-5V to +5V) with attenuverter is the VCV Rack standard for PWM CV
- LED ring indicators around ports can be implemented using LightWidget with circular SVG elements or by adapting Bogaudio's IndicatorKnob pattern
- The existing VcoEngine::process() already accepts PWM as a float_4 parameter - adding CV modulation is straightforward

**Primary recommendation:** Wire PWM CV inputs with bipolar attenuverter processing, implement sub-oscillator as a third VcoEngine instance at half-frequency (or direct sine/square generation at half frequency), and add modulation indicator lights around PWM CV ports.

## Standard Stack

The established libraries/tools for this phase:

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| VCV Rack SDK | 2.6.6 | Input/output handling, param processing | Already in use, native polyphonic CV handling |
| simd::clamp() | SDK 2.x | Clamp PWM to safe range (0.01-0.99) | Prevents DC offset at extremes |
| dsp::FREQ_C4 | SDK 2.x | Base frequency constant | Already in use for pitch calculations |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| MediumLight<> | SDK 2.x | LED indicator around ports | For PWM CV activity indicators |
| dsp::TRCFilter | SDK 2.x | DC blocking on sub output | If sub-oscillator needs DC filtering |
| simd::sin() | SDK 2.x | Sub-oscillator sine wave | If sub uses sine waveform |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Frequency-halved VcoEngine | Phase divider flip-flop | Flip-flop produces only square; frequency halving allows all waveforms |
| Separate sub-oscillator sine | Integrate VcoEngine output | Cleaner separation vs code reuse |

**Installation:**
```bash
# No additional libraries needed - all in VCV Rack SDK 2.6.6
```

## Architecture Patterns

### Recommended Project Structure
```
src/HydraQuartetVCO.cpp
├── VcoEngine (existing)           # Shared by VCO1, VCO2, and sub-oscillator
│   └── process(g, freq, sampleTime, pwm, ...) # Already SIMD-optimized
├── HydraQuartetVCO
│   ├── vco1, vco2 (existing)      # Dual VCO engines
│   ├── subOsc                      # NEW: Sub-oscillator engine or inline generation
│   ├── PWM CV processing           # NEW: Wire inputs with attenuverters
│   └── LED light updates           # NEW: Modulation indicators
└── HydraQuartetVCOWidget
    ├── Attenuverter knobs          # NEW: PWM1_ATT_PARAM, PWM2_ATT_PARAM
    ├── Sub output jack             # NEW: SUB_OUTPUT
    └── LED rings                   # NEW: Around PWM CV inputs
```

### Pattern 1: Bipolar CV with Attenuverter
**What:** Scale and optionally invert incoming CV, sum with knob position
**When to use:** All CV-controlled parameters in VCV Rack

**Example:**
```cpp
// Source: VCV Rack Voltage Standards + Bogaudio pattern
// https://vcvrack.com/manual/VoltageStandards

// Read CV input (may be polyphonic)
float_4 pwmCV = inputs[PWM1_INPUT].getPolyVoltageSimd<float_4>(c);

// Apply attenuverter: -1.0 to +1.0 range
float attenuverter = params[PWM1_ATT_PARAM].getValue(); // -1 to 1

// Scale CV: +/-5V * attenuverter gives +/-5V or inverted
// Then normalize to +/-0.5 for PWM range
float_4 cvContribution = pwmCV * attenuverter * 0.1f; // 0.1 = 1/10V, maps 10Vpp to 1.0

// Sum with knob value
float knobValue = params[PWM1_PARAM].getValue(); // 0 to 1
float_4 pwm = knobValue + cvContribution;

// Clamp to safe PWM range (avoid DC at extremes)
pwm = simd::clamp(pwm, 0.01f, 0.99f);
```

### Pattern 2: Sub-Oscillator via Frequency Division
**What:** Generate sub-oscillator by halving the master oscillator frequency
**When to use:** Sub-oscillator that tracks master perfectly

**Example:**
```cpp
// Source: Electric Druid sub-oscillator study
// https://electricdruid.net/a-study-of-sub-oscillators/

// Sub-oscillator tracks VCO1 frequency exactly one octave below
float_4 subFreq = freq1 * 0.5f;  // Half frequency = -1 octave

// For -2 octaves: freq1 * 0.25f

// Option A: Reuse VcoEngine for full waveform options
subOsc.process(g, subFreq, sampleTime, 0.5f, subSaw, subSqr, subTri, subSine);

// Option B: Direct generation for square-only (more efficient)
// Sub-oscillator phase accumulation
subPhase[g] += subFreq * sampleTime;
subPhase[g] -= simd::floor(subPhase[g]);
float_4 subSquare = simd::ifelse(subPhase[g] < 0.5f, 1.f, -1.f);

// Option C: Direct generation for sine-only
float_4 subSine = simd::sin(2.f * float(M_PI) * subPhase[g]);
```

### Pattern 3: Sub-Oscillator Waveform Switch
**What:** Toggle between square and sine sub-oscillator
**When to use:** User-selectable sub-oscillator character

**Example:**
```cpp
// Source: Context decision - switchable square/sine

// Read waveform switch (0 = square, 1 = sine)
float subWaveformSwitch = params[SUB_WAVE_PARAM].getValue();

// Generate both waveforms
float_4 subSquare = simd::ifelse(subPhase[g] < 0.5f, 1.f, -1.f);
float_4 subSine = simd::sin(2.f * float(M_PI) * subPhase[g]);

// Select based on switch
float_4 subOutput = (subWaveformSwitch < 0.5f) ? subSquare : subSine;

// Note: If MinBLEP antialiasing needed for square sub, use VcoEngine instead
```

### Pattern 4: Dual Output (Mixed + Dedicated)
**What:** Sub-oscillator feeds both internal mix and dedicated output jack
**When to use:** Flexible routing - internal mix OR external processing

**Example:**
```cpp
// Source: Context decision - both level knob and dedicated output

// Read sub level for internal mix
float subLevel = params[SUB_LEVEL_PARAM].getValue();

// Mix into main VCO1 output
float_4 vco1Mixed = tri1 * triVol1 + sqr1 * sqrVol1 + sine1 * sinVol1 + saw1 * sawVol1
                  + subOutput * subLevel;

// Also output to dedicated SUB jack (full level, unaffected by mix knob)
outputs[SUB_OUTPUT].setVoltageSimd(subOutput * 5.f, c);
```

### Pattern 5: LED Ring Modulation Indicator
**What:** Visual feedback showing CV modulation activity around a port
**When to use:** PWM CV inputs to show modulation is active

**Example (conceptual - may require custom widget):**
```cpp
// Source: Bogaudio IndicatorKnob pattern adapted for port
// https://github.com/bogaudio/BogaudioModules

// In process(), update light based on CV activity
// Use RMS or peak detection of incoming CV
float cvLevel = std::abs(inputs[PWM1_INPUT].getVoltage());
float lightBrightness = simd::clamp(cvLevel / 5.f, 0.f, 1.f);
lights[PWM1_CV_LIGHT].setBrightness(lightBrightness);

// In widget, position light around port
addChild(createLightCentered<SmallLight<GreenLight>>(
    mm2px(Vec(portX, portY - 3.0)), // Offset above port
    module,
    HydraQuartetVCO::PWM1_CV_LIGHT
));
```

### Anti-Patterns to Avoid

- **Modulating PWM outside 0-1 range:** PWM below 0 or above 1 produces DC offset. Always clamp to 0.01-0.99.

- **Using phase division for sub-oscillator:** Classic analog approach, but produces only square wave. Frequency division allows sine/other waveforms.

- **Forgetting polyphonic CV handling:** PWM CV may be polyphonic. Always use getPolyVoltageSimd() and process per-channel.

- **Sub-oscillator without antialiasing:** If using square sub-wave with fast phase changes, aliasing can occur. Consider VcoEngine with MinBLEP for square sub.

- **Sub inheriting detune:** Per context decision, sub should track VCO1 base pitch only (ignore detune) for stable foundation.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| CV input reading | Raw getVoltage() calls | getPolyVoltageSimd<float_4>() | Polyphonic handling built-in |
| PWM clamping | Manual min/max | simd::clamp() | SIMD-optimized, handles all lanes |
| Sine wave generation | std::sin() in loop | simd::sin() | 4x faster, SSE-optimized |
| Frequency calculation | Manual pow(2, pitch) | dsp::exp2_taylor5() | Optimized polynomial approximation |
| Antialiased square | Custom polynomial | VcoEngine (existing) | Already implemented with MinBLEP |

**Key insight:** The VcoEngine already handles all waveform generation with proper antialiasing. For sub-oscillator, either reuse VcoEngine at half frequency, or generate simple sine (no aliasing needed for pure sine).

## Common Pitfalls

### Pitfall 1: PWM Range Causing DC Offset
**What goes wrong:** PWM at 0% or 100% produces constant +1 or -1 output (pure DC)
**Why it happens:** Square wave with 0% or 100% duty cycle has no transitions
**How to avoid:**
```cpp
// Clamp PWM to safe range
pwm = simd::clamp(pwm, 0.01f, 0.99f);
// 1-99% ensures at least one transition per cycle
```
**Warning signs:** VU meter shows constant offset when PWM is at extreme

### Pitfall 2: Attenuverter Polarity Confusion
**What goes wrong:** CV modulation works backwards from user expectation
**Why it happens:** Attenuverter at negative position inverts CV, but user expects knob to reduce depth only
**How to avoid:**
```cpp
// Clear labeling: attenuverter ranges -100% to +100%
// Noon position = 0 (no modulation)
// Right = positive (normal polarity)
// Left = negative (inverted polarity)
configParam(PWM1_ATT_PARAM, -1.f, 1.f, 0.f, "PWM1 CV Attenuverter", "%", 0.f, 100.f);
```
**Warning signs:** User complains "turning attenuverter up makes modulation go backwards"

### Pitfall 3: Sub-Oscillator Phase Drift
**What goes wrong:** Sub-oscillator slowly drifts out of phase with master VCO
**Why it happens:** Separate phase accumulators accumulate rounding errors differently
**How to avoid:**
```cpp
// Option A: Derive sub phase directly from master (always in sync)
float_4 subPhase = vco1.phase[g] * 0.5f;

// Option B: Reset sub phase when master wraps
if (simd::movemask(wrapped)) {
    // Reset sub phase to match master
    subPhase[g] = vco1.phase[g] * 0.5f;
}
```
**Warning signs:** Beating between sub and master even at exact -1 octave

### Pitfall 4: Polyphonic CV Not Applied Per-Voice
**What goes wrong:** PWM CV affects all voices identically even when polyphonic CV is patched
**Why it happens:** Reading only channel 0 of polyphonic input
**How to avoid:**
```cpp
// WRONG:
float pwmCV = inputs[PWM1_INPUT].getVoltage(); // Only channel 0

// CORRECT:
float_4 pwmCV = inputs[PWM1_INPUT].getPolyVoltageSimd<float_4>(c); // 4 channels
```
**Warning signs:** All voices have same PWM when expecting per-voice modulation

### Pitfall 5: Sub-Oscillator Aliasing at High Frequencies
**What goes wrong:** Square sub-oscillator produces aliasing when VCO1 is at high pitch
**Why it happens:** Sub is still at audio rate (VCO1 at 2kHz = sub at 1kHz), transitions create harmonics
**How to avoid:**
```cpp
// Option A: Use VcoEngine with MinBLEP for square sub
VcoEngine subOsc;
subOsc.process(g, freq1 * 0.5f, sampleTime, 0.5f, ...);

// Option B: Use sine-only sub (no harmonics to alias)
float_4 subSine = simd::sin(2.f * float(M_PI) * subPhase[g]);
```
**Warning signs:** Metallic artifacts in square sub at high VCO1 pitch

## Code Examples

Verified patterns from official sources and existing codebase:

### Complete PWM CV Processing
```cpp
// Source: VCV Rack patterns + existing HydraQuartetVCO.cpp

// In process(), before SIMD loop:
float pwm1Knob = params[PWM1_PARAM].getValue();
float pwm2Knob = params[PWM2_PARAM].getValue();
float pwm1Att = params[PWM1_ATT_PARAM].getValue();
float pwm2Att = params[PWM2_ATT_PARAM].getValue();

// In SIMD loop:
for (int c = 0; c < channels; c += 4) {
    int g = c / 4;

    // Read polyphonic PWM CV
    float_4 pwm1CV = inputs[PWM1_INPUT].getPolyVoltageSimd<float_4>(c);
    float_4 pwm2CV = inputs[PWM2_INPUT].getPolyVoltageSimd<float_4>(c);

    // Apply attenuverter and normalize: +/-5V * att * 0.1 = +/-0.5 contribution
    float_4 pwm1 = pwm1Knob + pwm1CV * pwm1Att * 0.1f;
    float_4 pwm2 = pwm2Knob + pwm2CV * pwm2Att * 0.1f;

    // Clamp to safe range
    pwm1 = simd::clamp(pwm1, 0.01f, 0.99f);
    pwm2 = simd::clamp(pwm2, 0.01f, 0.99f);

    // Pass to VcoEngine (existing)
    vco1.process(g, freq1, sampleTime, pwm1, saw1, sqr1, tri1, sine1);
    vco2.process(g, freq2, sampleTime, pwm2, saw2, sqr2, tri2, sine2);
}
```

### Sub-Oscillator with Waveform Selection
```cpp
// Source: Context decisions + Electric Druid sub-oscillator patterns

// Module members
float_4 subPhase[4] = {};

// In process(), inside SIMD loop:
// Sub-oscillator frequency: half of VCO1 (ignoring detune for stability)
float_4 basePitch = inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c);
float_4 subPitch = basePitch + octave1 - 1.f;  // -1 octave from VCO1 base
// Alternative for -2 octaves: basePitch + octave1 - 2.f

float_4 subFreq = dsp::FREQ_C4 * dsp::exp2_taylor5(subPitch);
subFreq = simd::clamp(subFreq, 0.1f, sampleRate / 2.f);

// Phase accumulation for sub
float_4 subDelta = subFreq * sampleTime;
subPhase[g] += subDelta;
subPhase[g] -= simd::floor(subPhase[g]);

// Generate both waveforms
float_4 subSquare = simd::ifelse(subPhase[g] < 0.5f, 1.f, -1.f);
float_4 subSine = simd::sin(2.f * float(M_PI) * subPhase[g]);

// Select based on switch (0 = square, 1 = sine)
float subWaveSwitch = params[SUB_WAVE_PARAM].getValue();
float_4 subOut = (subWaveSwitch < 0.5f) ? subSquare : subSine;

// Mix into VCO1 section
float subLevel = params[SUB_LEVEL_PARAM].getValue();
// ... add subOut * subLevel to mixed output

// Also output to dedicated SUB jack
outputs[SUB_OUTPUT].setVoltageSimd(subOut * 5.f, c);
```

### LED Modulation Indicator Update
```cpp
// Source: VCV SDK pattern

// In process(), after SIMD loop:
// Update PWM CV activity lights (use peak detection)
if (inputs[PWM1_INPUT].isConnected()) {
    float peakCV = 0.f;
    for (int c = 0; c < channels; c++) {
        peakCV = std::max(peakCV, std::abs(inputs[PWM1_INPUT].getVoltage(c)));
    }
    lights[PWM1_CV_LIGHT].setBrightness(peakCV / 5.f);
} else {
    lights[PWM1_CV_LIGHT].setBrightness(0.f);
}

// Repeat for PWM2_INPUT
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Phase divider flip-flop | Frequency halving | Digital era | Allows any waveform, not just square |
| Fixed 50% sub duty cycle | Switchable waveforms | Modern VCOs | More timbral flexibility |
| Unipolar CV (0-10V) | Bipolar CV (+-5V) with attenuverter | VCV Rack standard | Bidirectional modulation |
| Simple on/off LED | Ring indicators | Bogaudio pattern | Shows modulation depth |

**Deprecated/outdated:**
- **Phase divider sub-oscillator:** Classic analog approach produces only square waves. Digital implementations can generate any waveform at half frequency.
- **Unipolar-only PWM CV:** Modern VCO modules support bipolar CV with attenuverter for bidirectional modulation.

## Open Questions

Things that couldn't be fully resolved:

1. **MinBLEP for square sub-oscillator**
   - What we know: Square waves with fast transitions need antialiasing
   - What's unclear: At half frequency, is MinBLEP necessary or is the sub low enough that aliasing is inaudible?
   - Recommendation: For simplicity, implement sine sub without antialiasing. If square sub sounds aliased at high VCO1 pitches, retrofit with VcoEngine. Sine is a safe starting point.

2. **Sub-oscillator octave selection UI**
   - What we know: Context says Claude's discretion on whether -1 or -2 octaves, or selectable
   - What's unclear: Best UX for octave selection (switch vs knob vs context menu)
   - Recommendation: Fixed at -1 octave for v1. Simpler UI, covers 90% of use cases. -2 octave can be added in v2 via toggle switch if requested.

3. **LED ring implementation complexity**
   - What we know: Bogaudio uses IndicatorKnob with custom drawing callbacks
   - What's unclear: Whether simple SmallLight<GreenLight> positioned near port is sufficient
   - Recommendation: Start with simple positioned LED. If user feedback indicates need for ring, upgrade later. Don't over-engineer visual feedback.

4. **Sub-oscillator output polarity**
   - What we know: Traditional subs are in phase with master oscillator
   - What's unclear: Whether users expect in-phase or 180-degree offset
   - Recommendation: Keep in phase (0 degrees). Standard expectation, reinforces bass frequencies.

## Sources

### Primary (HIGH confidence)
- VCV Rack SDK 2.6.6 headers:
  - `/include/simd/functions.hpp` - simd::sin(), simd::clamp()
  - `/include/dsp/common.hpp` - FREQ_C4, exp2_taylor5()
- Existing HydraQuartetVCO.cpp - VcoEngine pattern, PWM handling
- [VCV Rack Voltage Standards](https://vcvrack.com/manual/VoltageStandards) - Bipolar CV ranges

### Secondary (MEDIUM confidence)
- [Electric Druid: A Study of Sub-Oscillators](https://electricdruid.net/a-study-of-sub-oscillators/) - Frequency division vs phase division
- [Bogaudio IndicatorKnob widgets.hpp](https://github.com/bogaudio/BogaudioModules/blob/master/src/widgets.hpp) - LED ring implementation pattern
- [Gearspace: Sub Oscillator discussion](https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/831472-sub-oscillator-osc-dropped-octave-2-a.html) - Why frequency division is standard

### Tertiary (LOW confidence)
- WebSearch results on VCV Rack sub-oscillator implementations - Confirmed frequency division approach

## Metadata

**Confidence breakdown:**
- PWM CV wiring: HIGH - SDK patterns well-documented, existing codebase has infrastructure
- Sub-oscillator implementation: HIGH - Frequency division is standard, VcoEngine can be reused
- LED ring indicators: MEDIUM - Bogaudio source available but may require custom widget work
- Attenuverter behavior: HIGH - VCV Rack standard pattern documented

**Research date:** 2026-01-23
**Valid until:** 2026-03-23 (60 days - DSP patterns stable, no expected SDK changes)
