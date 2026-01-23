# Phase 1: Foundation & Panel - Context

**Gathered:** 2026-01-22
**Status:** Ready for planning

<domain>
## Phase Boundary

Development environment setup, panel design (30+ HP), and basic polyphonic module infrastructure. This phase delivers a working module shell with proper V/Oct tracking and polyphonic I/O — no oscillator DSP yet (that's Phase 2).

</domain>

<decisions>
## Implementation Decisions

### Panel Layout
- VCO1 on left, VCO2 on right (side by side)
- Global controls (outputs, main CV inputs) in center column between VCO sections
- Signal flow top to bottom within each VCO section (pitch at top, waveforms middle, modulation at bottom)
- Module width: 36 HP

### Visual Style
- Industrial/technical aesthetic — dark, utilitarian, professional studio gear feel
- Dark blue/navy background
- Pointer knobs (line indicator showing position)
- Color-coded labels by function (different colors for pitch, waveform, modulation sections)

### Control Grouping
- Four waveform volume controls arranged in horizontal row (Tri-Sq-Sin-Saw)
- VCO1 and VCO2 sections mirror each other (same positions for shared controls — easy to learn)
- Pitch controls (octave switch, detune/fine) at top of each VCO section
- Modulation controls (PWM, sync, FM) at bottom of each VCO section

### Module Identity
- Oscillator sections labeled "VCO 1" and "VCO 2"
- Minimal logo/branding — small text, doesn't dominate

### Claude's Discretion
- Module name and plugin brand name
- Exact color palette for function color-coding
- Font choice for labels
- Specific knob graphics (within pointer style)
- Jack/port styling

</decisions>

<specifics>
## Specific Ideas

- Industrial/technical look inspired by professional studio gear
- Mirror layout between VCO1 and VCO2 for learning curve — shared controls in same positions
- Color-coding helps users quickly identify control types (pitch vs waveform vs modulation)

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-foundation-panel*
*Context gathered: 2026-01-22*
