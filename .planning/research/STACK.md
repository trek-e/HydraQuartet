# Stack Research: VCV Rack Polyphonic VCO Module

**Project:** Triax VCO (8-voice polyphonic dual-VCO)
**Platform:** VCV Rack 2.x
**Researched:** 2026-01-22
**Current Rack Version:** 2.6.6 (released November 2024)

## Executive Summary

VCV Rack module development uses a mature, stable toolchain centered on **C++11** with **Makefile-based builds** and the **VCV Rack SDK**. For a polyphonic oscillator with 16 oscillators total, the critical stack decisions are:

1. **C++11 standard** (required for plugin library distribution)
2. **VCV Rack SDK 2.6+** with built-in DSP utilities
3. **SIMD optimization via float_4** for polyphonic processing (essential for performance)
4. **MinBLEP/PolyBLEP antialiasing** (implement yourself or reference open-source modules)
5. **Inkscape for SVG panel design** with helper.py code generation
6. **Platform-specific compiler toolchains** (MinGW64/GCC/Clang)

---

## Recommended Stack

### Core Framework & SDK

| Technology | Version | Purpose | Rationale |
|------------|---------|---------|-----------|
| **VCV Rack SDK** | 2.6+ | Module development framework | Latest stable release (Nov 2024); includes all DSP utilities, polyphonic cable API, SIMD helpers. SDK is pre-built for each platform — no need to compile Rack itself. |
| **C++ Standard** | C++11 | Language standard | **REQUIRED** for plugin library distribution. VCV's build system uses libstdc++ 5.4.0 on Linux which limits to C++11. You can use C++14/17 locally but cannot distribute to the official VCV Library. |
| **Python** | 3.x | Build tooling | Required for helper.py script (SVG→C++ code generation, plugin scaffolding). |

**Confidence:** HIGH (verified via official VCV documentation and developer statements)

### Build System & Compilers

| Platform | Toolchain | Version Notes | Installation |
|----------|-----------|---------------|--------------|
| **Windows** | MSYS2 + MinGW64 | GCC 64-bit | Install MSYS2, then: `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gdb mingw-w64-x86_64-cmake git make tar autoconf automake libtool` |
| **macOS** | Homebrew + Xcode CLI | Clang via Xcode | Install Homebrew, then: `brew install git wget cmake autoconf automake libtool jq python zstd pkg-config` |
| **Linux** | GCC/G++ | GCC 7+ recommended | Ubuntu: `apt install build-essential git curl cmake libgl1-mesa-dev libglu1-mesa-dev`<br>Arch: `pacman -S git wget gcc gdb make cmake tar unzip zip curl jq python zstd` |

**Build Flags (from VCV Rack):**
- `-O3` (optimization level 3)
- `-march=nehalem` (enables SSE up to SSE4.2, guarantees float_4 SIMD compatibility)
- `-funsafe-math-optimizations` (enables auto-vectorization by compiler)

**Build Commands:**
```bash
export RACK_DIR=/path/to/Rack-SDK
make                  # Build plugin
make install          # Install to local Rack plugins folder
make dist             # Create distributable plugin package
make clean            # Clean build artifacts
```

**Build Performance:**
- Append `-j<cores>` to make commands (e.g., `make -j4`) for parallel compilation
- Initial dependency build: 15-60 minutes
- Plugin builds: seconds to minutes depending on complexity

**Critical Constraints:**
- **No spaces in file paths** (breaks Makefile)
- **Disable antivirus on Windows** during builds (can interfere)
- Use MSYS2 MinGW64 shell on Windows (NOT MSYS shell)

**Confidence:** HIGH (verified via official Building documentation)

### DSP Libraries & Utilities

