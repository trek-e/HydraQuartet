# HydraQuartet VCO — User Manual

## Quick Start

1. Add **HydraQuartet VCO** to your patch (find it under Oscillator → Polyphonic)
2. Patch a **V/Oct** source (keyboard, sequencer) into the V/Oct input
3. Patch the **Audio** output to a mixer or audio device
4. You should hear VCO1's sine wave. Turn up other waveform knobs to mix.

The module defaults to VCO1 Sine at volume 1 and VCO2 Square at volume 1 — a simple starting point.

---

## Understanding the Dual-VCO Architecture

Each voice in HydraQuartet contains **two independent oscillators** (VCO1 and VCO2). Both receive the same V/Oct pitch, but you can:

- **Detune** VCO1 against VCO2 for chorusing and thickness
- **Shift octaves** independently using the Octave knobs
- **Fine-tune** VCO2 for precise FM frequency ratios
- **Cross-modulate** via FM, sync, and XOR

The waveform outputs from both VCOs are summed together into a single per-voice mix. Each waveform has its own volume knob (0–10), so you can blend any combination of triangle, square, sine, and sawtooth from both oscillators.

---

## VCO1 Controls (Left Section)

### Detune (0–50 cents)
Shifts VCO1 sharp relative to VCO2. At 0, both VCOs are in perfect tune. Increasing detune creates a chorusing/beating effect. VCO1 is the one that detunes — VCO2 stays at reference pitch.

### Octave / Pipe Length (16' 8' 4' 2' 1')
Selects the octave using organ footage notation:
- **16'** = −2 octaves
- **8'** = −1 octave
- **4'** = concert pitch (default)
- **2'** = +1 octave
- **1'** = +2 octaves

### FM Source (Sine / Triangle / Saw / Square / Sub)
Selects which VCO1 waveform is used as the FM modulator for VCO2. Different waveforms produce different FM timbres:
- **Sine** — cleanest FM, classic DX-style
- **Triangle** — similar to sine but with odd harmonics
- **Saw** — brighter, more complex FM spectra
- **Square** — harsh, metallic FM tones
- **Sub** — FM at half the frequency, deeper modulation

### Waveform Volumes (Triangle, Square, Sine, Sawtooth)
Each waveform runs simultaneously. Set volume to 0 to mute, up to 10 for maximum. All four can be active at once — they're additive, not a crossfade.

### PWM (0–100%)
Controls the pulse width of the square wave. 50% = perfect square wave. Lower values create increasingly nasal tones. The PWM affects the XOR output as well.

### Vibrato (0–100%)
Built-in vibrato using a ~5.5 Hz sine LFO. Depth is controlled by this knob. For more complex vibrato, use an external LFO patched to V/Oct.

### Sub-Oscillator
- **Sub Level** (0–10) — Volume of the sub-oscillator, which tracks VCO1 at exactly one octave below
- **Sub Wave** (toggle) — Square or Sine waveform selection
- The sub has a **dedicated output jack** for external processing, plus it's included in the main mix

---

## VCO2 Controls (Right Section)

### FM Amount (0–10)
Controls the depth of through-zero linear frequency modulation from VCO1 → VCO2.

**Key behavior:** When both VCOs are at the same pitch and you increase FM Amount, you get **waveshaping in tune** — the timbre changes but the pitch does not drift. This is the defining characteristic of through-zero FM (as opposed to standard exponential FM which causes pitch drift).

### Fine Tune (0–10)
A dual-range tuning control:
- **0 to 5** — 0 to +1 semitone (smooth, continuous) — for fine detuning
- **5 to 10** — +2 to +12 semitones (stepped) — for dialing in FM harmonic ratios

This dual range lets you use the same knob for both subtle detuning and precise interval selection.

