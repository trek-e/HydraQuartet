# Architecture Research: VCV Rack Polyphonic Dual-VCO Module

**Project:** 8-voice polyphonic dual-VCO module (16 oscillators total)
**Researched:** 2026-01-22
**Confidence:** HIGH (based on official VCV Rack documentation and multiple open-source implementations)

## Executive Summary

VCV Rack modules follow a specific architecture pattern with clear separation between DSP processing (Module class) and UI (ModuleWidget class). For a polyphonic dual-VCO module with 8 voices and extensive cross-modulation, the architecture should prioritize:

1. **SIMD-optimized voice processing** for efficient polyphonic handling
2. **Dual oscillator cores per voice** with independent phase accumulators
3. **Modular DSP components** (oscillator core, FM processor, sync processor, waveform mixer)
4. **Clear data flow** from CV inputs through voice allocation to audio mixing

This research identifies proven patterns from existing VCV Rack oscillator modules and provides concrete architectural recommendations for building a dual-VCO module efficiently.

---

## VCV Rack Module Structure

### Core Architecture Pattern

VCV Rack modules consist of two primary C++ classes:

**1. Module Class** (inherits from `rack::Module`)
- Contains all DSP processing logic
- Defines inputs, outputs, parameters, and lights as enums
- Implements `process(const ProcessArgs& args)` method called at sample rate
- Maintains internal state (phase accumulators, filters, etc.)

**2. ModuleWidget Class** (inherits from `rack::ModuleWidget`)
- Handles visual representation and UI
- Maps SVG panel graphics to C++ code
- Creates knobs, ports, lights that connect to Module params/inputs/outputs

### Process Function Execution Model

The `process()` method executes continuously at the audio sample rate (typically 44.1kHz):

```cpp
void process(const ProcessArgs& args) override {
    // args.sampleTime = 1/sampleRate (e.g., 1/44100)

    // 1. Read parameters and inputs
    float pitch = params[PITCH_PARAM].getValue();
    pitch += inputs[PITCH_INPUT].getVoltage();

    // 2. Process DSP
    float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);
    phase += freq * args.sampleTime;
    if (phase >= 1.f) phase -= 1.f;

    // 3. Write outputs
    outputs[SINE_OUTPUT].setVoltage(std::sin(2.f * M_PI * phase) * 5.f);
}
```

**Key principles:**
- Single-sample processing (1-sample buffer)
- Parameters accessed via `params[INDEX].getValue()`
- Inputs read via `inputs[INDEX].getVoltage()` or `getVoltageSum()`
- Outputs written via `outputs[INDEX].setVoltage(voltage)`
- Member variables persist state between calls

### VCV Rack DSP Library

The SDK provides `rack::dsp` namespace with essential utilities:

**Anti-aliasing:**
- `MinBlepGenerator` - Bandlimited step for discontinuities
- Oversampling + decimation strategies

**Filters:**
- `TRCFilter` - Simple RC filter
- `TExponentialFilter` - Exponential smoothing
- `TSlewLimiter` - Rate limiting
- `IIRFilter` - General IIR filtering

**Utilities:**
- `ClockDivider` - Reduce processing frequency
- `PulseGenerator` - Gate/trigger generation
- `TSchmittTrigger` - Hysteresis detection

**Constants:**
- `dsp::FREQ_C4 = 261.6256f` (middle C)
- `dsp::FREQ_SEMITONE = 1.0594630943592953f`

**Fast math approximations:**
- `dsp::exp2_taylor5(pitch)` - Fast 2^x for frequency conversion
- Polynomial evaluation functions

---

## Polyphony Architecture

### Channel Model

**Cable structure:**
- All cables in VCV Rack are polyphonic (1-16 channels)
- Monophonic = special case of polyphonic with 1 channel
- Polyphonic cables appear visually thicker
- Zero-channel cables supported for virtual patching

**Channel count determination:**
- Oscillators: Driven by V/OCT input channel count
- Voices: Set by polyphonic gate/trigger input
- Maximum: 16 channels per cable

### SIMD Optimization Strategy

**Performance benefit:** Polyphonic modules with SIMD achieve up to 4x better performance (25% CPU) compared to multiple monophonic instances.

**Implementation pattern:**

