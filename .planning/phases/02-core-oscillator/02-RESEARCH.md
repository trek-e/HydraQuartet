# Phase 2: Core Oscillator with Antialiasing - Research

**Researched:** 2026-01-22
**Domain:** Digital oscillator design with band-limited synthesis (antialiasing)
**Confidence:** HIGH

## Summary

Phase 2 implements a polyphonic VCO with four waveforms (triangle, square, sine, sawtooth) using MinBLEP antialiasing. Research reveals VCV Rack SDK provides native MinBLEP support (`dsp::MinBlepGenerator`) which is the standard approach for VCV oscillators requiring high-quality antialiasing.

**Key findings:**
- MinBLEP is the established standard for VCV Rack oscillators (used by Fundamental VCO, Befaco EvenVCO)
- PolyBLEP exists as a simpler alternative but sounds "slightly duller" and is less common in VCV ecosystem
- VCV Fundamental had a critical MinBLEP bug (double discontinuity insertion in PWM) that many developers copied—must avoid
- Triangle waves require special handling: either integrate antialiased square OR use polyBLAMP for slope discontinuities
- Phase accumulator must use float for VCV Rack compatibility; drift prevention relies on proper phase wrapping
- Individual waveform mixing requires DC filtering after MinBLEP correction

**Primary recommendation:** Use `dsp::MinBlepGenerator<16, 16>` (16 zero-crossings, 16x oversample) following VCV Fundamental v2 pattern, with DC blocking via `dsp::TRCFilter::highpass()` on mixed output.

## Standard Stack

The established libraries/tools for antialiased oscillators in VCV Rack:

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| VCV Rack SDK | 2.6.6 | MinBLEP implementation, SIMD math | Official SDK, provides `dsp::MinBlepGenerator` template |
| dsp::MinBlepGenerator | SDK 2.x | Band-limited step generation | Native antialiasing, used by Fundamental VCO |
| simd::sin() / simd::cos() | SDK 2.x | Fast vectorized trig | SSE-optimized via sse_mathfun, 4-wide SIMD |
| dsp::TRCFilter | SDK 2.x | DC blocking highpass | Simple RC filter for DC offset removal |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| dsp::exp2_taylor5() | SDK 2.x | Fast V/Oct to frequency | Already used in Phase 1 for pitch tracking |
| Bogaudio ANALYZER | VCV Plugin | Spectrum analysis | Testing/verification of antialiasing quality |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| MinBLEP | PolyBLEP | Simpler (no table), but "slightly duller sound", less common in VCV |
| MinBLEP | Oversampling | Higher CPU cost, Fundamental switched FROM oversampling TO MinBLEP |
| MinBLEP | Wavetables | Lower aliasing ceiling, not suitable for <2kHz verification requirement |

**Installation:**
```bash
# No additional libraries needed - all in VCV Rack SDK 2.6.6
# Already installed at /Users/trekkie/projects/vcvrack_modules/Rack-SDK
```

## Architecture Patterns

### Recommended Project Structure
```
src/
├── TriaxVCO.cpp         # Main module with oscillator DSP
│   ├── struct TriaxVCO
│   │   ├── MinBlepGenerator instances (per-voice)
│   │   ├── Phase accumulator (per-voice)
│   │   ├── DC filters (per-voice)
│   │   └── process() with polyphonic loop
```

### Pattern 1: MinBLEP Oscillator Architecture

**What:** Phase accumulator + naive waveform + discontinuity detection + MinBLEP correction

**When to use:** All harmonically-rich waveforms (sawtooth, square, triangle via integration)