| Component | Source | Purpose | Notes |
|-----------|--------|---------|-------|
| **VCV Rack DSP headers** | Built into SDK | Oscillators, filters, SIMD utilities, triggers | `#include <dsp/common.hpp>`, `<dsp/digital.hpp>`, `<dsp/resampler.hpp>`, etc. |
| **pffft** | Bundled in SDK (dep/) | Fast Fourier Transform | Pre-compiled in SDK dependencies. Use for spectral processing if needed. |
| **SIMD types: float_4** | `rack::simd` namespace | Process 4 channels simultaneously | SSE4.2-compatible. Use for polyphonic processing. `simd::float_4`, `simd::int32_4` with operators `+ - * /` and functions `sin()`, `exp()`, `floor()` via `simd::` namespace. |
| **Fast math approximations** | `rack::dsp` namespace | Optimized exp, log, etc. | `dsp::exp2_taylor5()` for V/oct→freq conversion with negligible error. Much faster than `std::exp2()`. |

**For Oscillator Antialiasing:**
VCV Rack does **NOT** include a built-in PolyBLEP/MinBLEP library. You must implement your own or reference open-source modules:

- **PolyBLEP** (Polynomial Band-Limited Step): Computationally efficient, smooths discontinuities. Recommended for real-time performance.
- **MinBLEP** (Minimum-phase Band-Limited Step): More accurate, higher CPU cost. Use for highest quality.
- **Reference implementations**:
  - VCV Fundamental modules (official examples): https://github.com/VCVRack/Fundamental
  - Bogaudio modules (high-quality oscillators): https://github.com/bogaudio/BogaudioModules
  - SquinkyLabs (CPU-efficient VCO): https://github.com/squinkylabs/SquinkyVCV

**Confidence:** HIGH (verified via DSP manual and Plugin API Guide)

### SIMD Optimization for Polyphony

**Why SIMD is Critical:**
With 8 voices × 2 VCOs × 4 waveforms = 64 waveform generators, polyphonic processing with SIMD can achieve **3-4x performance improvement** (25% CPU usage compared to naive implementation).

**How to Use float_4:**

```cpp
// Process 4 channels at a time
int channels = inputs[PITCH_INPUT].getChannels();
outputs[OUTPUT].setChannels(channels);

for (int c = 0; c < channels; c += 4) {
    // Get 4 voltages as simd::float_4
    simd::float_4 pitch = inputs[PITCH_INPUT].getPolyVoltageSimd<simd::float_4>(c);

    // Process with SIMD math
    simd::float_4 freq = dsp::exp2_taylor5(pitch);  // Fast approximation

    // Update oscillator phase (4 channels simultaneously)
    phase += freq * args.sampleTime;

    // Generate waveform
    simd::float_4 output = simd::sin(phase * 2.f * M_PI);

    // Write 4 channels back
    outputs[OUTPUT].setVoltageSimd(output, c);
}
```

**Best Practices:**
1. **Avoid branching in SIMD code** — use `simd::ifelse()` or conditional math instead of `if`
2. **Avoid indexing** — `float_4[i]` defeats SIMD benefits; process all 4 lanes
3. **Use fast math** — `dsp::exp2_taylor5()` instead of `std::exp2()`
4. **Organize state** — use arrays of structs for per-channel state

**Confidence:** HIGH (verified via Plugin API Guide and DSP manual)

### Panel Design Tools

| Tool | Purpose | Version | Rationale |
|------|---------|---------|-----------|
| **Inkscape** | SVG panel design (RECOMMENDED) | Latest stable | Free, cross-platform, official VCV recommendation. Precise control over vector graphics. |
| **Adobe Illustrator** | SVG panel design (alternative) | Any recent | Professional option, export to SVG. More expensive but familiar to designers. |
| **helper.py** | SVG→C++ code generation | Bundled with SDK | Analyzes SVG "components" layer, generates boilerplate widget code. Saves hours of manual coding. |

**Panel Design Workflow:**

1. **Document setup in Inkscape:**
   - Open Document Properties (Shift+Ctrl+D)
   - Units: millimeters (mm)
   - Height: 128.5 mm (Eurorack standard)
   - Width: HP × 5.08 mm (30 HP = 152.4 mm)

2. **Design panel:**
   - Create visual design on main layer
   - **Convert all text to paths** (Path > Object to Path) — required for VCV's SVG renderer
   - Use simple gradients only (2-stop linear/radial)
   - Avoid complex CSS styling