**Option 1: Array of scalar engines** (simpler, ~16x CPU cost)
```cpp
OscillatorEngine engine[16];  // One per channel

void process(const ProcessArgs& args) override {
    int channels = inputs[VOCT_INPUT].getChannels();
    for (int c = 0; c < channels; c++) {
        float pitch = inputs[VOCT_INPUT].getVoltage(c);
        float output = engine[c].process(pitch, args.sampleTime);
        outputs[AUDIO_OUTPUT].setVoltage(output, c);
    }
    outputs[AUDIO_OUTPUT].setChannels(channels);
}
```

**Option 2: SIMD float_4 processing** (recommended, ~4x CPU cost)
```cpp
OscillatorEngine<simd::float_4> engine[4];  // Process 4 channels at once

void process(const ProcessArgs& args) override {
    int channels = inputs[VOCT_INPUT].getChannels();
    for (int c = 0; c < channels; c += 4) {
        simd::float_4 pitch = inputs[VOCT_INPUT].getVoltageSimd<simd::float_4>(c);
        simd::float_4 output = engine[c/4].process(pitch, args.sampleTime);
        outputs[AUDIO_OUTPUT].setVoltageSimd(output, c);
    }
    outputs[AUDIO_OUTPUT].setChannels(channels);
}
```

**SIMD considerations:**
- Template oscillator core with `<typename T>` to support both `float` and `simd::float_4`
- Use `simd::ifelse(mask, a, b)` for conditional logic in SIMD context
- Compiler auto-vectorization with `-O3 -march=nehalem -funsafe-math-optimizations`
- VCV Rack on x64 supports up to SSE4.2 (4-element float vectors)

### Voice Allocation Patterns

For 8-voice polyphony with dual oscillators per voice:

**Standard polyphonic mode:**
- V/OCT input determines channel count (up to 8)
- Each channel = one voice = two oscillators
- Linear assignment: voice 0 = channel 0, voice 1 = channel 1, etc.

**Unison mode:**
- Monophonic V/OCT copied to multiple channels
- All voices play same pitch with detune
- Example: 4-voice unison = voices 0-3 play detuned variants

**Dual mode:**
- Independent control of VCO A and VCO B
- Separate V/OCT inputs for each oscillator
- Mix output represents both oscillator sets

**Implementation recommendation:**
- Use voicing mode parameter/switch
- Mode determines channel routing logic
- Keep polyphonic as default for compatibility

---

## Component Breakdown

### Recommended Architecture Layers

```
┌─────────────────────────────────────────────────────────────┐
│                       MODULE LAYER                          │
│  - Input/output/parameter definitions                       │
│  - Polyphony channel management                             │
│  - Voicing mode logic                                       │
│  - Global mixing and routing                                │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    VOICE PROCESSOR LAYER                     │
│  - Per-voice state management                               │
│  - Dual oscillator coordination                             │
│  - Cross-modulation routing                                 │
│  - Per-voice mix output                                     │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   OSCILLATOR CORE LAYER                      │
│  - Phase accumulation                                       │
│  - Waveform generation (saw, tri, sin, sq)                 │
│  - Anti-aliasing (polyBLEP/polyBLAMP)                       │
│  - PWM, sub-oscillator generation                           │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    MODULATION LAYER                          │
│  - FM processor (exponential, linear, through-zero)         │
│  - Sync processor (hard sync, soft sync)                    │
│  - Waveshaper (XOR, fold, clip)                             │
│  - Vibrato/detune effects                                   │
└─────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

#### 1. Module Class (`PolyDualVCO`)

**Responsibilities:**
- Define all enums for params, inputs, outputs, lights
- Manage polyphonic channel count
- Route CV inputs to voice processors
- Mix voice outputs to polyphonic output cables
- Handle voicing mode switching
- Manage UI state (lights, context menus)

**State:**
- Array of voice processors (8 or 4 if using SIMD float_4)
- Voicing mode (poly/dual/unison)
- Global settings (analog character amount, etc.)

**Does NOT:**
- Perform DSP directly (delegates to voice processors)
- Know about individual oscillator internals

---

#### 2. Voice Processor (`DualVCOVoice`)

**Responsibilities:**
- Manage two oscillator cores (VCO A and VCO B)
- Route modulation between oscillators (FM, sync, XOR)
- Process per-voice CV inputs (pitch, FM amount, sync, etc.)
- Mix waveforms within the voice
- Output single audio signal per voice

**State:**
- Two oscillator core instances
- Cross-modulation state
- Per-voice parameter values

**Template for SIMD:**
```cpp
template<typename T>
class DualVCOVoice {
    OscillatorCore<T> vcoA;
    OscillatorCore<T> vcoB;

