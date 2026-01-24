# Phase 6: Through-Zero FM - Research

**Researched:** 2026-01-23
**Domain:** Through-zero linear frequency modulation for VCV Rack VCO
**Confidence:** MEDIUM

## Summary

Phase 6 implements through-zero linear frequency modulation (TZFM) where VCO1 modulates VCO2's frequency. Through-zero FM is a special type of linear FM that allows the carrier oscillator's frequency to go negative (phase runs backwards), which maintains pitch stability at unison and enables in-tune wave shaping effects. This is the key differentiator from exponential FM, which causes pitch drift when modulation depth changes.

**Technical challenge:** Through-zero FM with phase reversal is incompatible with forward-only antialiasing methods like MinBLEP. The existing VcoEngine uses MinBLEP for sawtooth and square waves, which cannot accurately handle negative frequencies. Research indicates three viable approaches: (1) use oversampling with sine-only waveforms (no aliasing), (2) switch to wavetable oscillators with interpolation, or (3) conditionally disable MinBLEP antialiasing when FM is active and rely on band-limiting + oversampling.

**Key findings:**
- Through-zero FM modulates frequency linearly (Hz/volt) and allows negative frequencies where phase accumulator decrements instead of increments
- MinBLEP is "not really possible with any decent level of accuracy" for through-zero FM due to phase reversal
- VCV Rack DSP manual confirms audio-rate FM of sine signals does not require antialiasing
- Sine-only FM is the safest approach: no antialiasing needed, CPU-efficient, and produces classic FM timbres
- FM depth should scale linearly (constant Hz/volt), with typical range allowing ±1-2 octaves of modulation at full depth
- Polyphonic CV modulation of FM depth requires per-voice processing with attenuverter

**Primary recommendation:** Implement through-zero linear FM with VCO1 modulating VCO2's frequency by directly adding modulated frequency offset to freq2 before phase accumulation. Use the existing sine waveforms from both VCOs for FM synthesis (no antialiasing needed). Add FM_PARAM knob and FM_INPUT CV with attenuverter for depth control. Apply modulation after exponential pitch conversion to maintain linear Hz/volt response.

## Standard Stack

The established libraries/tools for through-zero FM in VCV Rack:

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| VCV Rack SDK | 2.6.6 | DSP functions, SIMD processing | Already in use, native through-zero support via phase accumulator |
| simd::float_4 | SDK 2.x | SIMD frequency calculation | 4x parallel processing, existing architecture |
| dsp::exp2_taylor5() | SDK 2.x | V/oct to Hz conversion | Fast polynomial approximation, already used |
| dsp::FREQ_C4 | SDK 2.x | Base frequency constant | Standard reference (261.6256 Hz) |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| simd::clamp() | SDK 2.x | Frequency bounds checking | Prevent extreme modulation from breaking phase accumulator |
| getPolyVoltageSimd<float_4>() | SDK 2.x | Polyphonic CV input | FM depth CV (both per-voice and global) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Linear FM only | Exponential FM | Linear maintains pitch at unison; exponential causes pitch drift |
| Sine-only FM | All waveforms with oversampling | Sine has zero aliasing; other waveforms need 4x+ oversampling |
| Direct frequency modulation | Phase modulation | FM modulates frequency; PM modulates phase (different timbre) |
| MinBLEP antialiasing | Oversampling + band-limiting | MinBLEP incompatible with negative frequencies |

**Installation:**
```bash
# No additional libraries needed - all in VCV Rack SDK 2.6.6
```

## Architecture Patterns

### Recommended Project Structure
```
src/HydraQuartetVCO.cpp
├── VcoEngine (existing)              # Already generates sine waveform (use for FM)
├── HydraQuartetVCO
│   ├── vco1, vco2 (existing)         # VCO2 receives FM from VCO1
│   ├── FM depth calculation          # NEW: FM_PARAM + FM_INPUT with attenuverter
│   ├── Frequency modulation          # NEW: freq2 += modulationAmount
│   └── FM depth light indicator      # NEW: FM_CV_LIGHT (optional)
└── HydraQuartetVCOWidget
    ├── FM_PARAM knob                 # Existing (not wired)
    ├── FM_INPUT jack                 # Existing (not wired)
    └── FM attenuverter               # NEW: FM_ATT_PARAM (global CV only)
```

