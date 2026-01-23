# Phase 5: PWM & Sub-Oscillator - Context

**Gathered:** 2026-01-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Pulse width modulation controls for both VCOs and a sub-oscillator output for VCO1. This completes basic dual-VCO functionality before advanced modulation phases (FM, sync). PWM affects square wave output and should be designed with future XOR interaction (Phase 8) in mind.

</domain>

<decisions>
## Implementation Decisions

### PWM Range & Behavior
- Full range: 1% - 99% pulse width available
- Default position: 50% (classic square wave, symmetrical)
- Knob curve: Linear mapping, 1:1 feel across range
- Design with XOR interaction in mind from the start (Phase 8 will use PWM values)

### PWM CV Response
- Bipolar CV input: -5V to +5V sweeps both directions around knob position
- Attenuverter on each PWM CV input: scale and invert incoming CV (-100% to +100%)
- Full modulation depth: +5V at 100% attenuverter sweeps from knob to max, -5V to min
- Separate CV inputs per VCO: VCO1 and VCO2 each have independent PWM CV

### Sub-Oscillator Character
- Waveform: Switchable between square and sine via toggle switch
- Output: Both level knob (mixed into main output) AND dedicated SUB output jack
- Pitch tracking: Claude's discretion (suggest: selectable -1 or -2 octaves below VCO1)
- Detune inheritance: Claude's discretion (suggest: ignore detune for stable foundation)

### Panel Layout
- PWM knobs: Group in a modulation section (not near waveform outputs)
- Sub controls: In VCO1 section since sub tracks VCO1
- Sub waveform switch: Toggle switch (physical switch appearance)
- CV indicators: LED rings on PWM CV inputs showing modulation activity

### Claude's Discretion
- Sub-oscillator octave selection (-1 or -2, or fixed at -1)
- Whether sub inherits VCO1 detune or stays at exact pitch
- Exact LED ring behavior and colors
- Attenuverter knob sizing relative to main PWM knob

</decisions>

<specifics>
## Specific Ideas

- PWM should work at extremes (1-99%) without audio problems - DC filtering already in place from Phase 2
- Designing for XOR now means PWM values need to be accessible for Phase 8 (internal state, not just audio output)
- Sub with both internal mix and dedicated output gives flexibility for external filtering/processing

</specifics>

<deferred>
## Deferred Ideas

None - discussion stayed within phase scope

</deferred>

---

*Phase: 05-pwm-sub-oscillator*
*Context gathered: 2026-01-23*