    T process(T pitchA, T pitchB, T fmAmount, T syncTrigger, float sampleTime);
};
```

**Does NOT:**
- Know about polyphony or channel count
- Access VCV Rack inputs/outputs directly
- Handle UI or parameter mapping

---

#### 3. Oscillator Core (`OscillatorCore`)

**Responsibilities:**
- Phase accumulation and wrapping
- Generate all waveforms simultaneously (saw, tri, sin, sq)
- Apply anti-aliasing (polyBLEP for discontinuities)
- PWM for square wave
- Sub-oscillator generation (octave down)
- Accept FM and sync inputs

**State:**
- Phase accumulator (0.0 to 1.0)
- Previous waveform samples (for BLEP)
- PWM duty cycle state
- Sub-oscillator phase

**Template for SIMD:**
```cpp
template<typename T = float>
class OscillatorCore {
    T phase = 0.f;
    T subPhase = 0.f;

    struct Waveforms {
        T saw;
        T tri;
        T sin;
        T sq;
        T sub;
    };

    Waveforms process(T frequency, T pwm, T fmInput, bool syncReset, float sampleTime);
};
```

**Anti-aliasing strategy:**
- **PolyBLEP** for saw, square (handles discontinuities)
- **PolyBLAMP** for triangle (handles derivative discontinuities)
- **Sine** inherently band-limited (no aliasing)
- Reference: Venom Modules use DPW alias suppression + oversampling

**Does NOT:**
- Know about cross-modulation with other oscillators
- Mix waveforms (returns all waveforms separately)
- Handle CV input scaling

---

#### 4. Modulation Processors

**FM Processor:**
```cpp
template<typename T>
class FMProcessor {
    // Through-zero FM implementation
    T applyLinearFM(T phase, T modulator, T amount);

    // Traditional exponential FM
    T applyExponentialFM(T frequency, T modulator, T amount);
};
```

**Sync Processor:**
```cpp
template<typename T>
class SyncProcessor {
    // Hard sync: reset phase on trigger
    T processHardSync(T phase, bool syncTrigger);

    // Soft sync: reverse phase direction on trigger
    T processSoftSync(T phase, T phaseInc, bool syncTrigger);

    // Probabilistic/gradual sync (for analog character)
    T processGradualSync(T phase, T syncSignal, T amount);
};
```

**Waveshaper (XOR, etc.):**
```cpp
template<typename T>
class Waveshaper {
    // XOR operation between two oscillators
    T applyXOR(T waveA, T waveB, T amount);

    // Fold/wrap operations
    T applyFold(T wave, T amount);
};
```

---

## Data Flow Architecture

### Signal Flow Diagram

```
CV INPUTS                VOICE PROCESSOR           OSCILLATOR CORES
────────────────────────────────────────────────────────────────────

V/OCT A  ──────┬──────> Pitch Calc A ──────────> VCO A Phase Acc
               │                                     │
V/OCT B  ──────┼──────> Pitch Calc B ──────────> VCO B Phase Acc
               │                                     │
               │                                     ↓
FM Amount ─────┤                              Waveform Generation
               │                               (saw/tri/sin/sq/sub)
Sync ──────────┤                                     │
               │                                     ↓
PWM ───────────┤                              Anti-aliasing (BLEP)
               │                                     │
XOR Amount ────┤                                     ↓
               │                              ┌──────┴──────┐
Detune ────────┤                              │             │
               │                           VCO A         VCO B
Vibrato ───────┘                           Waves         Waves
                                              │             │
                                              ↓             ↓
                                         ┌────┴────────────┬┘
                                         │                 │
                                         ↓                 ↓
                                    FM Modulation    Cross Sync
                                         │                 │
                                         ↓                 ↓
                                      Waveform Mixing
                                    (tri + sq + sin + saw)
                                              │
                                              ↓
                                         Per-Voice Mix
                                              │
                                              ↓
                                      AUDIO OUTPUT (poly)