3. **Add components layer:**
   - Create layer named "components" (Ctrl+Shift+L for Layers panel)
   - Add colored shapes for each widget:
     - **Red**: Parameters (knobs, switches)
     - **Green**: Input ports
     - **Blue**: Output ports
     - **Magenta**: Lights
     - **Yellow**: Custom widgets
   - Label shapes via Object Properties (Object > Object Properties)

4. **Generate C++ code:**
   ```bash
   python helper.py createmodule MyVCO res/MyVCO.svg src/MyVCO.cpp
   ```
   - Detects component positions and types
   - Generates widget creation code
   - Outputs: "Found N params, X inputs, Y outputs, Z lights..."

**SVG Renderer Limitations:**
- Text must be converted to paths (no live text)
- Complex gradients not supported (simple 2-stop gradients OK)
- Inline fill/stroke only (no external CSS)

**Confidence:** HIGH (verified via Panel Guide and tutorial)

---

## Build System Details

### Project Structure

```
MyPlugin/
├── plugin.json          # Plugin metadata (name, version, modules)
├── Makefile             # Build configuration
├── src/
│   ├── plugin.hpp       # Plugin-wide declarations
│   ├── plugin.cpp       # Plugin initialization
│   └── MyModule.cpp     # Module implementation
├── res/
│   ├── MyModule.svg     # Panel design
│   └── ComponentLibrary.svg  # Reusable components (optional)
└── LICENSE.txt          # License (required for distribution)
```

### Makefile Configuration

Minimal Makefile:
```makefile
RACK_DIR ?= /path/to/Rack-SDK

FLAGS +=
CFLAGS +=
CXXFLAGS +=
LDFLAGS +=

SOURCES += src/plugin.cpp
SOURCES += src/MyModule.cpp

DISTRIBUTABLES += res

include $(RACK_DIR)/plugin.mk
```

**Environment Variable:**
```bash
export RACK_DIR=/path/to/Rack-SDK
```

**Dependencies:**
VCV Rack SDK bundles all dependencies (GLEW, GLFW, pffft, rtaudio, rtmidi, etc.). Running `make dep` in Rack source builds them locally — **not needed when using SDK**.

**Confidence:** HIGH (verified via official documentation)

---

## What to Avoid

### Language & Standards

| What | Why Avoid | Impact |
|------|-----------|--------|
| **C++14, C++17, C++20** for distribution | VCV's build system requires C++11 for Linux compatibility (libstdc++ 5.4.0) | Cannot distribute via VCV Library if using newer standards. OK for local use only. |
| **Newer GCC/Clang features** beyond C++11 | Same as above | Build failures on official build server. |

**Source:** Community forum thread confirming C++11 requirement from VCV lead developer Andrew Belt (Vortico).

### DSP Pitfalls

| What | Why Avoid | Alternative |
|------|-----------|-------------|
| **std::exp(), std::sin() in audio rate** | Too slow, defeats SIMD | Use `dsp::exp2_taylor5()`, `simd::sin()` for negligible error and 5-10x speedup. |
| **Branching in SIMD loops** | Defeats vectorization | Use `simd::ifelse()` or conditional math: `result = condition * valueA + (!condition) * valueB` |
| **Naive polyphonic processing** | 4x CPU usage compared to SIMD | Use `float_4` and process 4 channels per iteration. |
| **Non-bandlimited waveforms** | Harsh aliasing artifacts | Implement PolyBLEP or MinBLEP. Reference Fundamental/Bogaudio modules. |
| **Propagating NaN/infinity** | Crashes or cable protection kicks in | Check for non-finite values: `std::isfinite()` |

### Architecture Mistakes

| What | Why Avoid | Alternative |
|------|-----------|-------------|
| **NULL module assumption in ModuleWidget** | Crashes Module Browser | Always handle `module == nullptr` in widget code (for module preview rendering). |
| **Ignoring polyphonic cable summing** | User confusion | For monophonic modules, sum polyphonic input channels when designing audio inputs. |
| **External dependencies without bundling** | Distribution failures | Bundle all non-standard libraries in dep/ folder. Use only SDK-provided libraries. |
| **Spaces in file paths** | Makefile failures | Use underscores or hyphens in paths. |

