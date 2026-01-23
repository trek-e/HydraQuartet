# Pitfalls Research: VCV Rack Polyphonic Oscillator Module

**Domain:** VCV Rack polyphonic dual-VCO module development
**Researched:** 2026-01-22
**Project:** 8-voice polyphonic dual-VCO with cross-modulation (16 oscillators total)

## Executive Summary

Building polyphonic oscillator modules for VCV Rack presents unique challenges across five critical domains: DSP anti-aliasing, polyphony architecture, performance optimization, platform integration, and UI/UX design. The most critical pitfalls stem from improper anti-aliasing (causing harsh artifacts), incorrect polyphony channel handling (breaking user patches), and threading violations (causing crashes). This research identifies 25+ specific pitfalls with actionable prevention strategies mapped to development phases.

**Confidence Level:** HIGH for DSP and polyphony pitfalls (verified with official docs), MEDIUM for performance pitfalls (community-sourced), MEDIUM for platform-specific issues (mixed sources).

---

## Critical Pitfalls

These mistakes cause rewrites, major functionality issues, or catastrophic failures.

### Pitfall 1: Missing Anti-aliasing on Discontinuous Waveforms

**What goes wrong:** Sawtooth and square waves without bandlimiting produce harsh, metallic aliasing artifacts that fold back into the audible spectrum. Users immediately notice and perceive the module as low-quality.

**Why it happens:** Developers assume simple waveform generation is sufficient, or misunderstand when anti-aliasing is necessary. The VCV Rack manual explicitly warns: "If [a signal] is not [bandlimited], reconstructing will result in an entirely different signal, which usually sounds ugly."

**Consequences:**
- Harsh, inharmonic overtones that increase with pitch
- Module unusable for musical purposes above ~1kHz
- Negative user reviews and reputation damage
- Requires complete oscillator core rewrite to fix

**Prevention:**
- Use minBLEP or polyBLEP for sawtooth/square discontinuities
- Use polyBLAMP for triangle waves (integrate minBLEP square)
- Test with spectrum analyzer at high frequencies (>2kHz)
- Reference VCV Fundamental VCO implementation

**Detection:**
- Visual inspection with spectrum analyzer showing inharmonic content
- Audible harshness increasing with pitch
- User reports of "digital" or "aliased" sound

**Phase to address:** Phase 1 (Core oscillator implementation) - Must be part of initial architecture, extremely difficult to retrofit.