```

### Processing Order Per Sample

1. **Read CV inputs** (polyphonic, per-channel)
   - V/OCT for both oscillators
   - FM amount, sync trigger, PWM, XOR, detune, vibrato

2. **Voice processor iteration** (8 voices or 4 SIMD groups)
   - Calculate frequencies from pitch CV (dsp::exp2_taylor5)
   - Apply vibrato/detune modulation
   - Process VCO A: phase accumulation → waveform generation → anti-aliasing
   - Process VCO B: same as VCO A

3. **Cross-modulation** (within each voice)
   - VCO A output → FM input to VCO B
   - VCO A sync → VCO B phase reset/reverse
   - XOR/waveshaping between VCO A and VCO B outputs

4. **Waveform mixing** (per voice)
   - Select/mix waveforms based on knobs
   - Apply analog character (subtle noise, drift)

5. **Output writing** (polyphonic)
   - Set voltage for each channel
   - Set total channel count

### CV Input Patterns

**Standard parameter + CV + attenuverter pattern:**
```cpp
// Frequency modulation amount
float fmBase = params[FM_AMOUNT_PARAM].getValue();        // Knob position
float fmCV = inputs[FM_CV_INPUT].getVoltage(channel);     // CV input
float fmAtten = params[FM_ATTEN_PARAM].getValue();        // Attenuverter
float fmTotal = fmBase + (fmCV * fmAtten * 0.1f);         // Combined value
```

**V/OCT standard:**
```cpp
// Pitch calculation (1V/octave standard)
float pitch = params[OCTAVE_PARAM].getValue();            // Octave knob
pitch += params[COARSE_PARAM].getValue() / 12.f;          // Semitone knob
pitch += params[FINE_PARAM].getValue() / 12.f;            // Fine tune
pitch += inputs[VOCT_INPUT].getVoltage(channel);          // V/OCT CV
float freq = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch);     // Fast conversion
```

**Sync trigger detection:**
```cpp
// Schmitt trigger for hard sync
dsp::SchmittTrigger syncTrigger[16];
bool syncTriggered = syncTrigger[channel].process(inputs[SYNC_INPUT].getVoltage(channel));
```

---

## Build Order and Dependencies

### Phase 1: Basic Monophonic Single VCO
**Goal:** Get a single oscillator working with one waveform

**Components to build:**
1. Module skeleton (params, inputs, outputs enums)
2. Basic oscillator core (phase accumulator + saw wave)
3. Pitch CV processing (V/OCT to frequency)
4. ModuleWidget with minimal panel

**Validation:**
- Produces clean sawtooth at correct pitch
- Responds to 1V/octave correctly (C4 at 0V)

**Dependencies:** None - pure foundation

---

### Phase 2: Waveform Generation + Anti-aliasing
**Goal:** Generate all waveforms cleanly without aliasing

**Components to build:**
1. Triangle, sine, square waveform generators
2. PolyBLEP anti-aliasing for saw/square
3. PolyBLAMP for triangle (if needed)
4. Waveform output mixing

**Validation:**
- All waveforms sound clean at high frequencies
- No audible aliasing artifacts
- Spectral analysis shows no aliasing above Nyquist

**Dependencies:** Phase 1 (needs working oscillator core)

---

### Phase 3: PWM and Sub-oscillator
**Goal:** Add PWM for square wave and sub-oscillator

**Components to build:**
1. PWM parameter + CV input
2. Duty cycle modulation in square wave generator
3. Sub-oscillator (divide-by-2 phase)
4. Sub output or mix control

**Validation:**
- PWM sweeps smoothly without clicks
- Sub-oscillator tracks main oscillator pitch
- Sub is exactly one octave down

**Dependencies:** Phase 2 (needs working square wave)

---

### Phase 4: Polyphony (SIMD)
**Goal:** Support 8-voice polyphony efficiently

**Components to build:**
1. Template oscillator core with `<typename T>`
2. SIMD float_4 support (process 4 channels at once)
3. Channel count management
4. Polyphonic output handling

**Validation:**
- Can play 8 voices simultaneously
- CPU usage reasonable (~4x single voice with SIMD)
- Each voice independent and correct

**Dependencies:** Phases 1-3 (needs fully working monophonic oscillator)

**Critical:** This is a major architectural shift. Must refactor oscillator core before proceeding.

---

### Phase 5: Dual Oscillator Architecture
**Goal:** Add second oscillator per voice

**Components to build:**
1. Duplicate oscillator core (VCO B)
2. Independent pitch control for VCO B
3. Separate waveform outputs or internal mixing
4. Voice processor coordination

**Validation:**
- Both oscillators run independently
- Can detune VCO B from VCO A
- Mix outputs correctly

**Dependencies:** Phase 4 (needs polyphonic infrastructure)

---

### Phase 6: Cross-modulation (FM)
**Goal:** VCO A can modulate VCO B frequency

**Components to build:**
1. FM processor (exponential and linear modes)
2. Through-zero FM implementation
3. FM amount CV input with attenuverter
4. FM routing in voice processor

**Validation:**
- FM produces harmonic content
- Through-zero FM works without phase discontinuities
- Amount control has musical range

**Dependencies:** Phase 5 (needs dual oscillators)

---

### Phase 7: Cross-modulation (Sync)
**Goal:** VCO A can sync VCO B

**Components to build:**
1. Sync processor (hard/soft sync modes)
2. Sync trigger detection
3. Sync amount/type controls

**Validation:**
- Hard sync produces classic sync sound
- Soft sync produces gentler harmonics
- No clicks or artifacts at sync points

**Dependencies:** Phase 5 (needs dual oscillators)

---

### Phase 8: Waveshaping (XOR, etc.)
**Goal:** Cross-modulation via waveshaping

**Components to build:**
1. XOR waveshaper
2. Other waveshaping algorithms
3. Amount controls and CV inputs

**Validation:**
- XOR produces interesting timbral changes
- Amount sweeps smoothly
- No unexpected clipping or distortion

**Dependencies:** Phase 5 (needs dual oscillators)

---

### Phase 9: Vibrato and Detune
**Goal:** Add musical pitch modulation

**Components to build:**
1. Internal LFO for vibrato
2. Detune spread for unison modes
3. Analog character drift

**Validation:**
- Vibrato sounds smooth and musical
- Detune creates rich unison sound
- Analog drift subtle but effective

**Dependencies:** Phase 4 (needs polyphonic infrastructure)

---

### Phase 10: Voicing Modes
**Goal:** Support poly/dual/unison modes

**Components to build:**
1. Voicing mode parameter
2. Channel routing logic per mode
3. Unison voice allocation
4. Dual mode separate outputs

**Validation:**
- Each mode sounds correct
- Mode switching doesn't click
- CPU usage scales appropriately

**Dependencies:** Phases 5 + 9 (needs dual oscillators + detune)

---

### Dependency Graph

```
Phase 1: Basic Monophonic Single VCO
    │
    ↓