### Pattern 1: Through-Zero Linear FM (Direct Frequency Modulation)
**What:** Add modulator frequency directly to carrier frequency, allowing negative total frequency
**When to use:** Through-zero FM synthesis maintaining pitch at unison

**Example:**
```cpp
// Source: VCV Rack Fundamental VCO linear FM pattern
// https://github.com/VCVRack/Fundamental/blob/v2/src/VCO.cpp

// Calculate base frequencies (exponential conversion)
float_4 pitch1 = basePitch + octave1 + detuneVolts;
float_4 freq1 = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch1);

float_4 pitch2 = basePitch + octave2;
float_4 freq2 = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch2);

// Read FM depth (knob + CV)
float fmDepth = params[FM_PARAM].getValue();  // 0 to 1
float_4 fmCV = inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c);
float_4 fmAmount = fmDepth + fmCV * fmAttenuverter * 0.1f;  // Scale CV

// Apply linear FM: modulate freq2 with freq1
// At unison (freq1 == freq2), this creates wave shaping in tune
float_4 fmOffset = freq1 * fmAmount;  // Linear Hz modulation
freq2 += fmOffset;

// Clamp to prevent extreme negative frequencies
freq2 = simd::clamp(freq2, -sampleRate / 2.f, sampleRate / 2.f);

// Phase accumulation (handles negative freq automatically)
float_4 deltaPhase2 = freq2 * sampleTime;
phase2[g] += deltaPhase2;  // Goes negative when freq2 < 0
phase2[g] -= simd::floor(phase2[g]);  // Wrap both directions
```

### Pattern 2: FM Depth Scaling (Musical Range)
**What:** Scale FM depth knob and CV to musically useful frequency deviation range
**When to use:** Control how much frequency modulation is applied

**Example:**
```cpp
// Source: Derived from VCV community discussions
// Musical FM depth: ±2 octaves at full depth is typical

// Option A: Fixed Hz/V scaling (constant deviation)
float fmDepth = params[FM_PARAM].getValue();  // 0 to 1
float fmHz = fmDepth * 1000.f;  // 0 to 1000 Hz deviation
freq2 += freq1 * (fmHz / freq2);  // Normalize to ratio

// Option B: Frequency ratio scaling (musical intervals)
float fmRatio = fmDepth * 2.f;  // 0 to 2x frequency modulation
freq2 += freq1 * fmRatio;

// Option C: Match carrier frequency (wave shaping)
// At knob = 1.0, freq1 completely replaces freq2 (maximum wave shaping)
freq2 += freq1 * fmDepth;  // Clean unison modulation
```

### Pattern 3: Polyphonic FM Depth CV
**What:** Per-voice FM depth modulation via polyphonic CV
**When to use:** FM-04 requirement - polyphonic CV control of FM depth

**Example:**
```cpp
// Source: VCV Rack polyphonic CV pattern + Phase 5 research

// Per-voice FM depth modulation
float_4 fmPolyCV = inputs[FM_POLY_INPUT].getPolyVoltageSimd<float_4>(c);
float_4 fmDepthPerVoice = baseFmDepth + fmPolyCV * 0.1f;  // ±5V -> ±0.5
fmDepthPerVoice = simd::clamp(fmDepthPerVoice, 0.f, 2.f);

// Apply per-voice modulation
freq2 += freq1 * fmDepthPerVoice;
```

### Pattern 4: Global FM Depth CV with Attenuverter
**What:** Global (monophonic) CV modulation of FM depth with bipolar attenuverter
**When to use:** FM-05 requirement - global CV with attenuator