### Deprecated Features

| Feature | Status | Replacement |
|---------|--------|-------------|
| **VCV Bridge** | Removed in Rack 1.0 | Use VCV Rack as VST2/VST3 plugin instead. |
| **Engine pausing API** | Removed | N/A (architectural change) |
| **Module disabling** | Replaced | Use module bypassing (routes inputs→outputs). |
| **Module "deprecated" property** | Deprecated | Use "hidden" property in plugin.json. |

**Confidence:** MEDIUM (sources: community forums, changelog, FAQ)

---

## Dependencies Bundled in SDK

VCV Rack SDK includes pre-compiled versions of:

| Library | Version (2.6) | Purpose |
|---------|---------------|---------|
| **GLEW** | — | OpenGL extension wrangler |
| **GLFW** | — | OpenGL window/context |
| **nanovg** | — | Vector graphics rendering |
| **nanosvg** | — | SVG parsing/rendering |
| **pffft** | — | Fast FFT implementation |
| **RtAudio** | — | Cross-platform audio I/O |
| **RtMidi** | — | Cross-platform MIDI I/O |
| **libcurl** | 8.10.0 | HTTP client (updated in 2.6) |
| **OpenSSL** | 3.3.2 | Crypto/TLS (updated in 2.6) |
| **libspeexdsp** | — | Fixed-ratio resampling |
| **libsamplerate** | — | Variable-ratio resampling |

**You do NOT need to install these separately.** They are built into the SDK.

**Confidence:** HIGH (verified via Building manual and GitHub)

---

## Development Workflow

### Setup (First Time)

1. **Download VCV Rack SDK** for your platform:
   - Windows x64
   - Mac x64 / Mac ARM64 (separate SDKs as of 2.6)
   - Linux x64
   - Source: https://vcvrack.com/downloads/

2. **Install toolchain:**
   - Windows: MSYS2 + MinGW64 packages
   - Mac: Homebrew + Xcode CLI tools
   - Linux: GCC/build-essential

3. **Set environment variable:**
   ```bash
   export RACK_DIR=/path/to/Rack-SDK
   # Add to .bashrc / .zshrc for persistence
   ```

4. **Create plugin template:**
   ```bash
   cd /path/to/plugin-dev
   python $RACK_DIR/helper.py createplugin MyPlugin
   cd MyPlugin
   ```

5. **Test build:**
   ```bash
   make
   # Should compile successfully with "empty" plugin
   ```

### Development Loop

1. **Design panel in Inkscape** (save to `res/ModuleName.svg`)
2. **Add components layer** with colored shapes
3. **Generate module code:**
   ```bash
   python $RACK_DIR/helper.py createmodule ModuleName res/ModuleName.svg src/ModuleName.cpp
   ```
4. **Implement DSP logic** in `ModuleName::process()`
5. **Build and install:**
   ```bash
   make install -j4
   ```
6. **Launch VCV Rack** and test module
7. **Iterate** (hot-reload panel SVGs by right-clicking module in Rack)

### Debugging

| Tool | Setup | Use |
|------|-------|-----|
| **VSCode** | Configure `tasks.json` to run `make install`, `launch.json` to attach debugger | Set breakpoints, step through code. Guides available for Windows/Mac/Linux. |
| **GDB** | Build Rack from source, run `make debug` | Command-line debugging. More control, steeper learning curve. |
| **Logging** | `DEBUG()`, `INFO()`, `WARN()` macros in Rack API | Console output in Rack's log. |
| **CPU profiling** | `perf` (Linux), Instruments (Mac), gprof/gperftools | Identify performance bottlenecks. Profile before optimizing. |

**Confidence:** MEDIUM-HIGH (verified via community forums and VSCode setup guides)

---

## Polyphonic VCO-Specific Recommendations

For your 8-voice dual-VCO module (16 oscillators total):