Phase 2: Waveforms + Anti-aliasing
    │
    ↓
Phase 3: PWM + Sub-oscillator
    │
    ↓
Phase 4: Polyphony (SIMD) ────────────────┐
    │                                      │
    ↓                                      ↓
Phase 5: Dual Oscillator              Phase 9: Vibrato/Detune
    │                                      │
    ├──────┬──────┬──────────┐            │
    ↓      ↓      ↓          ↓            │
Phase 6 Phase 7 Phase 8     │            │
  (FM)  (Sync) (XOR)         │            │
    │      │      │          │            │
    └──────┴──────┴──────────┴────────────┘
                   │
                   ↓
            Phase 10: Voicing Modes
```

**Critical path:** 1 → 2 → 3 → 4 → 5 → 10
**Parallel after Phase 5:** Phases 6, 7, 8, 9 can be developed independently

---

## Architecture Patterns from VCV Rack Ecosystem

### Anti-aliasing: Lessons from Existing Modules

**Bogaudio approach:**
- "CPU-efficient combination of band limiting and oversampling"
- Simultaneous waveform outputs (saw, square, triangle, sine)
- Traditional exponential and linear through-zero FM

**Venom approach:**
- DPW (Differentiated Parabolic Wave) alias suppression
- Additional oversampling layer
- Studied Befaco Even VCO source code for best practices

**21kHz Palm Loop approach:**
- PolyBLEP for saw/square discontinuities
- PolyBLAMP for triangle derivative discontinuities
- Minimalist, CPU-friendly design

**Recommendation:**
- Start with **polyBLEP** (proven, well-documented, CPU-efficient)
- Add **polyBLAMP** for triangle if needed
- Consider **2x oversampling** for FM/sync if aliasing detected
- Profile before adding expensive anti-aliasing

### FM Implementation Patterns

**Through-zero FM characteristics:**
- Phase can go negative (reverses playback direction)
- No phase discontinuities at zero-crossing
- Linear frequency modulation input (Hz, not V/oct)
- Dedicated LIN FM input with attenuverter

**Traditional exponential FM:**
- Modulates pitch in V/octave domain
- Exponential response (like pitch CV)
- EXP FM input, typically with attenuverter

**Implementation reference:**
- 21kHz Palm Loop: Both EXP and LIN FM with dedicated inputs
- Bogaudio: Supports both exponential and through-zero linear FM

### Sync Implementation Patterns

**Hard sync:**
- Reset slave phase to 0.0 when master triggers
- Classic, bright sync sound
- Simple to implement

**Soft sync:**
- Reverse slave phase direction instead of reset
- Gentler, less harsh harmonics
- VCV VCO official implementation uses this approach

**Gradual/probabilistic sync:**
- Vult Vessek: Gradual parameter from phase interference to full sync
- 21kHz Tachyon Entangler: Probability-based sync (0% to 100%)
- Most chaotic/interesting at intermediate settings

**Recommendation:**
- Implement **hard sync first** (simpler, essential)
- Add **soft sync** as mode switch
- Consider **gradual sync** for "analog character" feature

### Dual Oscillator Cross-modulation

**Vult Vessek pattern:**
- Oscillator A modulates oscillator B (not bidirectional)
- FM and AM modulation modes
- Sync uses A's reset signal to modulate B
- Simple, clear signal flow

**Tachyon Entangler pattern:**
- Bidirectional cross-sync (probabilistic)
- Creates chaotic behavior
- Requires careful state management

**Recommendation for this project:**
- **Unidirectional modulation:** VCO A → VCO B (matches Triax 8 hardware)
- Keeps DSP simpler and more predictable
- Optional: Allow mode switch for B → A or bidirectional

---

## Performance Considerations

### CPU Optimization Strategy

**1. Profile before optimizing**
- Use VCV Rack's built-in CPU meter
- External profilers: perf, Valgrind, Hotspot
- Identify the single bottleneck (likely anti-aliasing or SIMD loop)

**2. SIMD is mandatory for 8-voice polyphony**
- 16 oscillators (8 voices × 2) would be 16x CPU without SIMD
- float_4 reduces to ~4x CPU (very reasonable)
- Template oscillator core for both float and float_4

**3. Fast math approximations**
- Use `dsp::exp2_taylor5(pitch)` instead of `std::pow(2.f, pitch)`
- <6e-06 relative error (inaudible)
- Significantly faster

**4. Clock division for slow modulation**
- Don't update vibrato LFO every sample
- Use `ClockDivider` to update every 32 or 64 samples
- Human-rate modulation doesn't need audio-rate precision

**5. Anti-aliasing tradeoffs**
- PolyBLEP: Fast, good quality, industry standard
- Oversampling: Better quality, higher CPU cost
- DPW: Alternative approach, study Venom implementation
- Choose based on profiling results

### Memory Access Patterns

**SIMD-friendly data layout:**
```cpp
// BAD: Array of structs (cache-inefficient for SIMD)
struct Voice {
    float phaseA, phaseB;
    OscillatorCore oscA, oscB;
};
Voice voices[8];

