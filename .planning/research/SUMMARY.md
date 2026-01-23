# Research Summary: VCV Rack Polyphonic Dual-VCO Module

**Project:** 8-voice polyphonic dual-VCO module inspired by TipTop Audio Triax 8
**Research Date:** 2026-01-22
**Synthesized From:** STACK.md, FEATURES.md, ARCHITECTURE.md, PITFALLS.md

---

## Project Overview

We are building a sophisticated VCV Rack module featuring 16 oscillators total (2 VCOs per voice × 8 voices) with extensive cross-modulation capabilities. This is a high-complexity polyphonic oscillator module targeting advanced synthesis users who want Triax-8-inspired dual oscillator architecture with through-zero FM, cross-sync, XOR waveshaping, and flexible voicing modes.

**Key characteristics:**
- 8-voice polyphony with dual oscillators per voice
- Mixable waveforms: triangle, square, sine, sawtooth per oscillator
- Advanced modulation: through-zero FM, hard/soft sync, XOR waveshaping
- Sub-oscillator, PWM, built-in vibrato
- Voicing modes: polyphonic, dual, unison
- User-controllable analog character
- Wide panel (30+ HP) with all controls accessible

---

## Executive Summary

Building a polyphonic dual-VCO for VCV Rack requires mastering three critical domains: **anti-aliasing DSP**, **SIMD-optimized polyphony**, and **VCV Rack integration patterns**. The research reveals a clear technology path using C++11, VCV Rack SDK 2.6+, and polyBLEP/polyBLAMP anti-aliasing, but success hinges on making the right architectural decisions in Phase 1 that cannot be retrofitted later.

### The Recommended Approach

**Stack foundation:** VCV Rack SDK 2.6+ with C++11 (mandatory for plugin library distribution), SIMD float_4 for polyphonic processing, Makefile-based builds, and Inkscape for panel design. The SDK bundles all necessary DSP utilities, but critically, **does not include built-in anti-aliasing** - you must implement polyBLEP yourself or reference open-source modules like Fundamental, Bogaudio, or 21kHz.

**Architecture strategy:** Four-layer architecture (Module → Voice Processor → Oscillator Core → Modulation Components) with SIMD templating (`template<typename T>`) to support both scalar and float_4 processing. Process 4 voices at a time with float_4 (2 SIMD groups for 8 voices) to achieve 3-4x performance improvement over naive implementation. This is **mandatory** for acceptable CPU usage with 16 oscillators.

**Feature priorities:** The research identifies clear table stakes (polyphonic cable support, V/oct tracking, multiple waveforms, antialiasing, PWM, hard sync, FM) versus differentiators (through-zero FM, dual oscillator architecture, cross-modulation, XOR waveshaping, voicing modes). The dual-oscillator-per-voice architecture is unique and valuable but increases complexity significantly.

### Key Risks and Mitigation

**Risk 1: Anti-aliasing catastrophe.** With 16 oscillators generating sawtooth/square waves plus sync/FM modulation, missing or incorrect anti-aliasing will produce unusable harsh artifacts. VCV Fundamental has a documented minBLEP bug that many developers copy unknowingly. **Mitigation:** Implement polyBLEP from first principles, verify with spectrum analyzer, test at high frequencies (>2kHz), and study Befaco EvenVCO implementation (noted as nearly perfect).

**Risk 2: CPU usage makes module unusable.** 16 oscillators without SIMD optimization would consume 16x CPU of single oscillator, making polyphonic patches impossible. **Mitigation:** SIMD float_4 from Phase 1, profile continuously, target <15% CPU for 8-voice full-feature usage, avoid expensive oversampling (2x maximum), use fast math approximations (dsp::exp2_taylor5).

**Risk 3: Polyphony architecture mistakes break user patches.** Incorrect channel handling (summing CV inputs, not summing audio inputs) causes subtle failures that are hard to debug. Thread safety violations cause intermittent crashes. **Mitigation:** Follow VCV polyphony guidelines exactly, use args.sampleTime not global sample rate, never access GUI from process(), test with both mono and poly patches.

**Risk 4: Through-zero FM and cross-sync implementation complexity.** These differentiating features are technically challenging with severe aliasing concerns. **Mitigation:** Implement basic FM first, add through-zero carefully with linear FM, test extensively at unison, consider optional oversampling for FM/sync paths only.

---

## Key Stack Decisions

