# Feature Landscape: VCV Rack Polyphonic Oscillators

**Domain:** VCV Rack Polyphonic VCO Modules
**Researched:** 2026-01-22
**Confidence:** MEDIUM (WebSearch verified with official sources, cross-referenced with community discussions)

## Table Stakes

Features users expect from any serious polyphonic VCO module. Missing these = module feels incomplete.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **Polyphonic cable support** | Core VCV Rack standard since v1.0; up to 16 channels | Medium | SIMD optimization critical for CPU efficiency (4x improvement possible) |
| **V/Oct pitch tracking** | Musical tuning standard; users expect accurate tracking | Low | Software advantage: no analog drift unless explicitly modeled |
| **Multiple waveform outputs** | Standard expectation: sine, triangle, saw, square minimum | Medium | Each waveform needs proper antialiasing |
| **PWM (Pulse Width Modulation)** | Standard feature on square wave output with CV control | Medium | Applies to square wave output only |
| **Hard sync** | Expected modulation option; resets oscillator phase on trigger | High | Aliasing is major concern; needs proper antialiasing |
| **Antialiasing** | Required for harmonically rich waveforms above Nyquist frequency | High | MinBLEP is current standard; alternative is band limiting + oversampling |
| **FM (Frequency Modulation)** | Expected modulation capability; exponential and/or linear | Medium | Through-zero FM is differentiator, not table stakes |
| **Pitch CV input** | V/Oct input with attenuverter or dedicated FM input | Low | Polyphonic CV propagation required |
| **Low CPU usage** | Polyphonic modules multiply computational cost by voice count | High | Critical for 16-oscillator design; users track CPU % carefully |
| **Clear labeling** | Succinct labels stating purpose of knobs, switches, ports | Low | VCV standard: readable at 100% on non-high-DPI monitor |
| **Proper spacing** | Room between knobs/ports to place thumbs | Low | Design "as if designing hardware" per VCV guidelines |
| **Output port distinction** | Visual differentiation (inverted background for outputs) | Low | VCV design standard |

## Differentiators

Features that set products apart. Not expected by default, but highly valued when present.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Through-zero FM** | Musical FM that stays in tune when oscillators at same pitch; wave shaping vs harsh detuning | High | Requires linear FM implementation; distinguishes quality oscillators |
| **Dual oscillator per voice** | Richer sound; enables cross-modulation within single voice | Medium | Unique architecture; most poly VCOs are single-osc-per-voice |
| **Cross-modulation (cross-sync, cross-FM)** | Two oscillators modulating each other simultaneously | High | Creates complex timbres; see Vessek, Tachyon Entangler examples |
| **XOR waveshaping** | Combines pulse waves of both oscillators; PWM affects XOR output | High | Novel sound design capability; uncommon in VCV ecosystem |
| **Soft sync** | Reverses waveform direction instead of phase reset; gentler harmonics than hard sync | High | Less harsh than hard sync; good timbral variation |
| **Analog modeling/character** | Drift, warmth, per-voice detuning; models analog imperfections | High | User-controllable is differentiator; always-on can be annoyance |
| **Sub-oscillator** | One octave below main oscillator; adds bass weight | Low-Medium | Common in commercial modules (KlirrFactory, SynthTech); less common in free |
| **Voicing modes** (Poly/Dual/Unison) | Flexibility: standard poly, double voices per note, or mono unison | Medium | Powerful performance feature; enables different playing styles |
| **Waveform mixing** | Simultaneous output of multiple waveforms mixed to single output | Low | More flexible than crossfading or switching |
| **FM source selection** | Choose which waveform acts as FM modulator | Medium | Timbral variety; different waveforms = different FM character |
| **Vibrato with CV** | Built-in vibrato amount control + CV input per oscillator | Low-Medium | Convenience feature; saves patching external LFO |
| **Fine tuning control** | Precise detuning for FM ratios and beating effects | Low | Critical for musical FM; 0-1 semitone smooth, beyond that stepped |
| **Polyphonic + global CV** | Both per-voice and global control (e.g., FM depth) | Medium | Ergonomic: global sweeps + per-note expression |
| **Efficient SIMD implementation** | 25% CPU vs naive implementation (4x improvement) | High | Users care about CPU; polyphonic oscillators are expensive |
| **Wide panel with visible controls** | All parameters accessible without menu diving | Low | UX advantage; some modules hide features in right-click menus |

## Anti-Features

