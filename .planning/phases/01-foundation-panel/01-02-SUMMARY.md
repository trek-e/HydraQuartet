# Plan 01-02 Summary: Polyphonic Module Implementation

**Completed:** 2026-01-23
**Duration:** ~15 minutes (including user verification)

## What Was Built

A fully functional polyphonic VCO module with 8-voice support, accurate V/Oct tracking, and both polyphonic and mono mix outputs.

### Core Features Implemented
- **Polyphonic V/Oct input** - reads up to 16 channels via SIMD float_4
- **Phase accumulator oscillator** - placeholder sine wave (Phase 2 adds antialiased waveforms)
- **Polyphonic audio output** - 8-channel output matching input channel count
- **Mix output** - normalized mono sum (divides by voice count for consistent volume)
- **SIN1 volume knob** - functional volume control demonstrating parameter connectivity

### Technical Implementation
- Uses `dsp::FREQ_C4` (261.6256 Hz) and `dsp::exp2_taylor5()` for accurate V/Oct conversion
- SIMD processing in groups of 4 channels for efficiency
- Phase wrapping via `simd::floor()` for stability
- Proper `setChannels()` call on polyphonic output

## Key Decisions Made

1. **SIN1 knob functional early** - Added volume control to verify knob connectivity during Phase 1 testing

2. **Mix output averaging** - Divides sum by channel count to maintain consistent volume regardless of voice count (prevents clipping with many voices)

3. **Panel label font sizes** - Increased from 2-2.5px to 3-3.5px for readability (note: text must be converted to paths in Inkscape for VCV Rack rendering)

## Artifacts Created/Modified

| File | Changes |
|------|---------|
| `src/TriaxVCO.cpp` | Added process() with SIMD polyphonic processing, SIN1 volume control |
| `res/TriaxVCO.svg` | Increased label font sizes, aligned component positions |

## Verification Results

| Requirement | Status |
|-------------|--------|
| V/Oct tracking (0V = C4) | PASS - confirmed by user |
| SIN knob changes volume | PASS - confirmed by user |
| 8-voice polyphony | PASS - confirmed by user |
| Mix output (mono sum) | PASS - confirmed by user |
| Module loads in VCV Rack | PASS |

## Known Issues

1. **Panel labels not visible** - SVG `<text>` elements don't render in VCV Rack. User needs to convert text to paths in Inkscape (Path → Object to Path)

2. **Poly output louder than mix** - Expected behavior. Poly output is 5V per voice; when summed by audio output module, 8 voices = 40V peak. Mix output normalizes to ~5V.

## Notes for Phase 2

Phase 2 (Core Oscillator with Antialiasing) will:
1. Replace placeholder sine with proper antialiased waveforms (tri, sqr, sin, saw)
2. Implement polyBLEP for sawtooth and square waves
3. Connect all VCO1 waveform volume knobs
4. Add waveform mixing

### Current Parameter Connectivity
- SIN1_PARAM: Connected (volume control)
- All other params: Defined but not yet connected to DSP