**Example (based on VCV Fundamental VCO v2):**
```cpp
// Source: https://github.com/VCVRack/Fundamental/blob/v2/src/VCO.cpp

// Per-voice state
struct VoiceState {
    float phase = 0.f;
    dsp::MinBlepGenerator<16, 16, float> sawMinBlep;
    dsp::MinBlepGenerator<16, 16, float> sqrMinBlep;
    dsp::TRCFilter<float> dcFilter;
    float dcFilterState = 0.f;
};

// In process(), per voice:
float deltaPhase = freq * args.sampleTime;
phase += deltaPhase;

// Detect and correct discontinuity at phase wrap
if (phase >= 1.f) {
    phase -= 1.f;
    float subsample = phase / deltaPhase;
    // Insert BLEP for sawtooth (magnitude -2.f drops from +1 to -1)
    sawMinBlep.insertDiscontinuity(subsample - 1.f, -2.f);
}

// Generate naive sawtooth
float saw = 2.f * phase - 1.f;
// Apply MinBLEP correction
saw += sawMinBlep.process();
// Remove DC offset
saw = dcFilter.process(dcFilterState, saw);
```

### Pattern 2: Square Wave with Pulse Width

**What:** Two discontinuities per cycle (rising and falling edges)

**When to use:** Square/pulse waveforms with variable duty cycle

**Example:**
```cpp
// Source: VCV Fundamental VCO pattern

float pulseWidth = 0.5f; // 50% duty cycle

// Detect phase crossing at 0 (rising edge)
if (phase >= 1.f) {
    phase -= 1.f;
    float subsample = phase / deltaPhase;
    sqrMinBlep.insertDiscontinuity(subsample - 1.f, 2.f); // Low to high
}

// Detect phase crossing pulse width (falling edge)
float phasePrev = phase - deltaPhase;
if (phasePrev < pulseWidth && phase >= pulseWidth) {
    float subsample = (pulseWidth - phasePrev) / deltaPhase;
    sqrMinBlep.insertDiscontinuity(subsample - 1.f, -2.f); // High to low
}

// Generate naive square
float sqr = (phase < pulseWidth) ? 1.f : -1.f;
sqr += sqrMinBlep.process();
```

### Pattern 3: Triangle Wave via Integration

**What:** Integrate antialiased square wave to get antialiased triangle

**When to use:** Triangle waveforms (avoids polyBLAMP complexity)

**Example (based on Befaco EvenVCO):**
```cpp
// Source: https://github.com/VCVRack/Befaco/blob/v0.6/src/EvenVCO.cpp

// Generate antialiased square first (see Pattern 2)
float square = /* ... antialiased square ... */;

// Integrate with leaky integrator (prevents DC drift)
float decay = 0.999f; // Very slow leak
triangle = triangle * decay + square * 4.f * freq * args.sampleTime;
```

### Pattern 4: Polyphonic Waveform Mixing

**What:** Multiple waveforms with individual volume controls, mixed per-voice

**When to use:** Multi-waveform oscillators like VCO1/VCO2 in this project

**Example:**
```cpp
// Per-voice mixing
float triVol = params[TRI_VOL_PARAM].getValue();
float sqrVol = params[SQR_VOL_PARAM].getValue();
float sinVol = params[SIN_VOL_PARAM].getValue();
float sawVol = params[SAW_VOL_PARAM].getValue();

// Mix waveforms (each already antialiased and DC-filtered)
float mixed = tri * triVol + sqr * sqrVol + sine * sinVol + saw * sawVol;

// Output individual waveforms AND mix
outputs[TRI_OUTPUT].setVoltage(tri * 5.f, channel);
outputs[MIX_OUTPUT].setVoltage(mixed * 5.f, channel);
```

### Anti-Patterns to Avoid

- **Double discontinuity insertion (VCV Fundamental bug):** When PWM is modulated, previous implementations would insert discontinuities on EVERY sample where pulseWidth changed, not just where phase CROSSES pulseWidth. This causes pops. **Solution:** Only insert discontinuity when `phasePrev < pw && phase >= pw` (actual crossing).

- **No DC filtering after MinBLEP:** MinBLEP correction can introduce small DC offsets, especially with integration methods. **Solution:** Always apply `TRCFilter::highpass()` with very low cutoff (e.g., 10 Hz).

