# HydraQuartet VCO — Architecture Guide

This document describes the internal DSP architecture for developers who want to understand, modify, or learn from the codebase.

## File Structure

```
src/
├── plugin.hpp          # Plugin-level declarations, Model extern
├── plugin.cpp          # Plugin initialization, model registration
└── HydraQuartetVCO.cpp # All module code (918 lines)
res/
└── HydraQuartetVCO.svg # 40 HP panel (203.2mm × 128.5mm)
```

Everything lives in a single `.cpp` file. The module is self-contained with no external dependencies beyond the VCV Rack SDK.

## Core Types

### MinBlepTable (static singleton)
```cpp
struct MinBlepTable {
    float impulse[2 * 16 * 16 + 1];  // 16 zero crossings, 16× oversampling
};
static MinBlepTable minBlepTable;
```
Generated once at plugin load via `dsp::minBlepImpulse()`. Shared by all module instances.

### MinBlepBuffer\<N\> (per-waveform, per-SIMD-group)
```cpp
template <int N>  // N=32 in practice
struct MinBlepBuffer {
    float_4 buffer[2 * N];
    int pos;

    void insertDiscontinuity(float p, float x, int lane);
    float_4 process();
};
```
Stores pending MinBLEP corrections for 4 SIMD lanes in interleaved layout. Each waveform type gets its own buffer per SIMD group to avoid interference.

### VcoEngine (per-oscillator)
```cpp
struct VcoEngine {
    float_4 phase[4];           // Phase accumulators (4 groups × 4 lanes = 16 voices max)
    float_4 oldPhase[4];        // Previous phase (for edge detection)
    float_4 deltaPhase[4];      // Phase increment (for subsample calculation)
    MinBlepBuffer<32> sawMinBlepBuffer[4];
    MinBlepBuffer<32> sqrMinBlepBuffer[4];
    MinBlepBuffer<32> triMinBlepBuffer[4];
    MinBlepBuffer<32> xorMinBlepBuffer[4];

    void process(int g, float_4 freq, float sampleTime, float_4 pwm,
                 float_4& saw, float_4& sqr, float_4& tri, float_4& sine,
                 int& wrapMask,
                 float_4 sqr1Input = float_4(0.f),
                 float_4* xorOut = nullptr);

    void applySync(int g, int syncMask, ...);
};
```

Encapsulates all per-oscillator DSP state. The module has two instances: `vco1` and `vco2`.

**Key design decision:** XOR MinBLEP tracking is split between VcoEngine and module level. VCO2's engine tracks VCO2 square edges for XOR, while VCO1 square edges are tracked in a separate module-level `xorFromVco1MinBlep[4]` buffer. This is because VCO1's process() runs before VCO2's, and the XOR needs edges from both.

## Processing Pipeline

Per sample, per SIMD group (4 voices at a time):

```
1. Read parameters (once per sample, outside SIMD loop)
   ├── Octave switches → V/Oct offsets
   ├── Detune knob → V/Oct cents conversion (50/1200)
   ├── Fine tune → dual-range semitone conversion
   ├── Waveform volumes (knob or CV-replaces-knob)
   └── PWM, FM, vibrato amounts

2. SIMD loop: for (c = 0; c < channels; c += 4)
   │
   ├── 2a. Pitch calculation
   │   ├── basePitch = V/Oct input + parameter offsets
   │   ├── freq1 = FREQ_C4 * exp2_taylor5(pitch1 + detune + vibrato)
   │   └── freq2base = FREQ_C4 * exp2_taylor5(pitch2 + fine + vibrato)
   │
   ├── 2b. FM application (linear, post-exponential)
   │   ├── Select modulator waveform from VCO1 (based on FM Source switch)
   │   ├── fmDepth = FM knob + FM CV (polyphonic)
   │   ├── freq2 = freq2base + (modulator * fmDepth * freq2base)
   │   └── freq2 = clamp(freq2, 0.1 Hz, ∞)  // quasi through-zero
   │
   ├── 2c. VCO1 waveform generation
   │   ├── vco1.process() → saw1, sqr1, tri1, sine1
   │   ├── Sub-oscillator (separate phase at ½ frequency)
   │   └── Record wrapMask for sync
   │
   ├── 2d. VCO2 waveform generation
   │   ├── vco2.process(sqr1, &xorOut) → saw2, sqr2, tri2, sine2, xorOut
   │   └── Record wrapMask for sync
   │
   ├── 2e. Hard sync (if enabled)
   │   ├── If sync1 active: vco1.applySync() on vco2 wrap events
   │   ├── If sync2 active: vco2.applySync() on vco1 wrap events
   │   └── Sync regenerates affected waveforms with MinBLEP correction
   │
   ├── 2f. XOR MinBLEP from VCO1 edges
   │   ├── Track VCO1 wrap edges → xorFromVco1MinBlep discontinuity
   │   ├── Track VCO1 PWM fall edges → xorFromVco1MinBlep discontinuity
   │   └── Apply: xorOut += xorFromVco1MinBlep.process()
   │
   ├── 2g. Waveform mixing
   │   ├── mix = Σ(waveform × volume) for all active waveforms
   │   ├── Volumes are float_4 (per-voice when CV is patched)
   │   └── Scale by outputScale (1/3, assuming ~3 active waveforms)
   │
   └── 2h. Output stage (per lane)
       ├── DC filter: TRCFilter at 10 Hz
       ├── Soft clip: 3.0 * tanh(signal / 3.0)
       ├── Scale: × 2.0 (output range ±2V nominal)
       ├── Sanitize: NaN/Inf → 0
       └── Write to polyphonic output + per-voice outputs

3. Post-loop
   ├── Set output channel counts
   ├── Write mix output (mono sum)
   ├── Write gate outputs (pass-through)
   └── Update LED brightness (PWM/FM CV activity)
```

