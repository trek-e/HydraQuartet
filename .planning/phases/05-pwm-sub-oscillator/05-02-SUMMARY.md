# Plan 05-02 Summary: Sub-Oscillator

**Status:** Complete
**Verified:** 2026-01-23

---

## What Was Built

Sub-oscillator tracking VCO1 at -1 octave with switchable waveform and dedicated output:

1. **Sub-oscillator parameters:**
   - `SUB_WAVE_PARAM` - Toggle switch (0=Square, 1=Sine)
   - `SUB_LEVEL_PARAM` - Mix level control (0-100%)

2. **Sub-oscillator output:**
   - `SUB_OUTPUT` - Dedicated polyphonic output jack

3. **DSP implementation:**
   - Tracks `basePitch + octave1 - 1.f` (VCO1 base minus 1 octave)
   - Ignores VCO1 detune for stable foundation
   - Simple phase accumulator (no MinBLEP - sub frequencies low enough)
   - Square via `simd::ifelse(phase < 0.5f, 1.f, -1.f)`
   - Sine via `simd::sin(2.f * M_PI * phase)`

4. **Widget controls:**
   - RoundBlackKnob for SUB_LEVEL at (10.16, 88.0)
   - CKSS toggle switch for SUB_WAVE at (25.4, 88.0)
   - PJ301MPort output jack for SUB_OUTPUT at (40.64, 88.0)

---

## Key Decisions

- **Sub ignores detune:** Uses `basePitch + octave1 - 1.f` not `pitch1 - 1.f` so sub provides stable foundation even when VCO1 is detuned for thickness
- **No MinBLEP on sub:** At -1 octave, frequencies are low enough that aliasing is not a concern for square wave
- **Output scaling:** SUB_OUTPUT at ±2V (reduced from ±5V during testing phase)
- **Mix integration:** Sub added to main mix with `+ subOut * subLevel` before output scaling

---

## Files Modified

- `src/HydraQuartetVCO.cpp` - Sub-oscillator params, DSP, output, and widget controls

---

## Verification Results

Human verified:
- Sub-oscillator is exactly 1 octave below VCO1 fundamental
- Waveform switch toggles between square (buzzy) and sine (smooth)
- Sub does NOT beat against VCO2 when VCO1 detune is applied
- SUB_OUTPUT provides polyphonic signal
- Clean mix when sub is blended with main VCOs

---

## Requirements Completed

- WAVE-04: VCO1 has sub-oscillator output (one octave below) - COMPLETE

---

*Completed: 2026-01-23*