- **Using std::sin for audio rate:** Too slow for polyphonic oscillators. **Solution:** Use `simd::sin()` for SSE optimization.

- **Fixed-point phase accumulator:** VCV Rack uses float everywhere; fixed-point adds complexity without benefit at 44.1kHz-192kHz sample rates. **Solution:** Use float, rely on proper wrapping (`phase -= 1.f` not `phase = fmod(phase, 1.f)`).

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Antialiasing | Custom polynomial or wavetable | `dsp::MinBlepGenerator` | Precomputed impulse response, battle-tested in Fundamental |
| DC offset removal | Manual high-pass IIR | `dsp::TRCFilter::highpass()` | Euler RC filter, numerically stable |
| Fast sine | Taylor series or lookup table | `simd::sin()` | SSE-optimized via sse_mathfun, 4-wide SIMD |
| Frequency from V/Oct | `pow(2, volts)` | `dsp::exp2_taylor5()` | 6e-06 error, optimized polynomial |
| Subsample discontinuity position | Linear interpolation | VCV Fundamental's `getCrossing()` lambda | Handles phase wrapping correctly |

**Key insight:** VCV Rack SDK provides highly-optimized DSP primitives. Custom implementations are slower and more bug-prone. The Fundamental VCO bug demonstrates even experienced developers make mistakes when implementing antialiasing from scratch.

## Common Pitfalls

### Pitfall 1: PWM Double Discontinuity Bug (VCV Fundamental Issue #140)

**What goes wrong:** Pops and clicks when modulating pulse width, especially with LFO or envelope

**Why it happens:** Code inserts MinBLEP discontinuity every time `pulseWidth` parameter changes, rather than only when phase *crosses* the pulse width threshold. This creates spurious discontinuities.

**How to avoid:**
```cpp
// WRONG (causes pops):
if (pulseWidth != prevPulseWidth) {
    sqrMinBlep.insertDiscontinuity(...);
}

// CORRECT (only insert when phase crosses):
if (phasePrev < pulseWidth && phase >= pulseWidth) {
    float subsample = (pulseWidth - phasePrev) / deltaPhase;
    sqrMinBlep.insertDiscontinuity(subsample - 1.f, -2.f);
}
```

**Warning signs:** Clicks audible when slowly sweeping pulse width knob, spectrum analyzer shows impulse spikes