## MinBLEP Edge Tracking

### Sawtooth (1 edge type)
Phase wrap (0.999... → 0.0): discontinuity magnitude = −2.0

### Square (2 edge types)
1. **Rising edge** on phase wrap: +2.0
2. **Falling edge** on PWM threshold crossing: −2.0

### XOR (4 edge types)
Since `XOR = sqr1 × sqr2`, a transition occurs whenever **either** square wave transitions:

1. **VCO1 wrap** (sqr1: −1→+1): XOR discontinuity = `+2 × sqr2`
2. **VCO1 PWM fall** (sqr1: +1→−1): XOR discontinuity = `−2 × sqr2`
3. **VCO2 wrap** (sqr2: −1→+1): XOR discontinuity = `+2 × sqr1`
4. **VCO2 PWM fall** (sqr2: +1→−1): XOR discontinuity = `−2 × sqr1`

Edge types 3–4 are tracked in VcoEngine (VCO2 instance). Edge types 1–2 are tracked in module-level buffers because VCO1 processes before VCO2.

### Subsample Position Calculation
For a phase wrap at threshold `t`:
```cpp
float subsample = (t - oldPhase) / deltaPhase - 1.0f;
// Result is in range (-1, 0], where 0 = exactly on sample boundary
```

## Sync Implementation

Hard sync uses a two-phase approach:

1. **`process()`** — generates waveforms normally, records wrap events
2. **`applySync()`** — called after both VCOs process, resets slave phase and regenerates waveforms with MinBLEP correction for the sync discontinuity

This separation ensures that sync discontinuities are correctly antialiased. The slave's waveform outputs are updated by reference, so there's no one-sample delay.

## CV-Replaces-Knob Pattern

```cpp
// Outside SIMD loop
float saw1Knob = params[SAW1_PARAM].getValue();
bool saw1CVConnected = inputs[SAW1_CV_INPUT].isConnected();

// Inside SIMD loop
float_4 saw1Vol;
if (saw1CVConnected) {
    saw1Vol = simd::clamp(inputs[SAW1_CV_INPUT].getPolyVoltageSimd<float_4>(c), 0.f, 10.f);
} else {
    saw1Vol = float_4(saw1Knob);
}
```

Connection check is outside the loop for efficiency (same cable state across all channels). The `getPolyVoltageSimd()` call handles mono→poly broadcasting automatically.

## Memory Layout

Each VcoEngine contains:
- 4 × `float_4` phase arrays = 64 bytes
- 4 × `float_4` oldPhase arrays = 64 bytes
- 4 × `float_4` deltaPhase arrays = 64 bytes
- 4 × `MinBlepBuffer<32>` per waveform type × 4 types = 16 buffers × 256 bytes = 4096 bytes
- **Total per VcoEngine: ~4.3 KB**

Two engines (VCO1 + VCO2) + module-level XOR buffers ≈ **9.6 KB** of hot DSP state.

All state is stack-allocated (no heap allocations in the audio path).