### Architecture Strategy

1. **Use SIMD throughout:**
   - Process 4 voices at a time with `float_4`
   - Two iterations for 8 voices: `c=0` (voices 0-3), `c=4` (voices 4-7)

2. **State organization:**
   ```cpp
   struct VoiceState {
       float phase1, phase2;  // VCO1 and VCO2 phases
       // ... other per-voice state
   };
   VoiceState voices[8];  // NOT VoiceState voices[8][2]
   ```
   - Keep state for 4 voices contiguous for better cache performance

3. **Antialiasing approach:**
   - **PolyBLEP** recommended for real-time performance with 16 oscillators
   - Reference Bogaudio or Fundamental implementations
   - Avoid per-sample FFT/convolution (too expensive for 16 oscillators)

4. **Through-zero FM:**
   - Implement phase modulation: `phase2 += (freq2 + fm_amount * vco1_output) * sampleTime`
   - Use fast approximations: `dsp::exp2_taylor5()` for V/oct→freq
   - Keep FM in tune: ensure modulator/carrier frequencies tracked accurately

5. **Voicing modes (Poly/Dual/Unison):**
   - Process all 8 voices every sample, mix outputs differently:
     - **Poly**: Each voice → separate poly cable channel
     - **Dual**: Voices 0+1 → channel 0, 2+3 → channel 1, etc.
     - **Unison**: All voices → mono output (sum)

6. **Panel layout:**
   - 30 HP = 152.4 mm width
   - Use Inkscape grid (5.08mm spacing) for component placement
   - Adequate spacing for finger manipulation (10mm+ between knobs)

### Performance Targets

| Scenario | Target CPU % | Notes |
|----------|--------------|-------|
| 8 voices, no FM/sync | <5% | Baseline polyphonic oscillators |
| 8 voices, FM + sync | <10% | With SIMD optimization |
| 8 voices, all features | <15% | Full cross-modulation, XOR, etc. |

**Benchmark against:** VCV Fundamental VCO-1 (~2-3% per voice without SIMD, ~0.7% with SIMD).

**Confidence:** MEDIUM-HIGH (based on documented SIMD performance gains and oscillator complexity)

---

## Installation Instructions

### Windows (MSYS2 MinGW64)

```bash
# Install MSYS2 from https://www.msys2.org/
# Launch "MSYS2 MinGW 64-bit" shell

# Install toolchain
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gdb mingw-w64-x86_64-cmake
pacman -S git make tar autoconf automake libtool wget unzip zip

# Download Rack SDK (Windows x64) from https://vcvrack.com/downloads/
# Extract to /c/Rack-SDK

# Set environment variable
echo 'export RACK_DIR=/c/Rack-SDK' >> ~/.bashrc
source ~/.bashrc

# Create plugin
cd /c/Users/YourName/Documents
python $RACK_DIR/helper.py createplugin MyPlugin
cd MyPlugin
make
```

### macOS (Homebrew)

```bash
# Install Homebrew from https://brew.sh/

# Install toolchain
brew install git wget cmake autoconf automake libtool jq python zstd pkg-config

# Download Rack SDK (Mac x64 or Mac ARM64) from https://vcvrack.com/downloads/
# Extract to ~/Rack-SDK

# Set environment variable
echo 'export RACK_DIR=~/Rack-SDK' >> ~/.zshrc
source ~/.zshrc

# Create plugin
cd ~/Documents
python $RACK_DIR/helper.py createplugin MyPlugin
cd MyPlugin
make
```

### Linux (Ubuntu/Debian)

```bash
# Install toolchain
sudo apt install build-essential git curl cmake libgl1-mesa-dev libglu1-mesa-dev \
  libx11-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev

# Download Rack SDK (Linux x64) from https://vcvrack.com/downloads/
# Extract to ~/Rack-SDK

# Set environment variable
echo 'export RACK_DIR=~/Rack-SDK' >> ~/.bashrc
source ~/.bashrc

# Create plugin
cd ~/projects
python3 $RACK_DIR/helper.py createplugin MyPlugin
cd MyPlugin
make
```