**Sources:** [VCV Fundamental Issue #140](https://github.com/VCVRack/Fundamental/issues/140), [VCV Fundamental Issue #114](https://github.com/VCVRack/Fundamental/issues/114)

### Pitfall 2: Incorrect Subsample Position Calculation

**What goes wrong:** Aliasing artifacts remain despite using MinBLEP, or excessive high-frequency ringing

**Why it happens:** The subsample position passed to `insertDiscontinuity()` must be in range -1 < p <= 0 (relative to current frame). Off-by-one errors or using absolute phase cause MinBLEP to apply correction at wrong time.

**How to avoid:**
```cpp
// When phase wraps from >=1.0 to <1.0:
if (phase >= 1.f) {
    phase -= 1.f; // Wrap phase
    // Subsample = how far PAST 1.0 we went, as fraction of deltaPhase
    float subsample = phase / deltaPhase;
    // MinBLEP wants position relative to PREVIOUS sample: subsample - 1
    sawMinBlep.insertDiscontinuity(subsample - 1.f, -2.f);
}
```

**Warning signs:** High-frequency content at >10kHz when playing low notes, spectrum analyzer shows aliases "folding back"

### Pitfall 3: Triangle Wave Step Discontinuities

**What goes wrong:** Triangle wave has audible aliasing despite using MinBLEP for square wave integration method

**Why it happens:** Triangle has NO step discontinuities (continuous waveform), only *slope* discontinuities. MinBLEP corrects steps, not slopes. Need polyBLAMP or ensure square is perfectly antialiased before integration.

**How to avoid:** Use antialiased square integration method (Pattern 3). The integrated square's slope discontinuities become the triangle's corners, but integration inherently band-limits them.

**Warning signs:** Triangle sounds "buzzy" at high frequencies, spectrum shows odd harmonics with aliasing

### Pitfall 4: Phase Accumulator Drift

**What goes wrong:** Oscillator pitch drifts slightly over long periods (>10 minutes), or hard-synced oscillators slowly desync

**Why it happens:** Floating-point rounding errors accumulate if using `phase = fmod(phase, 1.f)` or similar. Float precision degrades far from zero.

**How to avoid:**
```cpp
// CORRECT: Subtract integer part, keeps phase near zero
if (phase >= 1.f) {
    phase -= 1.f; // or phase -= std::floor(phase) for large deltas
}

// WRONG: fmod has precision issues
phase = std::fmod(phase, 1.f);
```

**Warning signs:** Spectrum analyzer shows frequency slowly shifting over minutes, sync'd oscillators "beat" (phase drift visible)

**Sources:** [Building a Numerically Controlled Oscillator](https://zipcpu.com/dsp/2017/12/09/nco.html)

### Pitfall 5: Missing DC Filter After MinBLEP

**What goes wrong:** Oscillator output has small DC offset (0.01-0.1V), causes issues with VCAs and filters

**Why it happens:** MinBLEP corrections are band-limited approximations; numerical errors introduce DC. Integration methods (triangle) accumulate DC even faster.

**How to avoid:**
```cpp
// Always apply DC blocking highpass after MinBLEP
dsp::TRCFilter<float> dcFilter;
dcFilter.setCutoffFreq(10.f / args.sampleRate); // 10 Hz cutoff

float saw = /* ... antialiased sawtooth ... */;
saw = dcFilter.process(dcFilterState, saw);
```

**Warning signs:** Multimeter shows non-zero voltage on sustained note, filter self-oscillation frequency drifts

## Code Examples

Verified patterns from official sources:

### Complete MinBLEP Sawtooth Oscillator

```cpp
// Source: VCV Fundamental VCO v2 pattern
// https://github.com/VCVRack/Fundamental/blob/v2/src/VCO.cpp

struct SawtoothVoice {
    float phase = 0.f;
    dsp::MinBlepGenerator<16, 16, float> minBlep;
    dsp::TRCFilter<float> dcFilter;
    float dcFilterState = 0.f;

    void reset() {
        phase = 0.f;
        minBlep = dsp::MinBlepGenerator<16, 16, float>();
        dcFilter.reset();
        dcFilterState = 0.f;
    }

    float process(float freq, float sampleTime) {
        // Update phase
        float deltaPhase = freq * sampleTime;
        phase += deltaPhase;

        // Detect discontinuity at phase wrap
        if (phase >= 1.f) {
            phase -= 1.f;
            float subsample = phase / deltaPhase;
            // Sawtooth drops from +1 to -1, magnitude -2
            minBlep.insertDiscontinuity(subsample - 1.f, -2.f);
        }

        // Generate naive sawtooth [-1, +1]
        float saw = 2.f * phase - 1.f;

        // Apply MinBLEP correction
        saw += minBlep.process();

        // DC blocking (10 Hz highpass)
        dcFilter.setCutoffFreq(10.f / (1.f / sampleTime));
        saw = dcFilter.process(dcFilterState, saw);

        return saw;
    }
};
```

### Sine Wave (No Antialiasing Needed)

```cpp
// Source: VCV Rack Plugin Development Tutorial
// https://vcvrack.com/manual/PluginDevelopmentTutorial

// Sine is inherently band-limited (no harmonics to alias)
float sine = std::sin(2.f * M_PI * phase);

// For SIMD optimization (polyphonic):
simd::float_4 phase4 = /* ... 4 voices ... */;
simd::float_4 sine4 = simd::sin(2.f * M_PI * phase4);
```

### Square Wave with Pulse Width Modulation

```cpp
// Source: VCV Fundamental VCO v2 pattern (avoiding Issue #140 bug)

struct SquareVoice {
    float phase = 0.f;
    dsp::MinBlepGenerator<16, 16, float> minBlep;

    float process(float freq, float pulseWidth, float sampleTime) {
        float deltaPhase = freq * sampleTime;
        float phasePrev = phase;
        phase += deltaPhase;

        // Detect rising edge (phase wraps through 0)
        if (phase >= 1.f) {
            phase -= 1.f;
            float subsample = phase / deltaPhase;
            minBlep.insertDiscontinuity(subsample - 1.f, 2.f); // -1 to +1
        }

        // Detect falling edge (phase crosses pulse width)
        // CRITICAL: Only insert when phase CROSSES, not when pw changes
        if (phasePrev < pulseWidth && phase >= pulseWidth) {
            float subsample = (pulseWidth - phasePrev) / deltaPhase;
            minBlep.insertDiscontinuity(subsample - 1.f, -2.f); // +1 to -1
        }

        // Generate naive square
        float sqr = (phase < pulseWidth) ? 1.f : -1.f;
        sqr += minBlep.process();

        return sqr;
    }
};
```

### Triangle Wave via Square Integration

```cpp
// Source: Befaco EvenVCO pattern
// https://github.com/VCVRack/Befaco/blob/v0.6/src/EvenVCO.cpp

struct TriangleVoice {
    SquareVoice square; // Antialiased square generator
    float triState = 0.f;

    float process(float freq, float sampleTime) {
        // Get antialiased square wave
        float sqr = square.process(freq, 0.5f, sampleTime);

        // Integrate with leaky integrator (prevents drift)
        // Scale by 4 * freq to normalize amplitude
        triState = triState * 0.999f + sqr * 4.f * freq * sampleTime;

        return triState;
    }
};
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Oversampling | MinBLEP | VCV Fundamental v1→v2 | Lower CPU, same quality |
| PolyBLEP | MinBLEP | VCV ecosystem standard | MinBLEP preferred despite complexity |
| std::sin | simd::sin | VCV SDK 2.x | 4x faster (SSE), critical for 16-osc polyphony |
| Fixed-point phase | Float phase | VCV Rack design | Float sufficient at audio rates, simpler code |

**Deprecated/outdated:**
- **Oversampling for antialiasing:** VCV Fundamental switched from 2x oversample to MinBLEP due to CPU cost. MinBLEP is now standard.
- **Naive wavetables:** Without mip-mapping, wavetables alias badly above ~1kHz. MinBLEP is superior for classic waveforms.
- **Manual BLEP table generation:** VCV SDK provides `minBlepImpulse()` function. No need to compute Blackman-windowed sinc manually.

## Open Questions

Things that couldn't be fully resolved:

1. **PolyBLAMP for triangle slopes**
   - What we know: PolyBLAMP handles slope discontinuities, triangle has slopes at 0.25 and 0.75 phase
   - What's unclear: No VCV Rack modules use polyBLAMP in source code examined; all use square integration instead
   - Recommendation: Use square integration method (Pattern 3) as it's proven in Befaco EvenVCO. PolyBLAMP is research curiosity, not production pattern for VCV.

2. **Optimal MinBLEP parameters (Z and O values)**
   - What we know: VCV Fundamental uses Z=16, O=16. Befaco EvenVCO uses Z=16, O=32. Community discussions mention Z=48, O=64 for "best quality."
   - What's unclear: No published comparison of aliasing performance vs CPU cost for different parameters in VCV Rack context
   - Recommendation: Start with Z=16, O=16 (Fundamental standard). Profile CPU, increase if budget allows. Higher values = better aliasing rejection but more CPU.

3. **DC filter cutoff frequency**
   - What we know: VCV Fundamental applies DC filtering. Community suggests 10 Hz. Too high cuts bass, too low allows drift.
   - What's unclear: Exact cutoff used by Fundamental (code shows `dcFilter` but doesn't set frequency explicitly in examined sections)
   - Recommendation: Use 10 Hz (10.f / sampleRate) as safe default. User cannot hear below ~20 Hz, so 10 Hz removes DC without audible effect.

## Sources

### Primary (HIGH confidence)
- VCV Rack SDK 2.6.6 headers:
  - `/include/dsp/minblep.hpp` - MinBlepGenerator template, minBlepImpulse()
  - `/include/dsp/common.hpp` - FREQ_C4, sinc()
  - `/include/dsp/filter.hpp` - TRCFilter for DC blocking
  - `/include/dsp/approx.hpp` - exp2_taylor5()
  - `/include/simd/functions.hpp` - simd::sin() SSE implementation
- VCV Fundamental VCO v2 source (via WebFetch): MinBLEP usage patterns, Z=16 O=16, discontinuity detection
- Befaco EvenVCO source (via WebFetch): Triangle via square integration, Z=16 O=32

### Secondary (MEDIUM confidence)
- [VCV Rack DSP Manual](https://vcvrack.com/manual/DSP) - Official documentation on DSP patterns
- [VCV Plugin Development Tutorial](https://vcvrack.com/manual/PluginDevelopmentTutorial) - Sine wave example
- [Submarine technical articles](https://community.vcvrack.com/t/submarine-technical-articles/8770) - SSE sine approximation discussion
- [Martin Finke PolyBLEP Tutorial](https://www.martin-finke.de/articles/audio-plugins-018-polyblep-oscillator/) - PolyBLEP polynomial formula (alternative to MinBLEP)
- [Building a Numerically Controlled Oscillator](https://zipcpu.com/dsp/2017/12/09/nco.html) - Phase accumulator precision and drift

### Tertiary (LOW confidence)
- [KVR Audio: Oscillator antialiasing discussion](https://www.kvraudio.com/forum/viewtopic.php?t=437116) - Community comparison of polyBLEP vs minBLEP, notes polyBLEP "sounds duller"
- [KVR Audio: minBLEPs discussion](https://www.kvraudio.com/forum/viewtopic.php?t=364256) - Zero-crossings parameter choices
- [VCV Community: Aliasing in Fundamental VCO](https://community.vcvrack.com/t/aliasing-in-fundamental-vco-saw-output/14862) - User reports of aliasing bugs
- [Bogaudio ANALYZER](https://library.vcvrack.com/Bogaudio/Bogaudio-Analyzer) - Spectrum analyzer for verification

### Critical Bug References
- [VCV Fundamental Issue #140](https://github.com/VCVRack/Fundamental/issues/140) - "**SOLUTION** Fundamental VCO Pops and Clicks from PWM" - Double discontinuity bug
- [VCV Fundamental Issue #114](https://github.com/VCVRack/Fundamental/issues/114) - "VCO-1 PWM produces clicks/artifacts" - Original bug report
- [VCV Fundamental Issue #134](https://github.com/VCVRack/Fundamental/issues/134) - "Fundamental VCO outputs much more aliasing in analog mode" - Aliasing bug reports

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - VCV Rack SDK dsp::MinBlepGenerator is authoritative, confirmed in multiple production modules
- Architecture: HIGH - Patterns extracted from VCV Fundamental v2 and Befaco EvenVCO source code
- Pitfalls: HIGH - PWM bug documented in GitHub issues with reproduction steps, phase drift well-understood from NCO literature
- PolyBLEP alternative: MEDIUM - Tutorial sources are authoritative, but VCV ecosystem preference for MinBLEP is observed, not documented
- Optimal parameters (Z, O): LOW - No systematic comparison found, relying on observed values in production code

**Research date:** 2026-01-22
**Valid until:** 2026-03-22 (60 days - VCV Rack SDK is stable, DSP fundamentals don't change rapidly)