### Core Framework
- **VCV Rack SDK 2.6+** - Latest stable (Nov 2024), includes polyphonic cable API and SIMD utilities
- **C++11 standard** - REQUIRED for plugin library distribution (Linux libstdc++ 5.4.0 limitation)
- **Platform toolchains:** MinGW64 (Windows), Homebrew+Xcode (macOS), GCC 7+ (Linux)
- **Build system:** Makefile with flags `-O3 -march=nehalem -funsafe-math-optimizations`

### DSP Stack
- **SIMD processing:** rack::simd::float_4 for 4-channel vectorization
- **Fast math:** dsp::exp2_taylor5() for V/oct→freq conversion (negligible <6e-06 error)
- **Anti-aliasing:** PolyBLEP for saw/square, PolyBLAMP for triangle (implement yourself - not built-in)
- **Reference implementations:** VCV Fundamental, Bogaudio, 21kHz Palm Loop, Befaco EvenVCO

### Panel Design
- **Tool:** Inkscape (recommended) with helper.py code generation
- **Panel dimensions:** 128.5mm height × (HP × 5.08mm) width (30 HP = 152.4mm)
- **SVG constraints:** Convert text to paths, simple 2-stop gradients only, no CSS styling
- **Component layer:** Red=params, Green=inputs, Blue=outputs, Magenta=lights

### Build Performance Expectations
- **Initial setup:** 15-60 minutes for toolchain
- **Plugin builds:** Seconds with parallel compilation (`make -j4`)
- **CPU target:** <15% for 8-voice full features (with SIMD optimization)

**Confidence:** HIGH on all stack decisions (verified via official VCV documentation and SDK)

---

## Table Stakes vs Differentiators

### Table Stakes (Must Have)
Features users expect from any serious polyphonic VCO module:

| Feature | Complexity | Why Critical |
|---------|------------|--------------|
| Polyphonic cable support (16 channels) | Medium | VCV Rack standard since v1.0; SIMD optimization critical |
| V/Oct pitch tracking | Low | Musical tuning standard; software advantage = no analog drift |
| Multiple waveforms (sin/tri/saw/sq) | Medium | Each needs proper antialiasing |
| PWM (Pulse Width Modulation) | Medium | Standard for square wave with CV control |
| Hard sync | High | Expected modulation; aliasing is major concern |
| Anti-aliasing (MinBLEP/PolyBLEP) | High | Required for harmonically rich waveforms above Nyquist |
| FM (Frequency Modulation) | Medium | Expected capability; through-zero is differentiator |
| Low CPU usage | High | Critical for 16-oscillator design; users track CPU% carefully |
| Clear labeling, proper spacing | Low | VCV standard: readable, adequate room for thumbs between knobs |

### Differentiators (What Makes This Special)
Features that set this module apart:

| Feature | Value Proposition | Complexity |
|---------|-------------------|------------|
| **Through-zero FM** | Musical FM stays in tune; wave shaping vs harsh detuning | High |
| **Dual oscillator per voice** | Richer sound; cross-modulation within voice | Medium |
| **Cross-modulation (cross-sync, cross-FM)** | Two oscillators modulating simultaneously; complex timbres | High |
| **XOR waveshaping** | Combines pulse waves with PWM interaction; novel sound design | High |
| **Soft sync** | Gentler harmonics than hard sync; timbral variation | High |
| **Analog modeling/character** | User-controllable drift, warmth, detuning (not always-on) | High |
| **Sub-oscillator** | One octave below; adds bass weight | Low-Medium |
| **Voicing modes (Poly/Dual/Unison)** | Flexibility for different playing styles | Medium |
| **Waveform mixing** | Simultaneous output of mixed waveforms | Low |
| **FM source selection** | Choose which waveform acts as modulator; timbral variety | Medium |
| **Efficient SIMD** | 25% CPU vs naive (4x improvement); competitive advantage | High |
| **Wide panel, visible controls** | No menu diving; ergonomic advantage | Low |

### Anti-Features (Deliberately Avoid)
- **Poor antialiasing** - Creates inharmonic tones; major quality issue
- **Always-on analog modeling** - Users complain about unwanted drift; make it OFF-able
- **Individual voice outputs (1-8 jacks)** - Panel bloat; poly cables solve this
- **Menu-diving for core features** - Poor UX; wide panel with visible controls
- **Only monophonic support** - Polyphony expected in 2026
- **Embedded fonts in SVG** - VCV can't render; convert text to paths
- **Cramped layout** - Can't operate controls comfortably