---

## Confidence Assessment

| Area | Confidence | Rationale |
|------|------------|-----------|
| **C++11 requirement** | HIGH | Directly confirmed by VCV lead developer in community forums; verified in official documentation. |
| **SDK version (2.6+)** | HIGH | Official changelog and downloads page confirm 2.6.6 as current stable (November 2024). |
| **Build system (Makefile)** | HIGH | Official Building documentation describes Makefile workflow in detail. |
| **SIMD with float_4** | HIGH | Plugin API Guide provides code examples and performance claims verified by community. |
| **Antialiasing libraries** | MEDIUM | DSP manual mentions PolyBLEP/MinBLEP as recommended approaches, but no built-in library confirmed. Must reference open-source implementations. |
| **Panel design (Inkscape)** | HIGH | Panel Guide officially recommends Inkscape with detailed setup instructions. |
| **Performance targets** | MEDIUM | Based on documented SIMD speedups (3-4x) and VCV Fundamental benchmarks, but actual performance depends on implementation. |
| **Toolchain versions** | MEDIUM | Official docs specify tools (MinGW64, GCC) but not exact version numbers. Community confirms GCC 7+ works. |

---

## Sources & References

**Official VCV Rack Documentation:**
- [Plugin Development Tutorial](https://vcvrack.com/manual/PluginDevelopmentTutorial)
- [Building Guide](https://vcvrack.com/manual/Building)
- [Plugin API Guide](https://vcvrack.com/manual/PluginGuide)
- [DSP Manual](https://vcvrack.com/manual/DSP)
- [Polyphony Guide](https://vcvrack.com/manual/Polyphony)
- [Panel Design Guide](https://vcvrack.com/manual/Panel)
- [VCV Rack 2.6 Release Notes](https://vcvrack.com/news/2025-03-27-Rack-2.6)
- [Versioning](https://vcvrack.com/manual/Version)

**GitHub Repositories:**
- [VCV Rack Source](https://github.com/VCVRack/Rack)
- [VCV Fundamental (Official Modules)](https://github.com/VCVRack/Fundamental)
- [Bogaudio Modules](https://github.com/bogaudio/BogaudioModules)
- [SquinkyLabs Modules](https://github.com/squinkylabs/SquinkyVCV)

**Community Resources:**
- [C++ Standard Requirements Discussion](https://community.vcvrack.com/t/newest-c-standard-allowed-for-inclusion-in-plugin-library/5970)
- [Polyphony Reminders for Developers](https://community.vcvrack.com/t/polyphony-reminders-for-plugin-developers/9572)
- [VCV Community Development Forum](https://community.vcvrack.com/c/development/8)

**Development Tools:**
- [VSCode Debugging Setup (Windows)](https://medium.com/@tonetechnician/how-to-setup-your-windows-vs-code-environment-for-vcv-rack-plugin-development-and-debugging-6e76c5a5f115)
- [Dev Workflow Tips Thread](https://community.vcvrack.com/t/looking-for-dev-workflow-tips-testing-debugging-profiling-editor/22852)

---

## Next Steps for Roadmap

Based on this stack research, the roadmap should:

1. **Phase 1: Development Environment Setup**
   - Install SDK, toolchain, Inkscape
   - Create plugin scaffold with helper.py
   - Verify build system works

2. **Phase 2: Basic Oscillator Core**
   - Implement single-voice VCO with one waveform (sine)
   - Add PolyBLEP antialiasing (reference Fundamental)
   - Verify V/oct tracking accuracy

3. **Phase 3: SIMD Polyphonic Processing**
   - Convert to float_4 processing for 4 voices
   - Extend to 8 voices (2 iterations)
   - Benchmark CPU usage, optimize

4. **Phase 4+:** Add features incrementally (FM, sync, waveform mixing, etc.)

**Research flags:**
- **Phase 2-3 likely need deeper research** on PolyBLEP implementation (not built-in, must reference external code)
- **Phase 4+** should be straightforward once SIMD foundation is solid

---

**END OF STACK RESEARCH**