**Example:**
```cpp
// Source: Phase 5 PWM CV pattern adapted for FM

// Outside SIMD loop (read once per process() call)
float fmKnob = params[FM_PARAM].getValue();  // 0 to 1
float fmAtt = params[FM_ATT_PARAM].getValue();  // -1 to 1
float fmGlobalCV = inputs[FM_INPUT].getVoltage();  // Monophonic, channel 0

// Calculate global FM depth
float fmDepth = fmKnob + fmGlobalCV * fmAtt * 0.1f;  // ±5V -> ±0.5
fmDepth = clamp(fmDepth, 0.f, 2.f);

// Inside SIMD loop
float_4 fmModulation = freq1 * fmDepth;
freq2 += fmModulation;
```

### Pattern 5: Phase Accumulator with Negative Frequency Support
**What:** Phase accumulator that handles both positive and negative delta
**When to use:** Through-zero FM where frequency can go negative

**Example:**
```cpp
// Source: Through-zero FM wavetable pattern
// https://modwiggler.com/forum/viewtopic.php?t=8027

// Calculate phase increment (can be negative)
float_4 deltaPhase = freq * sampleTime;
// Note: Do NOT clamp deltaPhase to positive values - allow negative

// Accumulate phase (bidirectional)
phase[g] += deltaPhase;  // Increments when positive, decrements when negative

// Wrap phase (handles both directions)
phase[g] -= simd::floor(phase[g]);  // Works for negative phase too

// Generate sine (works identically for forward/backward phase)
float_4 sine = simd::sin(2.f * float(M_PI) * phase[g]);
```

### Anti-Patterns to Avoid

- **Using MinBLEP with through-zero FM:** MinBLEP is a forward-only antialiasing method incompatible with negative frequencies. Causes artifacts when phase reverses.

- **Exponential FM for through-zero:** Exponential FM (modulating pitch before exp2_taylor5) causes pitch drift. Use linear FM (modulating frequency after conversion).

- **Clamping deltaPhase to positive values:** Prevents phase reversal, breaking through-zero behavior. Allow negative deltaPhase.

- **FM all waveforms without antialiasing:** Sawtooth/square FM at audio rates produces severe aliasing. Use sine for FM, or implement oversampling for other waveforms.

- **Extreme FM depth without bounds checking:** Unclamped freq2 can overflow or cause NaN. Always clamp frequency to ±sampleRate/2.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Sine wave generation | Custom lookup table | simd::sin() with phase | SIMD-optimized, handles negative phase correctly |
| Phase wrapping | Manual if/else | simd::floor(phase) | Handles negative phase, SIMD-efficient |
| Frequency clamping | Custom bounds | simd::clamp() | SIMD-optimized, safe bounds |
| Exponential pitch | Custom pow(2, x) | dsp::exp2_taylor5() | Fast polynomial, already used |
| Through-zero antialiasing | Custom MinBLEP variant | Sine-only or oversampling | MinBLEP incompatible with phase reversal |

**Key insight:** Through-zero FM with phase reversal breaks many traditional antialiasing methods. The safest approach is to use sine waves exclusively for FM synthesis, as sine has no harmonics to alias. VCV Rack's DSP manual confirms "audio-rate FM of sine signals" does not require antialiasing.

## Common Pitfalls

### Pitfall 1: MinBLEP Artifacts with Negative Frequency
**What goes wrong:** Sawtooth/square waveforms produce glitches, clicks, or severe aliasing when frequency goes negative
**Why it happens:** MinBLEP inserts forward-looking discontinuities. When phase reverses, MinBLEP predictions are incorrect
**How to avoid:**
```cpp
// WRONG: Use MinBLEP-antialiased waveforms for FM
vco2.process(g, freq2, sampleTime, pwm2, saw2, sqr2, tri2, sine2);
float_4 fmOutput = saw2 + sqr2;  // Aliasing artifacts!

// CORRECT: Use sine only for FM synthesis
vco2.process(g, freq2, sampleTime, pwm2, saw2, sqr2, tri2, sine2);
float_4 fmOutput = sine2;  // No aliasing, works with negative freq
```
**Warning signs:** Clicks, pops, or metallic artifacts when FM depth increases