**Confidence:** MEDIUM on features (WebSearch verified with official sources, cross-referenced community)

---

## Architecture Highlights

### Component Layer Structure

```
MODULE LAYER (PolyDualVCO)
  ├── Input/output/parameter definitions
  ├── Polyphonic channel management (8 voices max)
  ├── Voicing mode logic (poly/dual/unison)
  └── Global mixing and routing
        ↓
VOICE PROCESSOR LAYER (DualVCOVoice<T>)
  ├── Two oscillator cores (VCO A, VCO B)
  ├── Cross-modulation routing (FM, sync, XOR)
  ├── Per-voice CV processing
  └── Per-voice mix output
        ↓
OSCILLATOR CORE LAYER (OscillatorCore<T>)
  ├── Phase accumulation (wrap every sample)
  ├── Waveform generation (saw/tri/sin/sq/sub)
  ├── Anti-aliasing (polyBLEP/polyBLAMP)
  └── PWM for square wave
        ↓
MODULATION LAYER
  ├── FM processor (exponential, linear, through-zero)
  ├── Sync processor (hard/soft/gradual)
  ├── Waveshaper (XOR, fold)
  └── Vibrato/detune effects
```

### SIMD Optimization Pattern

**Template-based architecture:**
```cpp
template<typename T = float>
class OscillatorCore {
    T phase = 0.f;

    struct Waveforms {
        T saw, tri, sin, sq, sub;
    };

    Waveforms process(T frequency, T pwm, T fmInput, bool syncReset, float sampleTime);
};
```

**Processing 8 voices with float_4:**
- Use 2 SIMD groups: voices 0-3, voices 4-7
- Process 4 channels per iteration: `for (int c = 0; c < channels; c += 4)`
- Use SIMD math: `simd::float_4`, `simd::sin()`, avoid branching
- Achievement: 25% CPU vs naive (4x improvement)

### Data Flow

1. **Read CV inputs** (polyphonic, per-channel): V/OCT, FM amount, sync, PWM, XOR, detune
2. **Voice processor iteration** (2 SIMD groups): Calculate frequencies (dsp::exp2_taylor5), apply modulation
3. **Oscillator processing** (per VCO): Phase accumulation → waveform generation → anti-aliasing
4. **Cross-modulation** (within voice): VCO A → FM/sync → VCO B, XOR waveshaping
5. **Waveform mixing** (per voice): Select/mix waveforms, apply analog character
6. **Output writing** (polyphonic): Set voltage per channel, set channel count

### Critical Patterns
- **V/Oct standard:** C4 = 0V = 261.6256 Hz, use fast dsp::exp2_taylor5()
- **Phase wrapping:** Every sample `if (phase >= 1.0f) phase -= 1.0f;` to prevent drift
- **Sync detection:** Schmitt trigger on rising edge (0V crossing from below)
- **Thread safety:** No NanoVG in process(), no module instance access, use expander messages
- **Sample rate:** Use `args.sampleTime` not global settings, recalc on onSampleRateChange()

**Confidence:** HIGH (based on official VCV documentation and multiple open-source implementations)

---

## Critical Pitfalls to Avoid

Ranked by impact and mapped to phases where they become critical:

### Phase 1: Core Oscillator (Must Get Right Initially)

**1. Missing Anti-aliasing on Discontinuous Waveforms**
- **Impact:** Module unusable above ~1kHz; harsh metallic artifacts; requires complete rewrite
- **Prevention:** Implement polyBLEP for saw/square, polyBLAMP for triangle; test with spectrum analyzer at >2kHz
- **Note:** VCV Rack does NOT include built-in anti-aliasing library

**2. Incorrect minBLEP Implementation (Double Discontinuity Bug)**
- **Impact:** Clicks/pops during PWM; affects Bog Audio, Slime Child, Instruo, Befaco
- **Prevention:** Only insert discontinuities where pre-antialiased value changes; call minBlepper.process() EVERY sample; study Befaco EvenVCO implementation, not VCV Fundamental (has bug)

**3. Incorrect Polyphony Channel Handling**
- **Impact:** Breaks user patches; polyphonic chord through mono module produces silence/wrong behavior
- **Prevention:** Audio inputs use getVoltageSum(), CV inputs use getPolyVoltage(channel); never sum CV

