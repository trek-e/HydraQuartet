# Code Review: HydraQuartet VCO

**Reviewed:** 2026-03-12  
**Scope:** `src/plugin.hpp`, `src/plugin.cpp`, `src/HydraQuartetVCO.cpp`  
**Fixes applied:** 2026-03-12 (all issues addressed)

---

## Summary

The module is a well-structured 8-voice polyphonic dual-VCO with SIMD, MinBLEP antialiasing, through-zero FM, hard/soft sync, and XOR output. The following issues should be addressed for correctness, portability, and consistency.

---

## Critical issues

### 1. **Intel SSE intrinsic breaks ARM (e.g. Apple Silicon)**

**Location:** `HydraQuartetVCO.cpp` lines 735–736

```cpp
mixSum.v = _mm_hadd_ps(mixSum.v, mixSum.v);
mixSum.v = _mm_hadd_ps(mixSum.v, mixSum.v);
```

`_mm_hadd_ps` is an Intel SSE3 intrinsic. It is not available on ARM (e.g. Rack’s Mac ARM64 build). The plugin will fail to compile or link on non-x86 platforms.

**Recommendation:** Use a portable horizontal sum. For example, sum the lanes in scalar code:

```cpp
float mixOut = mixSum[0] + mixSum[1] + mixSum[2] + mixSum[3];
```

If the Rack SDK exposes a SIMD horizontal-sum helper for `float_4`, use that instead so you stay portable and keep SIMD where possible.

---

### 2. **Unqualified `clamp` may be undefined**

**Location:** `HydraQuartetVCO.cpp` line 195 (inside `VcoEngine::applySync`)

```cpp
subsample = clamp(subsample, -1.f + 1e-6f, 0.f);  // Ensure valid range
```

`clamp` is used without a namespace. C++11 does not have `std::clamp` (it was added in C++17). If `rack.hpp` (or another header) does not define `clamp`, this can cause a compile error.

**Recommendation:** Use a form that is clearly defined in your build, e.g.:

- If the Rack/math API provides a float `clamp`, use it explicitly (e.g. `math::clamp` or whatever the SDK uses).
- Otherwise use a manual clamp:  
  `subsample = std::max(-1.f + 1e-6f, std::min(subsample, 0.f));`  
  and include `<algorithm>` if you use `std::min`/`std::max`.

---

## Medium issues

### 3. **License and copyright mismatch**

**Location:**  
- `HydraQuartetVCO.cpp` lines 1–15: Apache-2.0 header  
- `README.md`, `plugin.json`, `LICENSE`: GPL-3.0-or-later

The source file states Apache-2.0 while the project is GPL-3.0-or-later. This is inconsistent and can cause licensing confusion.

**Recommendation:** Replace the Apache-2.0 block in `HydraQuartetVCO.cpp` with a GPL-3.0-or-later notice that matches `LICENSE` and the rest of the project (e.g. the standard GPL “How to Apply” notice).

---

### 4. **Partial polyphony when channels is not a multiple of 4**

**Location:** `HydraQuartetVCO.cpp` process loop (e.g. `setVoltageSimd(mixed, c)` and per-voice DC filter loop)

When `channels` is not a multiple of 4 (e.g. 5, 6, 7), the last SIMD group still computes and writes four lanes via `setVoltageSimd(mixed, c)`. Lanes beyond the actual channel count (e.g. indices 5, 6, 7 when `channels == 5`) are never cleared and can contain garbage. `setChannels(channels)` tells the host how many channels are valid, but the underlying buffer may still hold undefined values for the extra lanes.

**Recommendation:** For the last SIMD group, when `groupChannels < 4`, either:

- Zero or clamp the unused lanes of `mixed` before calling `setVoltageSimd`, or  
- Write only the valid channels with `setVoltage(mixed[i], c + i)` for `i < groupChannels` and leave the rest unchanged (if the host ignores them).

This keeps behavior and potential future uses of the buffer well-defined.

---

### 5. **VCO2 octave range vs VCO1**

**Location:** `HydraQuartetVCO.cpp` line 355

- VCO1: `configSwitch(OCTAVE1_PARAM, -2.f, 2.f, 0.f, ...)` → 5 positions (16′, 8′, 4′, 2′, 1′).  
- VCO2: `configSwitch(OCTAVE2_PARAM, -2.f, 1.f, 0.f, ...)` → 4 positions (16′, 8′, 4′, 2′).

VCO2 cannot reach the highest octave (1′). If that’s intentional (e.g. design choice for FM), consider documenting it in a comment or the UI; otherwise consider aligning the range with VCO1.

---

## Minor / style

### 6. **`M_PI` portability**

**Locations:** e.g. lines 181, 484, 563

`M_PI` is a common extension (POSIX) but not part of standard C++. Some compilers require `_USE_MATH_DEFINES` (e.g. MSVC) or use a different constant.

**Recommendation:** If the Rack SDK guarantees `M_PI`, this is fine. Otherwise consider a local constant (e.g. `const float PI = 3.14159265358979323846f;`) or the constant provided by the SDK to avoid platform quirks.

---

### 7. **`using namespace rack` in header**

**Location:** `plugin.hpp` line 6

`using namespace rack;` in a header pulls the whole `rack` namespace into every translation unit that includes `plugin.hpp`, which can lead to name clashes or ambiguity in large projects.

**Recommendation:** Prefer `using` only in `.cpp` files, or qualify names in the header (e.g. `rack::Plugin*`, `rack::Model*`) and keep `using namespace rack` in the implementation.

---

### 8. **README build instructions**

**Location:** `README.md` “Building from Source”

The README says to clone “HydraQuartet”; the repo is under a `vco` folder. If the real repo is “HydraQuartet” and this is a subfolder, the clone path might be correct but the `cd` step may need to mention the subfolder (e.g. `cd HydraQuartet/vco` or similar) so users land in the right directory for `make`.

---

## Positive notes

- Clear separation of `VcoEngine` (DSP) and module/widget.
- SIMD used consistently (float_4, `getPolyVoltageSimd`, `setVoltageSimd`).
- MinBLEP and sync handling are well integrated (strided buffers, wrap/edge detection).
- Outputs and sub-output are sanitized with `std::isfinite` to avoid propagating NaN/Inf.
- DC filter and soft clipping are applied per-voice before output.
- Through-zero FM and sync logic (hard/soft, both directions) are implemented in a readable way.

---

## Suggested fix order

1. **Portable mix sum** – Replace `_mm_hadd_ps` with a portable horizontal sum (critical for ARM).
2. **`clamp`** – Resolve unqualified `clamp` (critical for portability and C++11).
3. **License** – Align source file header with GPL-3.0-or-later.
4. **Partial SIMD group** – Define behavior for the last group when `channels % 4 != 0`.
5. **VCO2 octave** – Confirm design and document or align with VCO1.
6. **Header namespace** – Remove or narrow `using namespace rack` in `plugin.hpp`.
7. **README** – Clarify clone path and directory for `make`.

If you want, the next step can be concrete patches for (1) and (2) only, or for all of the above.