### Pitfall 2: Exponential FM Instead of Linear FM
**What goes wrong:** Pitch drifts when FM depth changes, unison modulation not in tune
**Why it happens:** Modulating pitch before exp2_taylor5 creates exponential FM, which shifts average frequency
**How to avoid:**
```cpp
// WRONG: Exponential FM (pitch drift)
float_4 pitch2 = basePitch + octave2 + fmAmount;  // Modulate pitch
float_4 freq2 = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch2);

// CORRECT: Linear FM (no pitch drift)
float_4 pitch2 = basePitch + octave2;
float_4 freq2 = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch2);
freq2 += freq1 * fmAmount;  // Modulate frequency
```
**Warning signs:** User reports "FM changes pitch" or "can't get in-tune wave shaping"

### Pitfall 3: Clamping Frequency to Positive Values Only
**What goes wrong:** Phase never reverses, through-zero behavior doesn't work
**Why it happens:** Incorrectly assuming frequency must be positive
**How to avoid:**
```cpp
// WRONG: Clamp to positive only
freq2 = simd::clamp(freq2, 0.1f, sampleRate / 2.f);

// CORRECT: Allow negative frequencies
freq2 = simd::clamp(freq2, -sampleRate / 2.f, sampleRate / 2.f);
```
**Warning signs:** FM sounds like exponential FM (pitch drift) instead of through-zero

### Pitfall 4: Polyphonic FM CV Not Applied Per-Voice
**What goes wrong:** All voices have identical FM depth even with polyphonic CV
**Why it happens:** Reading only channel 0 of polyphonic input
**How to avoid:**
```cpp
// WRONG: Monophonic CV applied to all voices
float fmCV = inputs[FM_INPUT].getVoltage();  // Channel 0 only
freq2 += freq1 * fmCV;  // Same for all voices

// CORRECT: Polyphonic CV per-voice
float_4 fmCV = inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c);
freq2 += freq1 * fmCV;  // Different per voice
```
**Warning signs:** Polyphonic FM CV produces same timbre on all voices

### Pitfall 5: FM Depth Scaling Too Aggressive
**What goes wrong:** Slight FM knob adjustment causes extreme timbral changes or instability
**Why it happens:** FM depth not scaled to musically useful range
**How to avoid:**
```cpp
// WRONG: 1:1 ratio (too aggressive)
freq2 += freq1 * fmDepth;  // At fmDepth=1, doubles frequency

// CORRECT: Scaled to musical range (e.g., ±2 octaves max)
// Option A: Frequency ratio scaling
freq2 += freq1 * fmDepth * 0.5f;  // At fmDepth=1, ±50% deviation

// Option B: Match carrier at full depth (wave shaping)
freq2 += freq1 * fmDepth;  // At fmDepth=1, freq1 becomes carrier freq
```
**Warning signs:** FM knob feels too sensitive, small changes cause extreme effects

### Pitfall 6: No Frequency Bounds Checking with FM
**What goes wrong:** NaN, Inf, or extreme CPU spikes when FM modulation is extreme
**Why it happens:** freq2 can become very large positive or negative without bounds
**How to avoid:**
```cpp
// Always clamp modulated frequency to sane range
freq2 += freq1 * fmAmount;
freq2 = simd::clamp(freq2, -sampleRate / 2.f, sampleRate / 2.f);

// Alternatively, clamp FM amount before application
fmAmount = simd::clamp(fmAmount, -2.f, 2.f);  // ±2x max deviation
```
**Warning signs:** CPU meter spikes, audio output becomes NaN or silent

## Code Examples

Verified patterns from official sources and research:

### Complete Through-Zero Linear FM Implementation
```cpp
// Source: VCV Fundamental VCO.cpp + through-zero FM research
// https://github.com/VCVRack/Fundamental/blob/v2/src/VCO.cpp
// https://modwiggler.com/forum/viewtopic.php?t=8027

// In process(), before SIMD loop:
float fmKnob = params[FM_PARAM].getValue();  // 0 to 1
float fmAtt = params[FM_ATT_PARAM].getValue();  // -1 to 1 (for global CV)
float fmGlobalCV = inputs[FM_INPUT].getVoltage();  // Monophonic

// Calculate base FM depth (global, same for all voices)
float baseFmDepth = fmKnob + fmGlobalCV * fmAtt * 0.1f;
baseFmDepth = clamp(baseFmDepth, 0.f, 1.f);

// In SIMD loop:
for (int c = 0; c < channels; c += 4) {
    int g = c / 4;

    // Calculate base frequencies
    float_4 basePitch = inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c);

    // VCO1: modulator (with detune)
    float_4 pitch1 = basePitch + octave1 + detuneVolts;
    float_4 freq1 = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch1);
    freq1 = simd::clamp(freq1, 0.1f, sampleRate / 2.f);

    // VCO2: carrier (before FM)
    float_4 pitch2 = basePitch + octave2;
    float_4 freq2 = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch2);

    // Apply through-zero linear FM
    // freq1 modulates freq2 directly (linear Hz modulation)
    freq2 += freq1 * baseFmDepth;

    // Clamp to prevent extreme values (allow negative for through-zero)
    freq2 = simd::clamp(freq2, -sampleRate / 2.f, sampleRate / 2.f);

    // Process VCO engines
    // VCO1: modulator (normal operation)
    vco1.process(g, freq1, sampleTime, pwm1_4, saw1, sqr1, tri1, sine1);

    // VCO2: carrier with FM (use sine for FM to avoid aliasing)
    vco2.process(g, freq2, sampleTime, pwm2_4, saw2, sqr2, tri2, sine2);

    // For FM synthesis, use sine outputs primarily
    // Other waveforms can be mixed in but may alias with extreme FM
    float_4 mixed = sine2 * sinVol2;  // Start with sine (no aliasing)
    // Optional: add other waveforms at reduced levels
    // mixed += tri2 * triVol2 * 0.5f;  // Triangle less prone to aliasing
}
```

### FM Depth CV with Polyphonic Support
```cpp
// Source: Requirement FM-04 - polyphonic CV input for FM depth

// Option 1: Polyphonic FM depth (each voice has independent FM depth)
// Add FM_POLY_INPUT to InputId enum
configInput(FM_POLY_INPUT, "FM Depth (Polyphonic)");

// In SIMD loop:
float_4 fmPolyCV = inputs[FM_POLY_INPUT].getPolyVoltageSimd<float_4>(c);
float_4 fmDepth = baseFmDepth + fmPolyCV * 0.1f;  // ±5V -> ±0.5
fmDepth = simd::clamp(fmDepth, 0.f, 2.f);

freq2 += freq1 * fmDepth;

// Option 2: Single input that auto-detects poly/mono
// If FM_INPUT is polyphonic, use per-voice depth
// If monophonic, broadcast to all voices
int fmChannels = inputs[FM_INPUT].getChannels();
float_4 fmCV;
if (fmChannels > 1) {
    // Polyphonic: per-voice modulation
    fmCV = inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c);
} else {
    // Monophonic: broadcast to all voices
    fmCV = float_4(inputs[FM_INPUT].getVoltage());
}
float_4 fmDepth = baseFmDepth + fmCV * fmAtt * 0.1f;
```