**Sources:**
- [VCV Rack DSP Manual](https://vcvrack.com/manual/DSP)
- Community: Help me understand minBlep implementation

---

### Pitfall 2: Incorrect minBLEP Implementation (Double Discontinuity Bug)

**What goes wrong:** The VCV Fundamental VCO reference implementation has a minBLEP bug where it sometimes inserts a second discontinuity immediately after the first, causing audible pops and clicks. Many developers copy this flawed implementation.

**Why it happens:** Developers use VCV Fundamental VCO as reference code without knowing about the bug. The issue is subtle and not immediately obvious during development.

**Consequences:**
- Clicks and pops during pulse width modulation
- Artifacts during waveform transitions
- Issue affects multiple popular modules (Bog Audio, Slime Child, Instruo, Befaco)
- Difficult to diagnose - appears to work but sounds wrong

**Prevention:**
- Only insert discontinuities for the single sample where pre-antialiased value changes
- Detect exact sample where value jumps (e.g., -1 → +1 or +1 → -1)
- Call `minBlepper.process()` EVERY sample, not just when discontinuities occur
- Use linear interpolation to estimate sub-sample discontinuity position
- Verify with spectrum analyzer under PWM modulation

**Detection:**
- Clicks/pops when modulating pulse width with sine wave
- Issue worsens with faster modulation rates
- Spectrum analyzer shows unexpected transient spikes

**Phase to address:** Phase 1 (Core oscillator) - Critical to get right initially, as it affects all waveform outputs.

**Sources:**
- [VCV Fundamental Issue #140](https://github.com/VCVRack/Fundamental/issues/140)
- [Community: Help me understand minBLEP implementation](https://community.vcvrack.com/t/help-me-understand-minblep-implementation/16673)

---

### Pitfall 3: Incorrect Polyphony Channel Handling

**What goes wrong:** Modules incorrectly sum CV inputs (which should be per-channel) or fail to sum audio inputs (which should combine channels monophonically). This breaks user patches in unexpected ways.

**Why it happens:** Developers don't understand the distinction between audio and CV signal semantics in polyphonic context.

**Consequences:**
- Polyphonic oscillator playing chord into monophonic effect produces silence or wrong behavior
- CV modulation affects all voices identically instead of per-voice
- Feedback loops where channel counts increase but never decrease
- User patches break when switching between mono/poly

**Prevention:**
- **Audio inputs:** Use `getVoltageSum()` to sum all channels for monophonic processing
- **CV inputs:** Use `getPolyVoltage(channel)` to process each channel independently
- **Hybrid inputs:** Determine based on primary use case (pitch CV = don't sum, audio FM = don't sum)
- **Multiple primary inputs:** Use `std::max(..., 1)` to determine channel count from maximum of all inputs
- **Polyphonic lights:** Use root mean sum: √(x₀² + x₁² + ... + xₙ²)

**Detection:**
- Polyphonic chord through monophonic module produces unexpected output
- Channel count doesn't propagate correctly downstream
- Testing with both mono and poly patches reveals different behavior

**Phase to address:** Phase 1 (Polyphony architecture) - Must be designed correctly from start. Very difficult to change later as it affects all I/O ports.

**Sources:**
- [VCV Rack Polyphony Manual](https://vcvrack.com/manual/Polyphony)
- [Community: Polyphony reminders for plugin developers](https://community.vcvrack.com/t/polyphony-reminders-for-plugin-developers/9572)

---

### Pitfall 4: Thread Safety Violations (GUI Access from DSP Thread)

**What goes wrong:** Accessing NanoVG GUI functions from the `process()` method or directly accessing another module's instance variables causes crashes, corruption, or audio glitches.

**Why it happens:** Developers don't understand VCV Rack's threading model - audio processing happens on a separate thread from GUI rendering.

**Consequences:**
- Random crashes (especially on certain CPUs/configurations)
- Audio hiccups and dropouts
- Data corruption in inter-module communication
- Unpredictable processing order between modules

**Prevention:**
- Keep ALL NanoVG calls in `draw()` methods only
- Never access GUI state from `process()`
- Use expander messages (double-buffered) for inter-module communication
- Use atomic operations or accept 1-frame latency for shared state
- Restrict file I/O to event methods or worker threads (never in `process()`)
- Remember: VCV Rack guarantees `process()`, `dataToJson()`, `dataFromJson()` are mutually exclusive within a module

**Detection:**
- Crashes that only happen on some systems
- Audio glitches when moving mouse or adjusting parameters
- Corruption in saved state
- Unpredictable behavior with multiple instances

**Phase to address:** Phase 1 (Architecture) - Core design decision that affects all subsequent code. Cannot be easily refactored later.

**Sources:**
- [VCV Rack Plugin Guide](https://vcvrack.com/manual/PluginGuide)

---

### Pitfall 5: Denormal Numbers Causing CPU Spikes

**What goes wrong:** IIR filters and feedback paths produce denormal floating-point numbers (very small values) that are 100x slower to process, causing CPU spikes and audio dropouts.

**Why it happens:** When filters ring out to silence or feedback decays exponentially, values dip below ~1e-38 and become denormal. Older CPUs are especially vulnerable.

**Consequences:**
- CPU usage spikes to 100% when module should be idle
- Audio dropouts and crackles
- Patch becomes unusable on lower-end systems
- Issue only appears after module runs for extended time

**Prevention:**
- Set FTZ (flush-to-zero) and DAZ (denormals-are-zero) CPU flags (VCV Rack may handle this, verify)
- Add tiny DC offset (1e-18, ~-200dB) to feedback paths and filter outputs
- Detect when input goes to silence and zero out filter state
- Test filters with silence after signal to verify no CPU spike

**Detection:**
- CPU meter spikes when no audio present
- Performance degrades over time
- Issue appears in filters, reverbs, feedback FM paths

**Phase to address:** Phase 1 (DSP core) and Phase 2 (Modulation paths) - Add prevention to all IIR filters and feedback structures as they're implemented.

**Sources:**
- [EarLevel Engineering: Denormal Numbers](https://www.earlevel.com/main/2019/04/19/floating-point-denormals/)
- General DSP knowledge (LOW confidence for VCV Rack specifics - verify if VCV Rack already handles this)

---

### Pitfall 6: Voltage Range Non-compliance

**What goes wrong:** Outputting wrong voltage ranges (e.g., 0-10V audio instead of ±5V, or ±5V CV instead of 0-10V unipolar) breaks compatibility with other modules.

**Consequences:**
- Too-hot signals clip downstream modules
- CV controls have wrong range, making modulation unusable
- Incompatibility with hardware Eurorack via VCV Rack I/O modules
- User confusion when patches don't work as expected

**Prevention:**
- **Audio outputs:** ±5V before bandlimiting
- **Unipolar CV (LFO, envelope):** 0 to 10V
- **Bipolar CV:** ±5V
- **Gates/Triggers:** 10V when active, 0V when inactive
- **Triggers:** 10V pulse with 1ms duration
- **1V/oct pitch:** C4 = 0V, use C4 = 261.6256 Hz for oscillators
- Test with VCV Scope module at various settings
- Never hard-clip with `clamp()` - allow overshoots for downstream attenuation
- DO saturate outputs when applying >1x gain

**Detection:**
- VCV Scope shows wrong voltage ranges
- Compatibility issues with standard VCV modules
- User reports of "too loud" or "too quiet"

**Phase to address:** Phase 1 (Core I/O) - Define voltage ranges for all ports initially. Changing later breaks user patches.

**Sources:**
- [VCV Rack Voltage Standards](https://vcvrack.com/manual/VoltageStandards)

---

## Moderate Pitfalls

These mistakes cause delays, technical debt, or significant rework but don't break core functionality.

### Pitfall 7: Phase Accumulation Numerical Drift

**What goes wrong:** Using single-precision float for phase accumulation causes precision loss as the phase value grows, leading to pitch instability over time.

**Why it happens:** Developers accumulate phase with `phase += frequency * sampleTime` without proper wrapping or precision handling.

**Consequences:**
- Pitch drifts sharp over minutes/hours of continuous play
- Low-frequency oscillators (<1Hz) especially affected
- Long-running patches become detuned

**Prevention:**
- Wrap phase to [0, 1) range EVERY sample: `if (phase >= 1.0f) phase -= 1.0f;`
- Use double precision for phase accumulator if LFO rates are supported
- Test oscillator running continuously for 10+ minutes, check pitch stability
- For very low frequencies, consider alternative implementations (wavetable with high precision indexing)

**Detection:**
- Pitch measurement drifts over time
- Issue more pronounced at low frequencies
- Frequency meter shows instability

**Phase to address:** Phase 1 (Core oscillator) - Part of fundamental oscillator loop structure.

**Sources:**
- [VCV Rack Plugin Tutorial](https://vcvrack.com/manual/PluginDevelopmentTutorial) (shows phase wrapping example)
- General DSP best practices

---

### Pitfall 8: Incorrect minBLEP Time Parameter

**What goes wrong:** Developers struggle with the `addDiscontinuity()` time parameter, which represents sub-sample precision in the range (-1, 0], not absolute time.

**Why it happens:** The parameter name and documentation are unclear. It's not intuitive that time represents fractional sample position.

**Consequences:**
- Discontinuities placed at wrong sub-sample positions
- Anti-aliasing effectiveness reduced
- Subtle artifacts remain despite using minBLEP

**Prevention:**
- Time parameter is sub-sample offset: 0 = current sample, -1 = previous sample
- Use linear interpolation to estimate discontinuity position within sample
- Example: if phase wraps from 0.8 to 0.2, discontinuity occurred at position -(0.2 / (0.2 + (1.0 - 0.8))) = -0.2
- Verify with spectrum analyzer that aliasing is actually reduced

**Detection:**
- minBLEP used but aliasing still present
- Spectrum analyzer shows remaining harmonic artifacts

**Phase to address:** Phase 1 (Core oscillator) - Part of anti-aliasing implementation.

**Sources:**
- [Community: Help me understand minBLEP implementation](https://community.vcvrack.com/t/help-me-understand-minblep-implementation/16673)

---

### Pitfall 9: Triangle Wave Anti-aliasing Done Wrong

**What goes wrong:** Developers try to apply minBLEP directly to triangle waves, but triangle waves are continuous (no discontinuities), so traditional minBLEP doesn't apply. Integration-based approaches can be unstable with modulation.

**Why it happens:** Misunderstanding of when anti-aliasing techniques apply. Triangle waves have slope discontinuities, not value discontinuities.

**Consequences:**
- No anti-aliasing benefit despite using minBLEP
- Integration approaches cause DC drift with modulation
- Wasted CPU cycles on ineffective processing

**Prevention:**
- Use polyBLAMP (integrates polyBLEP) for triangle waves, OR
- Generate bandlimited square with minBLEP, then integrate that square wave
- Don't use simple integration with modulation - causes instability
- Consider wavetable approach for triangle if modulation needed

**Detection:**
- Triangle output shows aliasing despite anti-aliasing code
- DC drift in triangle waveform with modulation
- Spectrum analyzer shows high-frequency artifacts

**Phase to address:** Phase 1 (Core oscillator) - Part of waveform generation strategy.

**Sources:**
- [Community: Help me understand minBLEP implementation](https://community.vcvrack.com/t/help-me-understand-minblep-implementation/16673)
- General DSP knowledge about polyBLAMP

---

### Pitfall 10: Through-Zero FM Aliasing

**What goes wrong:** Through-zero FM causes frequency to go negative, creating discontinuities that alias severely if not handled properly. Simple oversampling may not be sufficient for extreme modulation.

**Why it happens:** Through-zero FM is complex to implement correctly. Negative frequencies require special phase handling.

**Consequences:**
- Severe aliasing during deep FM
- Audio-rate FM sounds harsh and digital
- Module unusable for characteristic through-zero sounds

**Prevention:**
- Implement proper through-zero algorithm (phase runs backwards when frequency negative)
- Apply anti-aliasing via polyBLEP/polyBLAMP or 2x-4x oversampling
- Consider oversampling specifically for FM path (can be switchable for CPU savings)
- Test with extreme FM amounts at audio rate
- Some aliasing may be acceptable at very high notes (Surge XT notes this)

**Detection:**
- Spectrum analyzer shows heavy aliasing during FM
- Audible harshness with audio-rate FM
- Issue worsens with higher modulation index

**Phase to address:** Phase 2 (Modulation features) - Through-zero FM is complex feature requiring careful implementation.

**Sources:**
- Search results mention polyBLEP/polyBLAMP for FM, oversampling approaches
- [Bogaudio Modules](https://github.com/bogaudio/BogaudioModules) implementation
- [21kHz Palm Loop](https://github.com/netboy3/21kHz-rack-plugins) example

---

### Pitfall 11: Hard Sync Triggering on Wrong Edge

**What goes wrong:** VCV Fundamental VCO had a bug where hard sync was triggered on negative slope instead of positive slope, causing incorrect behavior.

**Why it happens:** Developers copy reference implementations without understanding sync semantics.

**Consequences:**
- Sync behavior doesn't match user expectations (based on hardware Eurorack)
- Timing relationship between oscillators is wrong
- Can cause phase jumps at unexpected times

**Prevention:**
- Trigger sync on RISING edge (0V crossing from below)
- Use Schmitt trigger with hysteresis (~0.1V low, ~1-2V high) to avoid noise
- Account for 1-sample cable delay in VCV Rack when timing is critical
- Test with oscilloscope visualization to verify phase reset behavior

**Detection:**
- Sync behavior differs from hardware or other VCV modules
- Unexpected phase relationships
- User reports of "wrong" sync sound

**Phase to address:** Phase 2 (Sync features) - When implementing hard sync functionality.

**Sources:**
- [VCV Fundamental Changelog](https://github.com/VCVRack/Fundamental/blob/v1/CHANGELOG.md)
- [VCV Rack Voltage Standards](https://vcvrack.com/manual/VoltageStandards) (Schmitt trigger thresholds)

---

### Pitfall 12: PWM Clicks from Slew-Limiting Oversight

**What goes wrong:** Rapid PWM changes create discontinuities in the square wave that aren't properly handled, causing clicks and pops.

**Why it happens:** MinBLEP handles waveform discontinuities but not parameter discontinuities. PWM creates new discontinuities when pulse width changes.

**Consequences:**
- Audible clicks during PWM modulation
- Issue worse with faster modulation rates
- Affects multiple commercial modules (Bog Audio, Slime Child, Troika)

**Prevention:**
- Apply small slew limiting to discontinuous output jumps (Bogaudio approach)
- Ensure minBLEP discontinuities are inserted for PWM transitions, not just phase wraps
- Test with sine wave PWM modulation at various rates
- Verify with spectrum analyzer for transient spikes

**Detection:**
- Clicks/pops when modulating pulse width
- Issue increases with modulation rate
- Spectrum shows unexpected sharp transients

**Phase to address:** Phase 1 (Core oscillator) if PWM is core feature, or Phase 2 (Modulation) if added later.

**Sources:**
- [VCV Fundamental Issue #140](https://github.com/VCVRack/Fundamental/issues/140)
- [Community: Clicks and Pops from PWM](https://community.vcvrack.com/t/clicks-and-pops-from-pwm/15824)

---

### Pitfall 13: Incorrect Sample Rate Handling

**What goes wrong:** Using `rack::settings::sampleRate` instead of `args.sampleTime` or `args.sampleRate`, causing incorrect behavior when sample rate changes dynamically.

**Why it happens:** Developers use global settings instead of per-process parameters.

**Consequences:**
- Pitch changes when user changes engine sample rate
- Filter coefficients become wrong
- Oscillator frequencies incorrect
- Modulation rates wrong

**Prevention:**
- Always use `args.sampleTime` (= 1/sampleRate) from process args
- Override `onSampleRateChange()` to recalculate sample-rate-dependent coefficients
- Recalculate filter coefficients when sample rate changes
- Test module at 44.1kHz, 48kHz, 96kHz to verify correct behavior

**Detection:**
- Pitch changes when sample rate changed in VCV settings
- Filters sound different at different sample rates
- Frequency measurements don't match expected values

**Phase to address:** Phase 1 (Core architecture) - All DSP code must use correct sample rate from start.

**Sources:**
- [Community: Do not use rack::settings::sampleRate](https://community.vcvrack.com/t/do-not-use-rack-samplerate/24289)

---

### Pitfall 14: Large State Data in toJson/fromJson

**What goes wrong:** Serializing large buffers (>100KB) in `dataToJson()`/`dataFromJson()` blocks the UI thread, causing lag when saving/loading patches.

**Why it happens:** Developers store wavetables, sample data, or large state arrays directly in JSON.

**Consequences:**
- VCV Rack freezes for seconds when saving patch
- Auto-save causes periodic UI freezes
- Poor user experience, appears broken

**Prevention:**
- For data >100KB, use `createPatchStorageDirectory()` and `getPatchStorageDirectory()`
- Write large buffers to separate files in patch storage directory
- Keep JSON for small config data only (settings, parameter state)
- Remember: parameters (knobs, switches) are auto-saved, don't duplicate

**Detection:**
- UI lag when saving patches
- Patch files are extremely large
- User reports of freezing during save

**Phase to address:** Phase 3 (Wavetables/samples) if using large data, or Phase 1 if planning for it.

**Sources:**
- [VCV Rack Plugin Guide](https://vcvrack.com/manual/PluginGuide)
- [Community: Module state issues](https://community.vcvrack.com/t/using-isconnected-after-module-state-restore-with-datafromjson/18448)

---

### Pitfall 15: SIMD float_4 Branching

**What goes wrong:** Branching on SIMD vector elements (e.g., `if (float_4_value[0] > 0)`) destroys vectorization benefits and may cause errors.

**Why it happens:** Developers treat `float_4` like an array of floats instead of as a single SIMD value.

**Consequences:**
- No performance benefit from SIMD
- Possible incorrect behavior (endianness issues: x[0] is actually s[3] on x86_64)
- Polyphonic processing is as slow as 4x monophonic

**Prevention:**
- Never branch on individual SIMD elements
- Use SIMD conditional operations (masks, selects)
- Process all 4 channels together: `for (int c = 0; c < channels; c += 4)`
- Use `simd::` math functions, not `std::` functions
- Accessing individual elements is slow - only do in non-critical code

**Detection:**
- SIMD polyphonic code is not faster than monophonic
- Debugger shows scalar operations instead of vector instructions
- Profiler shows no SSE4.2 usage

**Phase to address:** Phase 1 (Polyphony architecture) - SIMD strategy must be designed from start for 16-oscillator module.

**Sources:**
- [VCV Rack API: simd::Vector](https://vcvrack.com/docs-v2/structrack_1_1simd_1_1Vector_3_01float_00_014_01_4)
- [Community: What's the point of writing manual SIMD code?](https://community.vcvrack.com/t/whats-the-point-of-writing-manual-simd-code/3774)

---

### Pitfall 16: Unison Voice Spreading Algorithm

**What goes wrong:** Poor detuning algorithms create clustered voices instead of spread, or odd channel counts don't preserve center pitch.

**Why it happens:** Developers use random or linear spreading without considering musical implications.

**Consequences:**
- Unison doesn't sound "wide" or "thick"
- Center pitch shifts with odd vs even voice counts
- Unmusical chorusing effect

**Prevention:**
- Symmetric spreading: voice pairs at +detune/-detune
- Odd channel counts: one voice at center pitch, others spread symmetrically
- Even channel counts: no center voice, symmetric pairs
- Detune amounts spread evenly (not random)
- Example (Bogaudio algorithm):
  - 1 channel: unaltered pitch
  - 2 channels: +full detune, -full detune
  - 3 channels: 0 detune, +full detune, -full detune
  - 4 channels: +full detune, -full detune, +half detune, -half detune

**Detection:**
- Unison sounds off-pitch
- Perceived pitch changes with voice count
- Weak stereo spreading effect

**Phase to address:** Phase 3 (Voicing modes) - When implementing unison functionality.

**Sources:**
- [Bogaudio UNISON module](https://library.vcvrack.com/Bogaudio/Bogaudio-Unison)
- [Community: Detune and unison](https://community.vcvrack.com/t/detune-and-unison/6072)

---

### Pitfall 17: Sub-Oscillator Phase Relationship

**What goes wrong:** Sub-oscillator runs at half frequency but phase isn't correctly tracked, causing beating or instability.

**Why it happens:** Dividing frequency by 2 and using separate phase accumulator creates phase drift between main and sub oscillators.

**Consequences:**
- Sub oscillator phase drifts relative to main oscillator
- Beating between fundamental and sub
- Sync resets don't affect sub correctly

**Prevention:**
- Derive sub-oscillator from main oscillator phase, not separate accumulator
- Sub phase = main_phase * 0.5 (or use phase-based algorithm)
- Sync input should reset both main and sub phase simultaneously
- Test with scope - sub should stay phase-locked to main

**Detection:**
- Sub oscillator drifts out of phase
- Beating audible between main and sub
- Scope shows phase relationship changing over time

**Phase to address:** Phase 2 (Sub-oscillator feature) - When implementing sub-oscillator.

**Sources:**
- General oscillator design knowledge
- [VCV modules sub-oscillator implementations](https://vcvrack.com/Free)

---

## Minor Pitfalls

These cause annoyance or suboptimal UX but are easily fixable.

### Pitfall 18: Panel Spacing Too Tight

**What goes wrong:** Knobs and ports placed too close together make module difficult to use, especially when patching with thick cables or using touch input.

**Why it happens:** Developers optimize for panel space without considering ergonomics.

**Consequences:**
- Difficult to patch without unplugging adjacent cables
- Knobs can't be adjusted while cables patched
- Poor user experience, frustration

**Prevention:**
- VCV Rack Panel Guide: "Make sure there is enough space between knobs and ports to put your thumbs between them"
- Test panel layout with multiple cables patched
- Use standard Eurorack spacing where possible
- Get feedback from beta testers on ergonomics

**Detection:**
- Difficulty patching module in practice
- User complaints about cramped layout
- Accidentally adjusting wrong knob

**Phase to address:** Phase 0 (Panel design) - Design panel layout before implementation begins.

**Sources:**
- [VCV Rack Module Panel Guide](https://vcvrack.com/manual/Panel)

---

### Pitfall 19: SVG Panel Export Issues

**What goes wrong:** Text objects not converted to paths, complex gradients, or CSS styling cause panel rendering to fail or look wrong.

**Why it happens:** SVG export from design tools includes unsupported features.

**Consequences:**
- Panel doesn't render correctly
- Text appears as boxes or wrong font
- Colors wrong or missing

**Prevention:**
- Convert all text to paths in Inkscape: Path > Object to Path
- Use only simple 2-color linear gradients
- Avoid CSS styling (limited support)
- Test panel rendering in VCV Rack before finalizing
- Panel must be 128.5mm height, width = 5.08mm * HP

**Detection:**
- Panel looks different in VCV Rack than in design tool
- Missing text or graphics
- Console errors about SVG parsing

**Phase to address:** Phase 0 (Panel design) - Verify SVG export before development starts.

**Sources:**
- [VCV Rack Module Panel Guide](https://vcvrack.com/manual/Panel)

---

### Pitfall 20: Output Port Visual Distinction

**What goes wrong:** Output ports not visually distinguished from input ports, causing user confusion about signal flow.

**Why it happens:** Developers use same visual style for all ports.

**Consequences:**
- Users patch backwards (outputs to outputs)
- Confusion about module functionality
- Poor user experience

**Prevention:**
- Use inverted backgrounds for output ports (VCV standard)
- Consistent port styling across module
- Clear labeling (but visual distinction is primary)
- Follow VCV Fundamental visual conventions

**Detection:**
- User confusion about which ports are outputs
- Patching mistakes common in testing

**Phase to address:** Phase 0 (Panel design) - Part of initial panel design.

**Sources:**
- [VCV Rack Module Panel Guide](https://vcvrack.com/manual/Panel)

---

### Pitfall 21: NaN/Infinity Propagation

**What goes wrong:** Unstable filters or divide-by-zero create NaN or Infinity values that propagate through patch, causing silence or noise.

**Why it happens:** No validation of DSP outputs, especially in edge cases.

**Consequences:**
- Patch suddenly goes silent
- Explosive noise destroys downstream modules
- CPU usage spikes
- Requires patch reload to fix

**Prevention:**
- Check for NaN/Inf in critical outputs: `if (!std::isfinite(output)) output = 0.f;`
- Validate inputs before division or other unstable operations
- Add safety limits to feedback paths
- Test with extreme parameter values

**Detection:**
- VCV Scope shows flat line or noise
- Downstream modules show wrong behavior
- Issue appears with specific parameter combinations

**Phase to address:** Phase 1 (Core DSP) and Phase 2 (Modulation) - Add checks to all critical DSP paths.

**Sources:**
- [VCV Rack Voltage Standards](https://vcvrack.com/manual/VoltageStandards) (mentions checking for NaN/Inf)

---

### Pitfall 22: Expander Message Race Conditions

**What goes wrong:** Directly accessing another module's instance variables causes data races, corruption, or crashes because processing order is undefined.

**Why it happens:** Developers want inter-module communication but bypass VCV Rack's expander message system.

**Consequences:**
- Data corruption between modules
- Unpredictable behavior based on module position
- Crashes on some systems

**Prevention:**
- Use expander messages (double-buffered) for all inter-module communication
- Accept 1-frame latency inherent in expander system
- Never directly access another module's variables from process()
- Use `getLeftExpander()` and `getRightExpander()` correctly

**Detection:**
- Behavior changes when modules swapped
- Intermittent corruption or crashes
- Different results on different systems

**Phase to address:** Only if planning expander modules (not in current project scope).

**Sources:**
- [VCV Rack Plugin Guide](https://vcvrack.com/manual/PluginGuide)

---

### Pitfall 23: Cable Connection Clicks

**What goes wrong:** Patching or unpatching cables creates audible clicks when module doesn't handle connection state changes smoothly.

**Why it happens:** No slewing or fade-in when cables connected, sudden voltage jumps.

**Consequences:**
- Loud click when patching live
- Disrupts performance or recording
- Poor user experience

**Prevention:**
- Detect cable connection changes: `input.isConnected()` in `process()`
- Apply short fade-in (1-10ms) when cable newly connected
- Optionally slew to new voltage instead of jumping
- Test by patching/unpatching during audio playback

**Detection:**
- Audible clicks when patching cables
- User complaints about patching noise

**Phase to address:** Phase 2 (Polish) - Quality-of-life feature, not critical but improves UX.

**Sources:**
- [Community: Click/Pops when connecting audio cables](https://community.vcvrack.com/t/question-click-pops-when-connecting-audio-cables/18783)

---

### Pitfall 24: One-Sample Cable Delay Timing

**What goes wrong:** Not accounting for VCV Rack's inherent 1-sample cable delay causes timing issues between CLOCK and RESET signals.

**Why it happens:** Developers assume zero-latency signal propagation like hardware.

**Consequences:**
- Clock and reset out of sync
- Sequencer timing issues
- Phase relationship between signals wrong

**Prevention:**
- Remember: every cable introduces 1-sample delay
- Design timing-critical features with this in mind
- Use Schmitt trigger detection to handle edge cases
- Test timing relationships with oscilloscope module

**Detection:**
- Timing issues between modules
- Phase relationships off by 1 sample
- Sequencer or clock behavior incorrect

**Phase to address:** Phase 1 if timing-critical features (sync), otherwise general awareness.

**Sources:**
- [VCV Rack Voltage Standards](https://vcvrack.com/manual/VoltageStandards)
- [Community discussions on sync timing](https://community.vcvrack.com/t/is-it-correct-to-state-that-vcv-rack-would-never-make-sync-seq/3726)

---

### Pitfall 25: Oversampling CPU Cost Underestimation

**What goes wrong:** Enabling high oversampling (8x, 16x) makes module unusable due to CPU consumption, especially with 16 oscillators.

**Why it happens:** Developers add oversampling without considering cumulative CPU cost across all voices.

**Consequences:**
- Single module instance consumes 50%+ CPU
- Patches limited to just a few modules
- Module gets reputation for poor performance

**Prevention:**
- Profile CPU usage at various oversampling levels
- Make oversampling optional/configurable via context menu
- Use oversampling selectively (only for FM/sync paths that need it)
- Consider 2x oversampling as default maximum for 16-oscillator module
- Document CPU usage in manual

**Detection:**
- CPU meter shows very high usage
- Audio dropouts with single module instance
- User complaints about performance

**Phase to address:** Phase 1 (Architecture) - CPU budget must be considered from start with 16 oscillators.

**Sources:**
- [Community: CPU performance discussions](https://community.vcvrack.com/t/more-cpu-cores-worse-performance/16580)
- [VCV Rack DSP Manual](https://vcvrack.com/manual/DSP) (profiling guidance)

---

## Phase-Specific Warnings

Mapping pitfalls to likely roadmap phases where they become relevant.

| Phase Topic | Critical Pitfalls | Prevention Focus |
|-------------|-------------------|------------------|
| **Phase 1: Core Oscillator** | #1 (Anti-aliasing), #2 (minBLEP bugs), #3 (Polyphony), #4 (Threading), #5 (Denormals), #6 (Voltage ranges), #7 (Phase drift), #13 (Sample rate), #25 (CPU budget) | Establish correct DSP architecture, anti-aliasing, polyphony handling. These are nearly impossible to fix later. |
| **Phase 2: Waveform Mixing** | #9 (Triangle anti-aliasing), #21 (NaN/Inf) | Ensure each waveform is properly bandlimited and validated. |
| **Phase 3: Modulation (FM/Sync)** | #10 (Through-zero FM aliasing), #11 (Sync edge), #12 (PWM clicks), #17 (Sub-oscillator phase) | Modulation paths require careful anti-aliasing and phase handling. |
| **Phase 4: Voicing Modes** | #16 (Unison spreading) | Musical detuning algorithms for unison. |
| **Phase 5: Polish** | #23 (Cable clicks), #24 (Cable delay timing) | Quality-of-life improvements for professional UX. |
| **Phase 0: Panel Design** | #18 (Spacing), #19 (SVG export), #20 (Port distinction) | Get panel design right before coding begins. |

---

## Research Confidence Assessment

| Area | Confidence | Basis |
|------|------------|-------|
| DSP Anti-aliasing | **HIGH** | Official VCV docs, verified community discussions, GitHub issues with solutions |
| Polyphony | **HIGH** | Official VCV manual, developer reminders thread |
| Threading/Architecture | **HIGH** | Official VCV plugin guide |
| Voltage Standards | **HIGH** | Official VCV voltage standards manual |
| Performance/CPU | **MEDIUM** | Community discussions, no official benchmarks for 16-oscillator case |
| minBLEP Specifics | **HIGH** | Well-documented bug with solution in GitHub issue |
| SIMD/float_4 | **HIGH** | Official API docs, community discussions |
| Panel Design | **HIGH** | Official panel guide |
| Denormal Handling | **MEDIUM** | General DSP knowledge, unclear if VCV Rack handles this automatically (requires verification) |
| Sub-oscillator | **MEDIUM** | General DSP knowledge, inferred from module examples |

---

## Gaps and Open Questions

**Questions requiring verification:**
1. Does VCV Rack automatically set FTZ/DAZ CPU flags, or must modules handle denormals manually?
2. What is realistic CPU budget for 16-oscillator module at 48kHz with 2x oversampling?
3. Are there VCV Rack-specific SIMD optimization patterns beyond basic `float_4` usage?
4. What is current status of VCV Fundamental VCO minBLEP bug - is it fixed in latest version?

**Areas for deeper phase-specific research:**
- Phase 2: Specific through-zero FM algorithms used in VCV Rack modules
- Phase 3: XOR waveshaping implementation details and aliasing characteristics
- Phase 4: Stereo spreading algorithms for unison mode beyond basic detune

**Testing requirements not covered:**
- Automated testing approaches for DSP correctness
- Regression testing for anti-aliasing effectiveness
- Performance profiling methodology for VCV Rack

---

## Key Takeaways for Roadmap Creation

1. **Front-load architecture decisions** - Pitfalls #1-7, #13, #25 MUST be addressed in Phase 1. They cannot be retrofitted without complete rewrites.

2. **Anti-aliasing is non-negotiable** - With 16 oscillators and cross-modulation, aliasing will be catastrophic if not handled from day one. Budget significant Phase 1 time for this.

3. **Polyphony is a first-class concern** - With 8-voice poly, incorrect channel handling (#3, #15) will break user patches. Design polyphony architecture before writing code.

4. **CPU budget is critical** - 16 oscillators means CPU usage is 16x single oscillator. Every DSP decision must consider cumulative cost. Profile early and often.

5. **Use VCV Fundamental as reference cautiously** - It has known bugs (#2). Verify against multiple sources and official docs.

6. **Thread safety is not optional** - Violation of threading rules (#4, #22) causes intermittent, hard-to-debug crashes. Understand VCV's threading model completely before coding.

7. **Panel design before coding** - Pitfalls #18-20 should be resolved in Phase 0 panel design to avoid rework.

8. **Testing must include spectrum analysis** - Visual verification of anti-aliasing is essential. Budget time for test tooling setup.

---

## Sources

All findings sourced from verified VCV Rack documentation and community resources:

### Official VCV Rack Documentation (HIGH confidence)
- [VCV Rack DSP Manual](https://vcvrack.com/manual/DSP)
- [VCV Rack Plugin Development Tutorial](https://vcvrack.com/manual/PluginDevelopmentTutorial)
- [VCV Rack Polyphony Manual](https://vcvrack.com/manual/Polyphony)
- [VCV Rack Plugin API Guide](https://vcvrack.com/manual/PluginGuide)
- [VCV Rack Voltage Standards](https://vcvrack.com/manual/VoltageStandards)
- [VCV Rack Module Panel Guide](https://vcvrack.com/manual/Panel)
- [VCV Rack API Documentation](https://vcvrack.com/docs-v2/)

### VCV Rack GitHub Issues (HIGH confidence)
- [Fundamental Issue #140: PWM Pops and Clicks Solution](https://github.com/VCVRack/Fundamental/issues/140)
- [Fundamental Changelog](https://github.com/VCVRack/Fundamental/blob/v1/CHANGELOG.md)

### VCV Community Forum (MEDIUM-HIGH confidence)
- [Polyphony reminders for plugin developers](https://community.vcvrack.com/t/polyphony-reminders-for-plugin-developers/9572)
- [Help me understand minBLEP implementation](https://community.vcvrack.com/t/help-me-understand-minblep-implementation/16673)
- [Clicks and Pops from PWM](https://community.vcvrack.com/t/clicks-and-pops-from-pwm/15824)
- [Do not use rack::settings::sampleRate](https://community.vcvrack.com/t/do-not-use-rack-samplerate/24289)
- [What's the point of writing manual SIMD code?](https://community.vcvrack.com/t/whats-the-point-of-writing-manual-simd-code/3774)
- [Detune and unison discussion](https://community.vcvrack.com/t/detune-and-unison/6072)

### Third-Party Module Documentation (MEDIUM confidence)
- [Bogaudio Modules GitHub](https://github.com/bogaudio/BogaudioModules)
- [Surge XT Modules Manual](https://surge-synthesizer.github.io/rack_xt_manual/)
- [21kHz Rack Plugins](https://github.com/netboy3/21kHz-rack-plugins)

### DSP Reference (MEDIUM-LOW confidence for VCV specifics)
- [EarLevel Engineering: Denormal Numbers](https://www.earlevel.com/main/2019/04/19/floating-point-denormals/)
- General DSP knowledge (phase accumulation, filter design, anti-aliasing theory)

**Note:** Web search was intermittently unavailable during research. Some findings are based on pre-existing knowledge marked as MEDIUM confidence and should be verified against official sources during implementation.