**4. Thread Safety Violations (GUI Access from DSP)**
- **Impact:** Random crashes, audio dropouts, data corruption
- **Prevention:** Keep ALL NanoVG in draw() only; never access GUI from process(); use expander messages for inter-module communication

**5. Denormal Numbers Causing CPU Spikes**
- **Impact:** CPU spikes to 100% when module idle; filters ring out to <1e-38 become 100x slower
- **Prevention:** Add tiny DC offset (1e-18) to feedback paths; detect silence and zero filter state

**6. Voltage Range Non-compliance**
- **Impact:** Breaks compatibility with other modules; too-hot signals clip downstream
- **Prevention:** Audio = ±5V, unipolar CV = 0-10V, bipolar CV = ±5V, gates = 10V/0V, C4 = 0V

**7. Phase Accumulation Numerical Drift**
- **Impact:** Pitch drifts sharp over minutes; low-frequency oscillators especially affected
- **Prevention:** Wrap phase to [0,1) EVERY sample; use double precision for LFO rates

**8. SIMD float_4 Branching**
- **Impact:** No performance benefit; 16-oscillator module as slow as 4x monophonic
- **Prevention:** Never branch on SIMD elements; use simd:: math functions; process all 4 channels together

### Phase 2: Modulation Features

**9. Through-Zero FM Aliasing**
- **Impact:** Severe aliasing during deep FM; unusable for characteristic through-zero sounds
- **Prevention:** Proper through-zero algorithm (phase runs backwards when frequency negative); apply polyBLEP or 2x-4x oversampling; test at audio-rate FM

**10. Hard Sync Triggering on Wrong Edge**
- **Impact:** Sync behavior doesn't match hardware/user expectations
- **Prevention:** Trigger on RISING edge (0V crossing from below); use Schmitt trigger with hysteresis

**11. PWM Clicks from Slew-Limiting Oversight**
- **Impact:** Audible clicks during PWM modulation
- **Prevention:** Apply small slew limiting to discontinuous jumps; ensure minBLEP discontinuities inserted for PWM transitions

**12. Triangle Wave Anti-aliasing Done Wrong**
- **Impact:** No anti-aliasing benefit despite using minBLEP; DC drift with modulation
- **Prevention:** Use polyBLAMP (integrates polyBLEP) or generate bandlimited square then integrate

### Phase 3: Voicing Modes

**13. Unison Voice Spreading Algorithm**
- **Impact:** Unison doesn't sound wide; center pitch shifts with odd/even voice counts
- **Prevention:** Symmetric spreading (voice pairs at +detune/-detune); odd channels preserve center pitch

### Cross-Phase Concerns

**14. Incorrect Sample Rate Handling**
- **Impact:** Pitch changes when user changes sample rate
- **Prevention:** Always use args.sampleTime from process args; override onSampleRateChange() to recalc coefficients

**15. NaN/Infinity Propagation**
- **Impact:** Patch goes silent or explosive noise; requires reload
- **Prevention:** Check for NaN/Inf in critical outputs: `if (!std::isfinite(output)) output = 0.f;`

**16. Oversampling CPU Cost Underestimation**
- **Impact:** 8x/16x oversampling with 16 oscillators = 50%+ CPU; module unusable
- **Prevention:** Make oversampling optional; use selectively for FM/sync only; 2x maximum default

### Phase 0: Panel Design (Before Coding)

**17. Panel Spacing Too Tight**
- **Impact:** Difficult to patch; can't adjust knobs with cables patched
- **Prevention:** Adequate space to "put thumbs between" knobs/ports per VCV guide

**18. SVG Panel Export Issues**
- **Impact:** Panel doesn't render or text appears wrong
- **Prevention:** Convert all text to paths; simple 2-stop gradients only; test in VCV Rack before finalizing

**Confidence:** HIGH for DSP/polyphony pitfalls (verified official docs), MEDIUM for performance (community-sourced)

---

## Recommended Phase Structure

Based on architectural dependencies and pitfall prevention, here's the optimal build order:

### Phase 0: Foundation & Panel Design (Before Code)
**Duration:** 1-2 days
**Deliverable:** Panel design, development environment verified

**Rationale:** Panel layout dictates feature accessibility and ergonomics. Must resolve spacing, SVG export, port distinction BEFORE coding to avoid rework.