### FM Depth Indicator Light
```cpp
// Source: Phase 5 PWM CV light pattern

// Add to LightId enum:
FM_CV_LIGHT,

// In process(), after SIMD loop:
if (inputs[FM_INPUT].isConnected()) {
    float peakCV = 0.f;
    int fmChannels = inputs[FM_INPUT].getChannels();
    for (int i = 0; i < fmChannels; i++) {
        peakCV = std::max(peakCV, std::abs(inputs[FM_INPUT].getVoltage(i)));
    }
    lights[FM_CV_LIGHT].setBrightness(peakCV / 5.f);
} else {
    lights[FM_CV_LIGHT].setBrightness(0.f);
}

// In widget constructor:
addChild(createLightCentered<SmallLight<GreenLight>>(
    mm2px(Vec(151.0, 85.0)),  // Near FM_INPUT jack
    module,
    HydraQuartetVCO::FM_CV_LIGHT
));
```

### Testing Through-Zero Behavior (Verification)
```cpp
// Source: Requirement FM-03 - verify unison tuning

// Test case 1: Both VCOs at same pitch (C4 = 261.63 Hz)
// - Set OCTAVE1_PARAM = 0, OCTAVE2_PARAM = 0
// - Apply 0V to VOCT_INPUT
// - Set FM_PARAM = 0.5 (moderate FM depth)
// Expected: Output stays at C4, timbre changes (wave shaping in tune)

// Test case 2: Increase FM depth
// - Gradually increase FM_PARAM from 0 to 1
// Expected: Timbral changes (more harmonics), no pitch drift

// Test case 3: Spectrum analyzer verification
// - Use VCV Rack Bogaudio Analyzer-XL
// - Route VCO2 sine output to analyzer
// - Increase FM_PARAM while monitoring spectrum
// Expected: Harmonics appear symmetrically, fundamental stays at same frequency
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Exponential FM only | Linear through-zero FM | ~2010s (digital synths) | Maintains pitch at unison, enables wave shaping |
| Fixed FM depth | CV-controllable depth with attenuverter | VCV Rack standard | Dynamic FM synthesis |
| All waveforms FM | Sine-only for FM (or oversampling) | Digital aliasing awareness | Clean FM without artifacts |
| Phase modulation | True frequency modulation | Terminology clarification | Different timbral characteristics |
| MinBLEP for all waveforms | Conditional antialiasing | Through-zero incompatibility | Choose method based on use case |

**Deprecated/outdated:**
- **Exponential FM for carrier modulation:** Causes pitch drift. Linear FM is now standard for through-zero.
- **MinBLEP with bidirectional phase:** Not compatible. Use sine or oversampling for through-zero FM.
- **Fixed 1:1 FM depth ratio:** Too aggressive for musical use. Scale to ±2 octaves or match carrier frequency.

## Open Questions

Things that couldn't be fully resolved:

1. **Optimal FM depth scaling**
   - What we know: Linear FM uses Hz/volt, typical range is ±1-2 octaves at full depth
   - What's unclear: Best scaling for this specific dual VCO (match carrier freq? Fixed Hz? Ratio-based?)
   - Recommendation: Start with freq2 += freq1 * fmDepth (1:1 ratio at full depth). At unison, this creates wave shaping. User can reduce with FM knob. Test with spectrum analyzer and adjust if too aggressive.

2. **Polyphonic vs global FM depth CV**
   - What we know: Requirements specify both FM-04 (polyphonic CV) and FM-05 (global CV with attenuator)
   - What's unclear: Should these be separate inputs, or one input that auto-detects poly/mono?
   - Recommendation: Single FM_INPUT that auto-detects channel count. If poly, per-voice depth. If mono, broadcast to all voices. Simpler UX, fewer jacks. Add FM_ATT_PARAM for global CV attenuverter.

3. **Antialiasing strategy for non-sine waveforms**
   - What we know: Sine has no aliasing. Sawtooth/square with FM need antialiasing. MinBLEP incompatible with through-zero.
   - What's unclear: Should we disable all waveforms except sine when FM is active? Or allow aliasing for timbral character?
   - Recommendation: Allow all waveforms, but recommend sine for FM in documentation. Users may like aliasing artifacts for aggressive timbres. Provide warning in success criteria about spectrum analyzer verification.

4. **FM depth range bounds**
   - What we know: Unclamped FM can cause extreme frequencies or NaN
   - What's unclear: Should we hard-limit FM depth to ±2x carrier frequency? Or allow wider modulation?
   - Recommendation: Clamp final freq2 to ±sampleRate/2 (±22.05kHz at 44.1kHz). This prevents overflow while allowing aggressive modulation. If freq2 exceeds Nyquist, aliasing occurs (may be desired for some patches).

5. **Triangle waveform compatibility**
   - What we know: VCV Fundamental VCO uses triangle with FM successfully
   - What's unclear: Does triangle wave alias less than sawtooth/square with through-zero FM?
   - Recommendation: Triangle is less prone to aliasing than sawtooth/square (fewer high harmonics). Can be used for FM with acceptable results. Sine is still cleanest, but triangle offers timbral variation without severe artifacts.

## Sources

### Primary (HIGH confidence)
- [VCV Rack DSP Manual - Antialiasing](https://vcvrack.com/manual/DSP) - "Audio-rate FM of sine signals" does not require antialiasing
- [VCV Rack MinBlepGenerator API](https://vcvrack.com/docs-v2/structrack_1_1dsp_1_1MinBlepGenerator) - Template parameters and insertDiscontinuity() method
- [VCV Fundamental VCO.cpp](https://github.com/VCVRack/Fundamental/blob/v2/src/VCO.cpp) - Linear FM implementation pattern
- Existing HydraQuartetVCO.cpp - VcoEngine architecture, SIMD processing, FM_PARAM/FM_INPUT defined

### Secondary (MEDIUM confidence)
- [MOD WIGGLER: through-zero FM](https://modwiggler.com/forum/viewtopic.php?t=8027) - "MinBLEP not really possible with decent level of accuracy" for through-zero
- [KVR Audio: Through-Zero FM discussion](https://www.kvraudio.com/forum/viewtopic.php?t=150942) - Digital implementation requires "two or three times the complexity" for negative frequency handling
- [Learning Modular: TZFM](https://learningmodular.com/glossary/tzfm/) - Through-zero FM maintains tuning at unison
- [VCV Community: FM CV levels](https://community.vcvrack.com/t/fm-cv-levels-correspond-to-what-exactly/13561) - FM depth scaling varies by module, no universal standard
- [Gearspace: Linear FM discussion](https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1100357-exponential-fm-vs-linear-fm.html) - Linear FM offsets frequency by Hz/volt

### Tertiary (LOW confidence)
- [Frap Tools: Linear TZFM](https://frap.tools/linear-tzfm/) - Marketing description, limited technical details
- [Bogaudio Modules GitHub](https://github.com/bogaudio/BogaudioModules) - "CPU-efficient combination of band limiting and oversampling" (source code not examined in detail)
- WebSearch results on wavetable through-zero FM - Wavetable approach suggested but not verified with source code

## Metadata

**Confidence breakdown:**
- Through-zero FM algorithm: MEDIUM - Core concept verified across multiple sources, but MinBLEP incompatibility needs testing
- Sine-only approach: HIGH - VCV Rack DSP manual confirms no antialiasing needed for sine FM
- FM depth scaling: LOW - No authoritative standard found, module-dependent
- Polyphonic CV handling: HIGH - VCV Rack SDK patterns well-documented from Phase 5 research

**Research date:** 2026-01-23
**Valid until:** 2026-02-23 (30 days - through-zero FM is niche topic, limited new research expected)

**Critical recommendation for planner:**
The existing VcoEngine uses MinBLEP antialiasing for sawtooth and square waves. This is incompatible with through-zero FM's phase reversal. The safest implementation path is to use sine waveforms exclusively for FM synthesis (no antialiasing needed), or to implement conditional logic that disables MinBLEP when FM is active (relies on natural band-limiting of the waveforms at audio rate). Testing with spectrum analyzer (success criterion FM-05) is critical to validate antialiasing approach.