### Octave / Pipe Length (16' 8' 4' 2')
Same as VCO1 but with range 16' to 2' (no 1' position).

### XOR Volume (0–10)
Controls the volume of the XOR ring modulation output — the product of VCO1's square wave and VCO2's square wave (`sqr1 × sqr2`). Defaults to 0 (off).

The XOR character changes when you:
- Adjust PWM on either VCO
- Detune the VCOs relative to each other
- Apply sync

### Other VCO2 controls
PWM, Vibrato, and waveform volumes work identically to VCO1.

---

## Global Controls (Center Section)

### Sync Switches

Two 3-position horizontal switches control oscillator synchronization:

| Position | Sync 1 (VCO1→VCO2) | Sync 2 (VCO2→VCO1) |
|----------|---------------------|---------------------|
| Left | Hard sync | Hard sync |
| Center | Off (default) | Off (default) |
| Right | Soft sync | Soft sync |

**Hard sync** resets the slave oscillator's phase when the master oscillator completes a cycle. This produces the classic sync sweep sound — sweep the slave's pitch for dramatic harmonic changes.

Both syncs can be active simultaneously (cross-sync), which creates unstable, chaotic textures.

---

## CV Inputs

### PWM CV (per VCO)
Bipolar CV input with built-in attenuverter. The scaling is:
```
PWM = knob + (CV × attenuverter × 0.1)
```
At ±5V input with full attenuverter, you get ±0.5 pulse width modulation — enough for a full sweep from near-0% to near-100%.

A green LED next to each PWM CV input shows modulation activity.

### FM CV
Polyphonic or monophonic CV input for FM depth modulation. Auto-detects mono/poly:
- Mono input broadcasts to all voices
- Poly input gives per-voice FM depth control

Scaling: `±5V × attenuverter × 0.1 = ±0.5 contribution to FM depth`

### Waveform Volume CVs (6 inputs)
Six polyphonic CV inputs for per-voice volume automation:

| Input | Controls |
|-------|----------|
| SUB CV | Sub-oscillator volume |
| SQR1 CV | VCO1 square volume |
| SAW1 CV | VCO1 sawtooth volume |
| SAW2 CV | VCO2 sawtooth volume |
| SQR2 CV | VCO2 square volume |
| XOR CV | XOR ring mod volume |

**CV-replaces-knob behavior:** When a cable is patched, the corresponding knob is bypassed. The CV voltage maps directly: 0V = 0 volume, 10V = 10 volume (clamped).

---

## Outputs

### Audio (Polyphonic)
Main output carrying all voices as polyphonic channels. The channel count matches the V/Oct input.

### Mix (Mono)
Mono sum of all voices — useful for quick monitoring or mono patches.

### Sub Output (Polyphonic)
Dedicated sub-oscillator output, separate from the main mix. Use this to process the sub independently (e.g., through a separate filter).

### Voice 1–8 (Mono)
Individual per-voice outputs. Each carries the mixed VCO1+VCO2 signal for that voice. Useful for per-voice processing or spatial placement.

### Gate 1–8 + Gate Mix (Mono)
Gate pass-through outputs. Each gate output echoes the corresponding channel from the Gate input. Gate Mix is the OR (maximum) of all gates.

---

## Technical Notes

### Polyphony
The number of voices is determined by the **V/Oct input** channel count. If V/Oct has 4 channels, the module produces 4 voices. Maximum 16 channels (limited by VCV Rack cable standard), but the module is optimized for 8 voices.

### CPU Usage
Typical usage at 48 kHz sample rate:
- 1 voice: ~0.3%
- 4 voices: ~0.8%
- 8 voices: ~1.6%
- 16 voices: ~3.2%

### Sample Rate Independence
All DSP uses `args.sampleTime` for rate-independent processing. The module works correctly at any sample rate VCV Rack supports (44.1 kHz to 768 kHz).

### DC Offset
A DC blocking filter (10 Hz high-pass via `dsp::TRCFilter`) removes any DC offset from the mixed output. This handles DC content from XOR at unison, asymmetric PWM, and other sources.

### Output Headroom
The output stage uses `tanh()` soft clipping. Signal compression begins gradually above ±3V and reaches hard limiting near ±5V. With a single waveform active, there's no clipping. With many waveforms active, the saturation keeps things musical.