**Tasks:**
- Install VCV Rack SDK 2.6+, toolchain (MinGW64/Homebrew/GCC), Inkscape
- Design panel in Inkscape (30 HP = 152.4mm width × 128.5mm height)
- Add components layer with color-coded shapes
- Convert text to paths, verify simple gradients
- Create plugin scaffold with helper.py
- Test build system: `make -j4` should compile

**Avoid pitfalls:** #17 (spacing), #18 (SVG export), #20 (port distinction)

---

### Phase 1: Core Oscillator Foundation (CRITICAL - Must Get Right)
**Duration:** 1-2 weeks
**Deliverable:** Single monophonic VCO with all waveforms, anti-aliasing verified

**Rationale:** This is the foundation. Anti-aliasing (#1, #2), polyphony architecture (#3), thread safety (#4), and phase handling (#7) CANNOT be retrofitted. Getting this wrong means complete rewrite.

**Tasks:**
1. Implement basic oscillator core with phase accumulator
2. Generate single waveform (sawtooth) with V/Oct tracking
3. **Implement polyBLEP anti-aliasing from first principles** (don't copy VCV Fundamental bug)
4. Add triangle (polyBLAMP), sine (inherent), square (polyBLEP) waveforms
5. Verify with spectrum analyzer at high frequencies (>2kHz)
6. Test phase wrapping stability over 10+ minutes continuous play
7. Define voltage ranges for all ports (±5V audio, 0-10V CV, C4=0V)
8. Verify thread safety (no GUI access from process())

**Features:** Single monophonic VCO, V/Oct tracking, 4 waveforms (saw/tri/sin/sq), anti-aliasing

**Avoid pitfalls:** #1, #2, #4, #6, #7, #13, #15 (all CRITICAL)

**Research needs:** Deep study of polyBLEP/polyBLAMP implementation; review Befaco EvenVCO, Bogaudio, 21kHz code

---

### Phase 2: SIMD Polyphonic Processing (ARCHITECTURAL MILESTONE)
**Duration:** 1 week
**Deliverable:** 8-voice polyphony with SIMD optimization, CPU profiled

**Rationale:** With 16 oscillators, SIMD is mandatory for acceptable CPU usage. This is a major architectural shift requiring oscillator core templating. Must happen BEFORE adding dual oscillators.

**Tasks:**
1. Template oscillator core: `OscillatorCore<typename T = float>`
2. Implement SIMD float_4 processing (process 4 voices at once)
3. Set up channel count management (driven by V/OCT input)
4. Verify polyphonic channel handling (audio=sum, CV=per-channel)
5. Profile CPU usage: target <5% for 8 voices baseline
6. Test with both monophonic and polyphonic patches
7. Verify SIMD matches scalar results (bit-exact for test signals)

**Features:** 8-voice polyphony, SIMD optimization, polyphonic cable support

**Avoid pitfalls:** #3, #8, #15, #16, #25 (polyphony and CPU concerns)

**Research needs:** VCV SIMD patterns, float_4 best practices, CPU profiling methodology

---

### Phase 3: Dual Oscillator Architecture (CORE VALUE)
**Duration:** 1 week
**Deliverable:** Two independent oscillators per voice with separate controls

**Rationale:** Dual-VCO-per-voice is the core value proposition. Enables all cross-modulation features. Must be stable before adding FM/sync complexity.

**Tasks:**
1. Duplicate oscillator core (VCO B) within voice processor
2. Independent pitch control for VCO A and VCO B
3. Separate waveform outputs or internal mixing
4. Voice processor coordination (DualVCOVoice class)
5. Verify both oscillators track pitch independently
6. Test detuning between VCO A and VCO B
7. Profile CPU: should be ~2x Phase 2 baseline

**Features:** Dual oscillator per voice (16 oscillators total), independent pitch control, detuning

**Avoid pitfalls:** #5 (denormals in dual paths), #25 (CPU budget with 16 oscillators)

**Research needs:** None (standard patterns)

---

### Phase 4: PWM and Sub-Oscillator (TABLE STAKES)
**Duration:** 3-5 days
**Deliverable:** PWM for square wave, sub-oscillator at -1 octave

**Rationale:** Table stakes features that complete basic dual-VCO functionality before adding complex modulation.

**Tasks:**
1. PWM parameter + CV input per VCO
2. Duty cycle modulation in square wave generator
3. Ensure minBLEP handles PWM transitions (avoid clicks)
4. Sub-oscillator (derived from main phase, not separate accumulator)
5. Verify sub tracks main oscillator exactly (-1 octave)
6. Test PWM with sine wave modulation (no clicks)

**Features:** PWM per VCO with CV, sub-oscillator

**Avoid pitfalls:** #11 (PWM clicks), #17 (sub-oscillator phase relationship)

**Research needs:** None (standard patterns)

---

### Phase 5: Through-Zero FM (DIFFERENTIATOR - HIGH COMPLEXITY)
**Duration:** 1-2 weeks
**Deliverable:** Musical through-zero FM with VCO A → VCO B modulation

**Rationale:** Key differentiator but technically challenging. Requires careful implementation to avoid severe aliasing. Separate phase ensures complexity doesn't block other features.

**Tasks:**
1. Implement exponential FM first (simpler, verify architecture)
2. Add linear FM implementation
3. Implement through-zero algorithm (phase runs backwards when freq negative)
4. FM amount CV input with attenuverter per voice
5. Apply anti-aliasing (polyBLEP or 2x oversampling for FM path)
6. Test at unison (both oscillators same pitch) - should produce wave shaping
7. Test at audio-rate FM with spectrum analyzer
8. Profile CPU impact of FM anti-aliasing

**Features:** Through-zero FM (VCO A → VCO B), exponential + linear modes, FM amount CV

**Avoid pitfalls:** #9 (through-zero FM aliasing - CRITICAL for this feature)

**Research needs:** DEEP - Through-zero FM algorithms, Bogaudio implementation study, anti-aliasing strategies for FM

---

### Phase 6: Hard Sync (TABLE STAKES - MODERATE COMPLEXITY)
**Duration:** 3-5 days
**Deliverable:** Hard sync (both directions: A→B and B→A)

**Rationale:** Expected modulation capability. Moderate aliasing concerns but simpler than FM.

**Tasks:**
1. Sync trigger detection (Schmitt trigger, rising edge)
2. Hard sync implementation (reset slave phase on trigger)
3. Sync routing: VCO A → VCO B sync
4. Optional: VCO B → VCO A sync (bidirectional)
5. Apply anti-aliasing to sync operations
6. Test with scope visualization to verify phase reset
7. Verify no clicks at sync points

**Features:** Hard sync (A→B, optionally B→A)

**Avoid pitfalls:** #10 (sync edge detection), #1 (sync creates discontinuities - needs anti-aliasing)

**Research needs:** Sync anti-aliasing strategies (likely same as Phase 5 FM research)

---

### Phase 7: Cross-Modulation Extensions (DIFFERENTIATOR - HIGH COMPLEXITY)
**Duration:** 1 week
**Deliverable:** Cross-sync, XOR waveshaping, FM source selection

**Rationale:** Unique features that differentiate from standard dual-VCOs. Build on FM/sync foundation.

**Tasks:**
1. Cross-sync: both VCOs sync simultaneously (careful - instability risk)
2. XOR waveshaper: combine pulse waves with PWM interaction
3. FM source selection: choose which VCO A waveform modulates VCO B
4. Anti-alias XOR output
5. Test for unstable feedback loops in cross-modulation
6. Verify CPU usage still acceptable

**Features:** Cross-sync, XOR waveshaping, FM source selection

**Avoid pitfalls:** #9 (XOR aliasing), #21 (NaN/Inf in complex modulation paths)

**Research needs:** XOR waveshaping implementation, BASTL THYME+ approach, Vessek cross-modulation patterns

---

### Phase 8: Voicing Modes (DIFFERENTIATOR - MODERATE COMPLEXITY)
**Duration:** 3-5 days
**Deliverable:** Poly/Dual/Unison modes with proper voice allocation

**Rationale:** Performance feature that unlocks different playing styles. Builds on polyphonic foundation.

**Tasks:**
1. Voicing mode parameter/switch
2. Polyphonic mode: standard channel assignment
3. Dual mode: independent control of VCO sets
4. Unison mode: all voices on one note with symmetric detuning
5. Implement Bogaudio-style detuning algorithm (preserves center pitch)
6. Test mode switching doesn't click
7. Verify CPU scales appropriately per mode

**Features:** Voicing modes (Poly/Dual/Unison), symmetric detuning for unison

**Avoid pitfalls:** #13 (unison spreading algorithm)

**Research needs:** Bogaudio UNISON detuning algorithm, stereo spreading strategies

---

### Phase 9: Polish & Quality-of-Life (OPTIONAL - LOW PRIORITY)
**Duration:** 2-4 days
**Deliverable:** Soft sync, vibrato, analog character, cable click prevention

**Rationale:** Nice-to-have features that improve UX but not critical for MVP.

**Tasks:**
1. Soft sync (reverse phase direction instead of reset)
2. Built-in vibrato with CV control
3. Analog character (user-controllable: OFF, subtle, strong)
4. Cable connection fade-in to prevent clicks
5. Final CPU profiling and optimization
6. Documentation and user testing

**Features:** Soft sync, vibrato, analog character (optional), polished UX

**Avoid pitfalls:** #23 (cable clicks)

**Research needs:** None (standard patterns)

---

## Phase Dependency Graph

```
Phase 0: Foundation & Panel
    ↓
Phase 1: Core Oscillator (CRITICAL - 1-2 weeks)
    ↓
Phase 2: SIMD Polyphony (ARCHITECTURAL MILESTONE - 1 week)
    ↓
Phase 3: Dual Oscillator (CORE VALUE - 1 week)
    ↓
Phase 4: PWM + Sub-Oscillator (TABLE STAKES - 3-5 days)
    ↓
    ├──────────────────┬──────────────────┐
    ↓                  ↓                  ↓
Phase 5: FM       Phase 6: Sync     Phase 8: Voicing
(1-2 weeks)       (3-5 days)        (3-5 days)
    │                  │
    └──────────────────┘
              ↓
    Phase 7: Cross-Modulation
         (1 week)
              ↓
    Phase 9: Polish (OPTIONAL)
         (2-4 days)
```

**Critical path:** 0 → 1 → 2 → 3 → 4 → 5 → 7 (6-8 weeks minimum)
**Parallel after Phase 4:** Phases 5, 6, 8 can be developed independently

---

## Research Flags: Which Phases Need Deeper Research

| Phase | Research Needed? | Topics | Confidence Gap |
|-------|------------------|--------|----------------|
| **Phase 0** | NO | Standard VCV setup, Inkscape panel design | HIGH confidence from docs |
| **Phase 1** | YES (CRITICAL) | PolyBLEP/polyBLAMP implementation details; VCV Fundamental minBLEP bug; Befaco EvenVCO study | MEDIUM - Must study multiple implementations to avoid bugs |
| **Phase 2** | MAYBE | SIMD float_4 best practices, CPU profiling methodology | MEDIUM-HIGH - Official docs cover basics, community has examples |
| **Phase 3** | NO | Standard dual-oscillator patterns | HIGH - Straightforward architecture |
| **Phase 4** | NO | PWM and sub-oscillator are standard | HIGH - Well-documented |
| **Phase 5** | YES (CRITICAL) | Through-zero FM algorithms, anti-aliasing for FM, Bogaudio implementation study | MEDIUM - Complex feature with limited documentation |
| **Phase 6** | MAYBE | Sync anti-aliasing strategies (likely covered in Phase 5) | MEDIUM - Similar to FM research |
| **Phase 7** | YES | XOR waveshaping implementation, BASTL THYME+ approach, cross-modulation stability | MEDIUM-LOW - Less documented, may need experimentation |
| **Phase 8** | MAYBE | Bogaudio UNISON detuning algorithm | MEDIUM-HIGH - Algorithm documented in community |
| **Phase 9** | NO | Standard polish patterns | HIGH - Well-understood |

**Recommendation:** Run `/gsd:research-phase` for:
- **Phase 1** (anti-aliasing) - MANDATORY before starting
- **Phase 5** (through-zero FM) - MANDATORY before starting
- **Phase 7** (XOR, cross-modulation) - Optional, can experiment during phase

---

## Confidence Assessment

| Area | Confidence | Notes | Gaps to Address |
|------|------------|-------|-----------------|
| **Stack (C++11, SDK 2.6, toolchain)** | HIGH | Verified via official VCV docs and SDK | None - well documented |
| **SIMD with float_4** | HIGH | Plugin API Guide with examples, community verification | None - examples available |
| **Anti-aliasing techniques** | MEDIUM | Multiple implementations available, but VCV Fundamental has bug | CRITICAL: Must study Befaco EvenVCO, Bogaudio before Phase 1 |
| **Polyphony architecture** | HIGH | Official Polyphony Manual, developer guidelines | None - clear patterns |
| **Through-zero FM implementation** | MEDIUM | Bogaudio/21kHz examples exist but need deep study | CRITICAL: Research Phase 5 specifics before starting |
| **Performance targets (CPU)** | MEDIUM | Based on documented SIMD speedups, but 16-oscillator case untested | Needs continuous profiling during build |
| **Build order dependencies** | MEDIUM | Inferred from architecture, not officially documented | Validate with community during Phase 1 |
| **Panel design (Inkscape/SVG)** | HIGH | Official Panel Guide with detailed workflow | None - straightforward |

**Overall Confidence:** MEDIUM-HIGH for core technology, MEDIUM for complex features (through-zero FM, anti-aliasing specifics)

---

## Open Questions & Gaps to Address

### Technical Verification Needed
1. **Does VCV Rack automatically set FTZ/DAZ CPU flags for denormal handling?** (Pitfall #5)
   - Action: Test filters ringing out to silence, monitor CPU usage
   - If not automatic, add `1e-18` DC offset to feedback paths

2. **What is realistic CPU budget for 16 oscillators at 48kHz with 2x oversampling?**
   - Action: Profile continuously during Phase 2-3, adjust targets
   - Preliminary target: <15% for 8-voice full features

3. **Is VCV Fundamental minBLEP bug fixed in latest version?**
   - Action: Check GitHub issue #140 status, verify with spectrum analyzer
   - Recommendation: Implement from first principles anyway, don't copy

4. **What specific through-zero FM algorithm do Bogaudio/21kHz use?**
   - Action: Deep code study during Phase 5 research
   - Gap: CRITICAL for Phase 5 success

### Design Decisions for Validation
1. **Unidirectional (A→B) or bidirectional (A↔B) cross-modulation?**
   - Recommendation: Unidirectional matches Triax 8 hardware, simpler DSP
   - Optional: Add mode switch for bidirectional later

2. **Oversampling strategy: always-on, optional, or per-feature?**
   - Recommendation: Optional per context menu, default OFF or 2x maximum
   - Apply selectively to FM/sync paths only if needed

3. **Analog character: always-on or user-controllable?**
   - Recommendation: User-controllable with OFF position (research consistent)
   - Anti-feature: Always-on drift annoys users

### Feature Scope for MVP
**Defer to post-MVP:**
- Soft sync (nice-to-have, not critical)
- Vibrato (can patch externally)
- Analog character (clean digital is advantage)
- Advanced voicing modes beyond basic poly/unison

**Include in MVP (high value):**
- Through-zero FM (key differentiator)
- Cross-sync and XOR (unique architecture value)
- Basic voicing modes (poly/unison minimum)

---

## Sources & References

All findings synthesized from verified VCV Rack documentation and community resources:

### Official VCV Rack Documentation (HIGH confidence)
- VCV Rack Plugin Development Tutorial, Building Guide, Plugin API Guide
- VCV Rack DSP Manual, Polyphony Guide, Panel Design Guide
- VCV Rack Voltage Standards, Versioning
- VCV Rack 2.6 Release Notes (November 2024)

### GitHub Repositories (HIGH confidence)
- VCV Rack Source, VCV Fundamental (Official Modules)
- Bogaudio Modules, SquinkyLabs Modules, 21kHz Modules
- VCV Fundamental Issue #140 (minBLEP bug documentation)

### Community Resources (MEDIUM-HIGH confidence)
- VCV Community Development Forum (C++ standards, polyphony patterns, SIMD)
- Developer discussions on anti-aliasing, performance, workflow

### Technical References (MEDIUM confidence for VCV specifics)
- EarLevel Engineering (denormal numbers)
- General DSP knowledge (phase accumulation, filter design, anti-aliasing theory)

---

## Ready for Requirements

This research provides sufficient foundation to proceed with requirements definition and roadmap creation. Key takeaways for roadmapper:

1. **Phase 1 is make-or-break** - Anti-aliasing, polyphony, thread safety cannot be retrofitted
2. **SIMD is mandatory** - 16 oscillators without float_4 optimization = unusable CPU
3. **Through-zero FM is high-value, high-complexity** - Requires dedicated research phase
4. **Build order matters** - Dependencies enforce 1→2→3→4 critical path before features diverge
5. **CPU budget drives architecture** - Profile early, optimize continuously, target <15% for full features

**Next step:** Roadmapper agent creates phase-by-phase plan with success criteria, validation steps, and research integration points.

---

**END OF RESEARCH SUMMARY**
