# Phase 8: XOR Waveshaping - Context

**Gathered:** 2026-01-23
**Status:** Ready for planning

---

## Phase Boundary

XOR output combining pulse waves from VCO1 and VCO2, with PWM interaction and polyphonic CV control for waveform volumes. Final phase completing v1 requirements.

---

## Decisions Made

### 1. XOR Signal Generation
**Decision:** Analog-style ring modulation (sqr1 * sqr2)

True ring modulation approach: multiply the two square waves.
- +1 * +1 = +1
- +1 * -1 = -1
- -1 * -1 = +1

Full MinBLEP antialiasing with dedicated xorMinBlepBuffer. Track both edges (when VCO1 OR VCO2 square transitions).

**Implementation details:**
- XOR calculation happens inside VcoEngine (not main process loop)
- Own MinBLEP buffer, not shared with sqr2
- DC filter on XOR output (when oscillators lock, multiplication produces DC)
- No sync interaction — just multiplies current square values regardless of sync state

### 2. XOR Output Behavior
**Decision:** Mixed with VCO2, not dedicated jack

- XOR becomes VCO2's fifth waveform (tri2 + sqr2 + sin2 + saw2 + xor)
- Default level: Off (0) — user turns it up when wanted
- Same 0-10 volume range as other waveforms
- Included in mono mix output

### 3. Waveform CV Inputs
**Decision:** 6 polyphonic CV inputs, no attenuverters, no LEDs

CV inputs for selected waveforms only:
- VCO1 side: SAW1, SQR1, SUB
- VCO2 side: XOR, SQR2, SAW2

Behavior:
- 0-10V = 0-10 volume (direct 1:1 mapping)
- CV replaces knob when patched (not additive)
- Positioned directly below corresponding volume knobs

Triangle and Sine keep their volume knobs but get no CV inputs.

### 4. Panel Expansion
**Decision:** Expand to 40HP if needed

Panel can widen from 36HP up to 40HP to accommodate the 6 new CV jacks organized by VCO side.

### 5. PWM Interaction
**Decision:** Natural interaction, full dynamic response

- PWM changes on both VCOs affect XOR character naturally
- XOR responds to PWM CV modulation in real-time
- Phase cancellation effects when both PWMs modulated by same source — embrace it
- Full MinBLEP tracking for PWM threshold crossings (not just frequency edges)

### 6. Output Headroom
**Decision:** Soft clipping on outputs

Apply gentle saturation to prevent harsh digital clipping when many waveforms are active simultaneously.

### 7. Octave Switch Labeling
**Decision:** Organ pipe feet notation (Moog style)

Octave switches labeled: 16', 8', 4', 2' (feet notation like traditional organs/Moog synthesizers)

### Claude's Discretion
- How to pass sqr1 to VCO2 engine for XOR calculation (parameter vs separate method)
- Where xorMinBlepBuffer lives (VCO2's VcoEngine or module level)
- Exact soft clipping algorithm

---

## Specific Ideas

- "Similar to Moog style" for octave switch labeling
- Ring modulation character (sqr1 * sqr2) rather than digital XOR formula

---

## Deferred Ideas

None — discussion stayed within phase scope

---

*Phase: 08-xor-waveshaping*
*Context gathered: 2026-01-23*