Features to deliberately NOT build. Common mistakes or things that annoy users.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| **Poor antialiasing** | Aliasing creates inharmonic tones; major quality issue | Use MinBLEP or band limiting + oversampling; don't skip this |
| **Always-on analog modeling** | Users complain about unwanted drift/instability; not everyone wants "analog" | Make analog character user-controllable with OFF position |
| **Individual voice outputs (1-8 jacks)** | Panel bloat; VCV poly cables solve this | Use polyphonic output cable; leverage VCV ecosystem |
| **Menu-diving for core features** | Poor UX; users want visible controls | Wide panel with all controls accessible; right-click for utilities only |
| **Ignoring CPU optimization** | Users actively avoid CPU-heavy modules; complaints in forums | SIMD optimization, efficient antialiasing, performance profiling |
| **Oscillator sync without antialiasing** | Sync creates extreme aliasing artifacts | Critical to antialias sync operations; documented issue in Fundamental VCO |
| **Non-standard V/Oct behavior** | Users expect accurate musical tuning | Follow VCV voltage standards precisely |
| **Oversampling without band limiting** | Inefficient; wastes CPU for marginal benefit | Combine band limiting + oversampling for CPU efficiency (Bogaudio approach) |
| **Only supporting monophonic** | In 2026, polyphony is expected in VCV Rack | Build polyphonic from the start; use VCV's polyphonic cable system |
| **Embedded fonts in SVG** | Technical failure; VCV can't render embedded fonts | Convert all text to paths in Inkscape (Path > Object to Path) |
| **Complex gradients in panel** | Rendering issues in VCV's SVG renderer | Simple two-color linear gradients only |
| **Unclear labeling** | Users can't figure out what controls do | Succinct labels; follow VCV panel guidelines |
| **Cramped layout** | Can't operate controls comfortably | Adequate spacing "to put thumbs between" knobs/ports |

## Complexity Analysis

### High Complexity Features (Hardest to Implement Well)

**1. Through-Zero FM (Complexity: HIGH, Priority: CRITICAL)**
- **Why hard:** Requires linear FM implementation that maintains tuning when oscillators at same pitch
- **Quality bar:** FM should produce wave shaping in tune, not harsh detuning
- **Risk:** Poor implementation sounds bad; users will notice immediately
- **Mitigation:** Study Bogaudio implementation; test extensively with oscillators at unison

**2. Antialiasing for Sync Operations (Complexity: HIGH, Priority: CRITICAL)**
- **Why hard:** Oscillator sync creates extreme aliasing; documented issue in Fundamental VCO
- **Quality bar:** Sync should sound clean, not produce inharmonic artifacts
- **Risk:** Without proper antialiasing, sync modes will be unusable
- **Mitigation:** MinBLEP approach; study Befaco EvenVCO (noted as nearly perfect)

**3. CPU Optimization for 16 Oscillators (Complexity: HIGH, Priority: CRITICAL)**
- **Why hard:** 16 oscillators × 4 waveforms each = 64+ waveform generators
- **Quality bar:** Users expect <30% CPU for typical patches
- **Risk:** CPU-heavy modules get avoided; complaints in forums
- **Mitigation:** SIMD from day one; profile early; study Vult modules (compiled for performance)

**4. Cross-Modulation (Cross-Sync, Cross-FM) (Complexity: HIGH, Priority: MEDIUM)**
- **Why hard:** Two oscillators modulating each other simultaneously; feedback concerns
- **Quality bar:** Musical, stable results; no runaway feedback
- **Risk:** Unstable feedback loops; aliasing compounds
- **Mitigation:** Study Vessek implementation (gradual sync); careful feedback limiting

**5. XOR Waveshaping (Complexity: HIGH, Priority: MEDIUM)**
- **Why hard:** Combining pulse waves of both oscillators; PWM interaction complex
- **Quality bar:** XOR output responds musically to PWM changes on both VCOs
- **Risk:** Aliasing from XOR operation; needs antialiasing
- **Mitigation:** Research BASTL THYME+ approach; antialias XOR output

**6. Analog Character Modeling (Complexity: HIGH, Priority: LOW)**
- **Why hard:** Modeling drift, warmth, detuning without making it sound broken
- **Quality bar:** Should "pass a blind test" (Vessek standard)
- **Risk:** Can sound unstable or annoying if always-on
- **Mitigation:** User-controllable; OFF position; study Surge XT Character feature

### Medium Complexity Features

**7. Polyphonic Cable Support with SIMD (Complexity: MEDIUM, Priority: CRITICAL)**
- **Why moderate:** VCV provides polyphonic infrastructure, but SIMD optimization requires effort
- **Quality bar:** 25% CPU of naive implementation (4x improvement via SIMD)
- **Mitigation:** Use VCV's SIMD utilities; study open-source polyphonic modules

**8. Voicing Modes (Poly/Dual/Unison) (Complexity: MEDIUM, Priority: MEDIUM)**
- **Why moderate:** Voice allocation logic; stereo spreading for unison
- **Quality bar:** Smooth transitions; no clicks when switching modes
- **Mitigation:** Study Bogaudio UNISON module; implement detune spreading algorithm

