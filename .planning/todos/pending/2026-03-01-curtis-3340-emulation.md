---
created: 2026-03-01T05:09:06.202Z
title: Curtis 3340 emulation
area: general
files:
  - src/HydraQuartetVCO.cpp
---

## Problem

The HydraQuartet VCO currently produces clean digital waveforms. The PROJECT.md lists "analog character" controls (CHAR-01, CHAR-02) as v2 deferred features, and the module's inspiration — the TipTop Triax 8 — is built around CEM3340-style oscillators with inherent analog warmth. Adding Curtis CEM3340 emulation would bring vintage analog character: slight pitch drift between voices, waveform imperfections (soft saw edges, triangle rounding), and temperature-dependent tuning instability.

## Solution

Research CEM3340 analog behavior and implement as a user-controllable "analog character" knob:
- Per-voice micro-detuning (random walk drift)
- Waveform shape imperfections (capacitor charging curves on saw, triangle asymmetry)
- Optional thermal drift simulation
- Reference: Befaco EvenVCO, Fundamental VCO analog mode implementations
- Aligns with deferred requirements CHAR-01, CHAR-02 from PROJECT.md