// GOOD: Struct of arrays (SIMD-friendly)
template<typename T>
struct VoiceBank {
    OscillatorCore<T> oscA[4];  // 4 × float_4 = 16 voices
    OscillatorCore<T> oscB[4];
};
```

**Minimize virtual function calls in process():**
- No virtual dispatch in hot loop
- Template-based polymorphism instead
- Inline small functions

---

## Testing Strategy

### Unit Testing Approach

**Phase accumulator correctness:**
- Verify phase wraps at 1.0
- Check frequency accuracy (should match dsp::FREQ_C4 * 2^pitch)
- Test edge cases (very high/low frequencies)

**Anti-aliasing validation:**
- Spectral analysis: no energy above Nyquist frequency
- Listen test at high frequencies (>5kHz fundamental)
- Compare aliased vs. anti-aliased side-by-side

**Polyphony correctness:**
- Each voice independent (no cross-talk)
- Channel count propagation correct
- SIMD matches scalar results (bit-exact for test signals)

**Modulation accuracy:**
- FM produces expected harmonic series
- Sync resets phase at correct time
- Through-zero FM has no discontinuities at zero-crossing

### Integration Testing

**Patch with known synthesis techniques:**
- Classic sync sweep patch
- FM bass patch
- Unison supersaw
- Through-zero FM crossfade

**CPU profiling:**
- Measure CPU usage with 1, 4, 8, 16 voices
- Should scale roughly linearly (with SIMD optimization)
- Compare to reference modules (e.g., VCV VCO)

**Edge case testing:**
- All knobs at minimum
- All knobs at maximum
- Extreme CV modulation (±10V)
- Rapid parameter changes

---

## Sources

**HIGH confidence (official VCV Rack documentation):**
- [VCV Rack Plugin Development Tutorial](https://vcvrack.com/manual/PluginDevelopmentTutorial) - Module structure, process() function, basic oscillator
- [VCV Rack Polyphony Manual](https://vcvrack.com/manual/Polyphony) - Polyphonic cable architecture, channel handling
- [VCV Rack DSP Manual](https://vcvrack.com/manual/DSP) - SIMD, anti-aliasing, performance optimization
- [VCV Rack API: rack::dsp Namespace](https://vcvrack.com/docs-v2/namespacerack_1_1dsp) - DSP classes and utilities
- [Making Your Monophonic Module Polyphonic](https://community.vcvrack.com/t/making-your-monophonic-module-polyphonic/6926) - SIMD implementation patterns

**MEDIUM confidence (open-source implementations):**
- [Bogaudio Modules GitHub](https://github.com/bogaudio/BogaudioModules) - Anti-aliasing, FM, polyphony examples
- [21kHz Modules GitHub](https://github.com/21kHz/21kHz-rack-plugins) - Through-zero FM, polyBLEP/polyBLAMP
- [Venom Modules GitHub](https://github.com/DaveBenham/VenomModules) - DPW anti-aliasing, sync implementation
- [Vult Vessek Module](https://modlfo.github.io/VultModules/vessek/) - Dual oscillator cross-modulation pattern

**LOW confidence (general ecosystem knowledge):**
- [VCV Library Oscillator Tag](https://library.vcvrack.com/?tag=Oscillator) - Survey of oscillator modules
- [VCV Free Modules](https://vcvrack.com/Free) - VCV VCO waveform generation, soft sync

---

## Confidence Assessment

| Topic | Level | Rationale |
|-------|-------|-----------|
| Module structure (Module/ModuleWidget) | HIGH | Official documentation, clear patterns |
| Polyphony SIMD implementation | HIGH | Official tutorial with code examples |
| DSP library utilities | HIGH | Official API documentation |
| Anti-aliasing techniques | MEDIUM | Multiple implementations, some variation |
| FM/sync implementation details | MEDIUM | Code available but specific approaches vary |
| Build order dependencies | MEDIUM | Inferred from architecture, not documented |
| Performance optimization | HIGH | Official DSP manual guidance |

---

## Architectural Recommendations Summary

**1. Use SIMD float_4 from the start**
- Template oscillator core: `OscillatorCore<typename T = float>`
- Process 4 channels at once (2 SIMD groups for 8 voices)
- Critical for acceptable CPU usage with 16 oscillators

**2. Layer architecture clearly**
- Module → Voice Processor → Oscillator Core → Modulation Components
- Each layer has single responsibility
- Enables unit testing and parallel development

**3. Start simple, expand iteratively**
- Phase 1-3: Monophonic, single VCO, all waveforms
- Phase 4: SIMD polyphony (major milestone)
- Phase 5-10: Dual oscillator features incrementally

**4. Anti-aliasing: PolyBLEP first, profile, then optimize**
- Proven technique, well-documented, CPU-efficient
- Add oversampling only if profiling shows aliasing problems
- Study Venom DPW approach if needed

**5. Cross-modulation: Unidirectional A→B**
- Simpler data flow, easier to understand
- Matches hardware inspiration (Triax 8)
- Can add bidirectional later if desired

**6. Separate concerns: DSP vs. routing vs. UI**
- Oscillator cores don't know about polyphony
- Voice processor doesn't know about voicing modes
- Module class handles all VCV Rack integration