**9. Soft Sync (Complexity: MEDIUM, Priority: LOW)**
- **Why moderate:** Reverses waveform direction instead of phase reset; different algorithm than hard sync
- **Quality bar:** Smoother, less harsh than hard sync; fewer harmonics
- **Mitigation:** Reference Vessek "gradual sync" approach

### Low Complexity Features

**10. Sub-Oscillator (Complexity: LOW-MEDIUM, Priority: LOW)**
- **Why easy:** Octave divider; can be implemented as separate oscillator -1 octave
- **Quality bar:** Tracks main oscillator; clean sine or square
- **Mitigation:** Dedicated oscillator instance at -12 semitones

**11. Standard Waveforms + Mixing (Complexity: LOW-MEDIUM, Priority: HIGH)**
- **Why easy:** VCV provides example implementations; mixing is straightforward
- **Quality bar:** Clean waveforms with proper antialiasing
- **Mitigation:** Use VCV's oscillator DSP utilities

**12. Panel Design (Spacing, Labels, Colors) (Complexity: LOW, Priority: HIGH)**
- **Why easy:** VCV provides clear guidelines and helper scripts
- **Quality bar:** Follow VCV manual standards exactly
- **Mitigation:** Use Inkscape; convert text to paths; follow color codes for component types

## Feature Dependencies

```
CRITICAL PATH (must work first):
┌─────────────────────────────────────────────────────┐
│ Polyphonic Cable Support (foundation)              │
└─────────────┬───────────────────────────────────────┘
              │
              v
┌─────────────────────────────────────────────────────┐
│ Single VCO with Antialiasing (prove it works)      │
│ - V/Oct tracking                                    │
│ - Basic waveforms (sin, tri, saw, sq)              │
│ - MinBLEP antialiasing                              │
└─────────────┬───────────────────────────────────────┘
              │
              v
┌─────────────────────────────────────────────────────┐
│ SIMD Optimization (make it efficient)               │
│ - Required BEFORE scaling to 16 oscillators         │
└─────────────┬───────────────────────────────────────┘
              │
              v
┌─────────────────────────────────────────────────────┐
│ Dual VCO Architecture (two oscillators per voice)   │
└─────────────┬───────────────────────────────────────┘
              │
              ├──> Through-Zero FM ──> FM Source Selection
              │
              ├──> Hard Sync ──> Soft Sync (optional)
              │
              ├──> Cross-Modulation (cross-sync + cross-FM)
              │
              └──> XOR Waveshaping (uses both VCO pulse waves)

INDEPENDENT FEATURES (can be added anytime):
- PWM (square wave manipulation)
- Sub-oscillator (additional oscillator -1 octave)
- Vibrato (LFO integration)
- Voicing Modes (voice allocation logic)
- Analog Character (modulation of base parameters)
- Waveform Mixing (output mixing)
- Fine Tuning (pitch offset)

DEPENDENT FEATURES (require prior features):
┌───────────────────────────────────────┐
│ Through-Zero FM                       │
│ DEPENDS ON: V/Oct tracking, antialiasing │
└───────────────────────────────────────┘

┌───────────────────────────────────────┐
│ Cross-Sync                            │
│ DEPENDS ON: Dual VCO, Hard Sync working │
└───────────────────────────────────────┘

┌───────────────────────────────────────┐
│ XOR Waveshaping                       │
│ DEPENDS ON: Dual VCO, PWM, antialiasing │
└───────────────────────────────────────┘

┌───────────────────────────────────────┐
│ Voicing Modes (Unison)                │
│ DEPENDS ON: Polyphony working, detuning │
└───────────────────────────────────────┘

┌───────────────────────────────────────┐
│ FM Source Selection                   │
│ DEPENDS ON: Through-Zero FM, waveform routing │
└───────────────────────────────────────┘
```

## MVP Recommendation

For MVP (Minimum Viable Product), prioritize in this order:

### Phase 1: Foundation (MUST HAVE)
1. **Polyphonic cable support** (16 channels, VCV standard)
2. **Single VCO with V/Oct tracking** (prove musical tuning works)
3. **Four waveforms** (sine, triangle, saw, square with individual outputs)
4. **MinBLEP antialiasing** (quality bar for waveforms)
5. **SIMD optimization** (BEFORE scaling to multiple oscillators)
6. **Basic panel design** (follow VCV guidelines, adequate spacing)

### Phase 2: Dual VCO Architecture (CORE VALUE)
7. **Dual VCO per voice** (16 oscillators total, 8 voices)
8. **Through-zero FM** (VCO1 → VCO2, with FM amount control)
9. **Hard sync** (both directions: VCO1→VCO2 and VCO2→VCO1)
10. **PWM** (per VCO, with CV input)
11. **Waveform mixing** (mix all four waveforms to single output)

### Phase 3: Differentiators (COMPETITIVE ADVANTAGE)
12. **Cross-modulation** (cross-sync: both VCOs sync simultaneously)
13. **XOR waveshaping** (combines pulse waves with PWM interaction)
14. **FM source selection** (choose which VCO1 waveform modulates VCO2)
15. **Voicing modes** (Poly/Dual/Unison)

### Defer to Post-MVP:
- **Soft sync**: Medium value, high complexity; hard sync sufficient for MVP
- **Analog character**: Low priority; clean digital is advantage
- **Sub-oscillator**: Nice-to-have; not core to dual-VCO value proposition
- **Vibrato**: Can be patched externally; convenience feature
- **Global + per-voice CV**: Start with one or the other; add both later

## Feature Interaction Notes

**Through-Zero FM + Fine Tuning:**
- Fine tuning control is critical for musical FM
- Creates frequency ratios between VCO1 and VCO2
- 0-1 semitone range should be smooth (not stepped)
- Beyond 1 semitone: stepped in semitone increments to +1 octave

**XOR + PWM:**
- XOR output depends on pulse waves from both VCOs
- PWM on VCO1 affects XOR output
- PWM on VCO2 affects XOR output
- User can shape XOR timbre with both PWM controls
- Needs antialiasing or will sound harsh

**Cross-Sync + Cross-Modulation:**
- Both VCOs can sync to each other simultaneously
- Creates complex, evolving timbres
- Risk of instability; needs careful implementation
- Vessek uses "gradual sync" to control intensity

**Voicing Modes + Detuning:**
- Unison mode: 16 oscillators on one note
- Requires stereo spreading/detuning or sounds flat
- Bogaudio UNISON: detunes ±50 cents maximum
- Channel count affects detune distribution (see Bogaudio algorithm)

**Analog Character + Stability:**
- User-controllable is key differentiator
- OFF position = clean digital (advantage of software)
- ON position = drift, warmth, per-voice detuning
- Should NOT affect tuning accuracy when off
- Surge XT approach: "Character" filters (warmth/brightness) + optional "Oscillator Drift"

## Sources

### Official Documentation
- [VCV Rack Polyphony Manual](https://vcvrack.com/manual/Polyphony)
- [VCV Rack Module Panel Guide](https://vcvrack.com/manual/Panel)
- [VCV Rack DSP Guide](https://vcvrack.com/manual/DSP)

### Oscillator Module Examples
- [Bogaudio Modules GitHub](https://github.com/bogaudio/BogaudioModules)
- [Vult Vessek Oscillator](https://modlfo.github.io/VultModules/vessek/)
- [VCV Library - Oscillators](https://library.vcvrack.com/?tag=Oscillator)
- [Surge XT Modules Manual](https://surge-synthesizer.github.io/rack_xt_manual/)

### Technical Discussions
- [VCV Community - Analog Modeled Oscillators](https://community.vcvrack.com/t/analog-modelled-oscillators/17299)
- [VCV Community - Detune and Unison](https://community.vcvrack.com/t/detune-and-unison/6072)
- [VCV Community - Wide Range Oscillator](https://community.vcvrack.com/t/wide-range-oscillator/15763)
- [VCV Community - Waveshaping with XOR](https://community.vcvrack.com/t/waveshaping-with-xor-modulation/22717)
- [GitHub Issue - Fundamental VCO Sync Aliasing](https://github.com/VCVRack/Fundamental/issues/87)

### Antialiasing & Performance
- [VCV Community - Help with Aliasing](https://community.vcvrack.com/t/help-dealing-with-aliasing/21406)
- [VCV Community - Anti-aliasing as Module](https://community.vcvrack.com/t/anti-aliasing-as-a-discrete-module/17035)
- [VCV Community - MinBLEP Implementation](https://community.vcvrack.com/t/help-me-understand-minblep-implementation/16673)
- [VCV Community - Max Practical CPU Usage](https://community.vcvrack.com/t/max-practical-cpu-usage-to-aim-for/10999)

### Module Comparisons & Reviews
- [ExpertBeacon - Best VCV Rack Modules](https://expertbeacon.com/the-ultimate-guide-to-the-best-vcv-rack-modules/)
- [Studio Brootle - Best VCV Rack Modules](https://www.studiobrootle.com/best-vcv-rack-modules/)
- [VCV Community - Recommended Premium Modules](https://community.vcvrack.com/t/recommended-premium-modules-for-basic-synthesis/17733)
