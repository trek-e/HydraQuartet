/*
 * Copyright 2026 HydraQuartet
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "plugin.hpp"
#include <cmath>
#include <cstring>

using simd::float_4;

// Constants for MinBLEP generation
static constexpr int MINBLEP_Z = 16;  // Zero crossings
static constexpr int MINBLEP_O = 16;  // Oversample factor

// Static MinBLEP impulse table (shared lookup, generated once)
struct MinBlepTable {
	float impulse[2 * MINBLEP_Z * MINBLEP_O + 1];

	MinBlepTable() {
		dsp::minBlepImpulse(MINBLEP_Z, MINBLEP_O, impulse);
		impulse[2 * MINBLEP_Z * MINBLEP_O] = 1.f;
	}
};
static MinBlepTable minBlepTable;

// SIMD-compatible MinBLEP buffer with stride support
// Stores 4 interleaved lanes for efficient SIMD processing
template <int N>
struct MinBlepBuffer {
	float_4 buffer[2 * N] = {};
	int pos = 0;

	// Insert discontinuity with stride=4 for a single lane
	// p: subsample position (-1 < p <= 0)
	// x: discontinuity magnitude
	// lane: which SIMD lane (0-3)
	void insertDiscontinuity(float p, float x, int lane) {
		if (!(-1.f < p && p <= 0.f))
			return;
		for (int j = 0; j < 2 * MINBLEP_Z; j++) {
			float minBlepIndex = ((float)j - p) * MINBLEP_O;
			int index = (pos + j) % (2 * N);
			// Access specific lane using array indexing
			buffer[index][lane] += x * (-1.f + math::interpolateLinear(minBlepTable.impulse, minBlepIndex));
		}
	}

	float_4 process() {
		float_4 v = buffer[pos];
		buffer[pos] = float_4(0.f);
		pos = (pos + 1) % (2 * N);
		return v;
	}
};

// VcoEngine: Reusable oscillator DSP with SIMD state
// Encapsulates all per-oscillator state for dual VCO architecture
struct VcoEngine {
	float_4 phase[4] = {};
	float_4 oldPhase[4] = {};
	float_4 deltaPhase[4] = {};
	MinBlepBuffer<32> sawMinBlepBuffer[4];
	MinBlepBuffer<32> sqrMinBlepBuffer[4];
	MinBlepBuffer<32> triMinBlepBuffer[4];
	MinBlepBuffer<32> xorMinBlepBuffer[4];  // XOR discontinuity tracking

	// Process one SIMD group (4 voices), returns 4 waveforms via output parameters
	// g: SIMD group index (0-3)
	// freq: frequency for 4 voices
	// sampleTime: 1/sampleRate
	// pwm: pulse width for 4 voices
	// wrapMask: output parameter indicating which lanes wrapped
	// sqr1Input: square wave from VCO1 (for XOR calculation in VCO2)
	// xorOut: optional XOR output pointer
	void process(int g, float_4 freq, float sampleTime, float_4 pwm,
	             float_4& saw, float_4& sqr, float_4& tri, float_4& sine,
	             int& wrapMask,
	             float_4 sqr1Input = float_4(0.f),  // Square from VCO1 (for XOR)
	             float_4* xorOut = nullptr) {        // Optional XOR output
		// Phase accumulation with SIMD
		deltaPhase[g] = simd::clamp(freq * sampleTime, 0.f, 0.49f);
		oldPhase[g] = phase[g];
		phase[g] += deltaPhase[g];

		// Detect phase wrap
		float_4 wrapped = phase[g] >= 1.f;
		phase[g] -= simd::floor(phase[g]);  // Handles large FM jumps

		// === SAWTOOTH with strided MinBLEP ===
		wrapMask = simd::movemask(wrapped);
		if (wrapMask) {
			for (int i = 0; i < 4; i++) {
				if ((wrapMask & (1 << i)) && deltaPhase[g][i] > 0.f) {
					float subsample = (1.f - oldPhase[g][i]) / deltaPhase[g][i] - 1.f;
					sawMinBlepBuffer[g].insertDiscontinuity(subsample, -2.f, i);
				}
			}
		}
		saw = 2.f * phase[g] - 1.f + sawMinBlepBuffer[g].process();

		// === SQUARE with PWM using strided MinBLEP ===
		// Falling edge detection (phase crosses PWM threshold)
		float_4 fallingEdge = (oldPhase[g] < pwm) & (phase[g] >= pwm);
		int fallMask = simd::movemask(fallingEdge);
		if (fallMask) {
			for (int i = 0; i < 4; i++) {
				if ((fallMask & (1 << i)) && deltaPhase[g][i] > 0.f) {
					float subsample = (pwm[i] - oldPhase[g][i]) / deltaPhase[g][i] - 1.f;
					sqrMinBlepBuffer[g].insertDiscontinuity(subsample, -2.f, i);
				}
			}
		}

		// Rising edge on wrap
		if (wrapMask) {
			for (int i = 0; i < 4; i++) {
				if ((wrapMask & (1 << i)) && deltaPhase[g][i] > 0.f) {
					float subsample = (1.f - oldPhase[g][i]) / deltaPhase[g][i] - 1.f;
					sqrMinBlepBuffer[g].insertDiscontinuity(subsample, 2.f, i);
				}
			}
		}

		sqr = simd::ifelse(phase[g] < pwm, 1.f, -1.f) + sqrMinBlepBuffer[g].process();

		// === XOR ring modulation (only if requested) ===
		if (xorOut != nullptr) {
			// Raw ring modulation: sqr1 * sqr2
			*xorOut = sqr1Input * sqr;

			// Track XOR edges from THIS oscillator's square transitions
			// (sqr1Input edges are tracked separately in VCO1's call)

			// Falling edge detection (PWM threshold crossing)
			// When sqr transitions from +1 to -1, XOR changes by -2 * sqr1Input
			if (fallMask) {
				for (int i = 0; i < 4; i++) {
					if ((fallMask & (1 << i)) && deltaPhase[g][i] > 0.f) {
						float subsample = (pwm[i] - oldPhase[g][i]) / deltaPhase[g][i] - 1.f;
						float xorDisc = -2.f * sqr1Input[i];  // sqr: +1 -> -1
						xorMinBlepBuffer[g].insertDiscontinuity(subsample, xorDisc, i);
					}
				}
			}

			// Rising edge on wrap (when phase wraps, sqr goes from -1 to +1)
			if (wrapMask) {
				for (int i = 0; i < 4; i++) {
					if ((wrapMask & (1 << i)) && deltaPhase[g][i] > 0.f) {
						float subsample = (1.f - oldPhase[g][i]) / deltaPhase[g][i] - 1.f;
						float xorDisc = 2.f * sqr1Input[i];  // sqr: -1 -> +1
						xorMinBlepBuffer[g].insertDiscontinuity(subsample, xorDisc, i);
					}
				}
			}

			// Apply MinBLEP correction
			*xorOut += xorMinBlepBuffer[g].process();
		}

		// === TRIANGLE via direct calculation (normalized to ±1) ===
		// Triangle from phase: rises 0->0.5, falls 0.5->1
		tri = simd::ifelse(phase[g] < 0.5f,
			4.f * phase[g] - 1.f,           // -1 to +1 as phase goes 0 to 0.5
			3.f - 4.f * phase[g]);          // +1 to -1 as phase goes 0.5 to 1
		tri = tri + triMinBlepBuffer[g].process();

		// === SINE (no antialiasing needed) ===
		sine = simd::sin(2.f * float(M_PI) * phase[g]);
	}

	// Apply hard sync: reset phase and insert MinBLEP discontinuities
	// Called after process() when primary oscillator wraps
	void applySync(int g, int syncMask, float_4 primaryOldPhase, float_4 primaryDeltaPhase, float_4 pwm,
	               float_4& saw, float_4& sqr, float_4& tri) {
		for (int i = 0; i < 4; i++) {
			if (!(syncMask & (1 << i))) continue;
			if (deltaPhase[g][i] <= 0.f) continue;  // Skip if negative freq (FM)
			if (primaryDeltaPhase[i] <= 0.f) continue;  // Skip if primary freq negative

			// Calculate subsample position of primary wrap
			float subsample = (1.f - primaryOldPhase[i]) / primaryDeltaPhase[i] - 1.f;
			subsample = clamp(subsample, -1.f + 1e-6f, 0.f);  // Ensure valid range

			// Calculate old waveform values (at current phase, before reset)
			float currentPhase = phase[g][i];
			float oldSaw = 2.f * currentPhase - 1.f;
			float oldSqr = (currentPhase < pwm[i]) ? 1.f : -1.f;
			float oldTri = (currentPhase < 0.5f)
				? (4.f * currentPhase - 1.f)
				: (3.f - 4.f * currentPhase);

			// Reset phase to subsample-accurate position
			float newPhase = deltaPhase[g][i] * (-subsample);
			phase[g][i] = newPhase;

			// Calculate new waveform values (at reset phase)
			float newSaw = 2.f * newPhase - 1.f;
			float newSqr = (newPhase < pwm[i]) ? 1.f : -1.f;
			float newTri = (newPhase < 0.5f)
				? (4.f * newPhase - 1.f)
				: (3.f - 4.f * newPhase);

			// Insert MinBLEP discontinuities for all geometric waveforms
			sawMinBlepBuffer[g].insertDiscontinuity(subsample, newSaw - oldSaw, i);

			// Square: only insert if value actually changed
			if (oldSqr != newSqr) {
				sqrMinBlepBuffer[g].insertDiscontinuity(subsample, newSqr - oldSqr, i);
			}

			// Triangle: uses dedicated triMinBlepBuffer (added in Task 1)
			// Insert amplitude discontinuity for sync-induced phase reset
			triMinBlepBuffer[g].insertDiscontinuity(subsample, newTri - oldTri, i);

			// Update waveform output values for this lane to reflect synced phase
			saw[i] = newSaw;
			sqr[i] = newSqr;
			tri[i] = newTri;
		}
	}
};

// Maximum polyphony: 16 voices (4 SIMD groups of 4 voices each)
// Arrays are sized for this limit; process() enforces bounds checking
struct HydraQuartetVCO : Module {
	enum ParamId {
		// VCO1 Section
		OCTAVE1_PARAM,
		DETUNE1_PARAM,
		TRI1_PARAM,
		SQR1_PARAM,
		SIN1_PARAM,
		SAW1_PARAM,
		PWM1_PARAM,
		PWM1_ATT_PARAM,
		SYNC1_PARAM,
		SUB_WAVE_PARAM,
		SUB_LEVEL_PARAM,
		// VCO2 Section
		OCTAVE2_PARAM,
		FINE2_PARAM,
		TRI2_PARAM,
		SQR2_PARAM,
		SIN2_PARAM,
		SAW2_PARAM,
		XOR_PARAM,  // XOR volume control
		PWM2_PARAM,
		PWM2_ATT_PARAM,
		SYNC2_PARAM,
		FM_PARAM,
		FM_ATT_PARAM,  // FM CV attenuverter
		PARAMS_LEN
	};
	enum InputId {
		// Global
		VOCT_INPUT,
		GATE_INPUT,
		// VCO1
		PWM1_INPUT,
		// VCO2
		PWM2_INPUT,
		FM_INPUT,
		// Waveform volume CV inputs
		SAW1_CV_INPUT,
		SQR1_CV_INPUT,
		SUB_CV_INPUT,
		XOR_CV_INPUT,
		SQR2_CV_INPUT,
		SAW2_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AUDIO_OUTPUT,
		MIX_OUTPUT,
		SUB_OUTPUT,
		// Per-voice outputs (VCO1+VCO2 mixed for each voice)
		VOICE1_OUTPUT,
		VOICE2_OUTPUT,
		VOICE3_OUTPUT,
		VOICE4_OUTPUT,
		VOICE5_OUTPUT,
		VOICE6_OUTPUT,
		VOICE7_OUTPUT,
		VOICE8_OUTPUT,
		// Per-voice gate pass-through
		GATE1_OUTPUT,
		GATE2_OUTPUT,
		GATE3_OUTPUT,
		GATE4_OUTPUT,
		GATE5_OUTPUT,
		GATE6_OUTPUT,
		GATE7_OUTPUT,
		GATE8_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		PWM1_CV_LIGHT,
		PWM2_CV_LIGHT,
		FM_CV_LIGHT,  // FM CV activity indicator
		LIGHTS_LEN
	};

	// Dual VCO engines (each encapsulates phase, MinBLEP buffers, tri state)
	VcoEngine vco1;
	VcoEngine vco2;

	// XOR MinBLEP tracking for VCO1 square edges (module-level, not in VcoEngine)
	MinBlepBuffer<32> xorFromVco1MinBlep[4];  // Track VCO1 sqr transitions for XOR

	// Sub-oscillator state (tracks VCO1 at -1 octave)
	float_4 subPhase[4] = {};

	// DC filters kept scalar (not in hot path, operate on mixed output)
	dsp::TRCFilter<float> dcFilters[16];

	HydraQuartetVCO() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// VCO1 Parameters
		configSwitch(OCTAVE1_PARAM, -2.f, 2.f, 0.f, "VCO1 Octave", {"-2", "-1", "0", "+1", "+2"});
		configParam(DETUNE1_PARAM, 0.f, 1.f, 0.f, "VCO1 Detune");
		configParam(TRI1_PARAM, 0.f, 10.f, 0.f, "VCO1 Triangle");
		configParam(SQR1_PARAM, 0.f, 10.f, 1.f, "VCO1 Square");
		configParam(SIN1_PARAM, 0.f, 10.f, 1.f, "VCO1 Sine");
		configParam(SAW1_PARAM, 0.f, 10.f, 0.f, "VCO1 Sawtooth");
		configParam(PWM1_PARAM, 0.f, 1.f, 0.5f, "VCO1 Pulse Width", "%", 0.f, 100.f);
		configParam(PWM1_ATT_PARAM, -1.f, 1.f, 0.f, "VCO1 PWM CV Attenuverter", "%", 0.f, 100.f);
		configSwitch(SYNC1_PARAM, 0.f, 1.f, 0.f, "VCO1 Sync", {"Off", "Hard"});
		configSwitch(SUB_WAVE_PARAM, 0.f, 1.f, 0.f, "Sub Waveform", {"Square", "Sine"});
		configParam(SUB_LEVEL_PARAM, 0.f, 10.f, 0.f, "Sub Level");

		// VCO2 Parameters
		configSwitch(OCTAVE2_PARAM, -2.f, 2.f, 0.f, "VCO2 Octave", {"-2", "-1", "0", "+1", "+2"});
		configParam(FINE2_PARAM, -1.f, 1.f, 0.f, "VCO2 Fine Tune", " cents", 0.f, 100.f);
		configParam(TRI2_PARAM, 0.f, 10.f, 0.f, "VCO2 Triangle");
		configParam(SQR2_PARAM, 0.f, 10.f, 1.f, "VCO2 Square");
		configParam(SIN2_PARAM, 0.f, 10.f, 0.f, "VCO2 Sine");
		configParam(SAW2_PARAM, 0.f, 10.f, 0.f, "VCO2 Sawtooth");
		configParam(XOR_PARAM, 0.f, 10.f, 0.f, "XOR Volume");
		configParam(PWM2_PARAM, 0.f, 1.f, 0.5f, "VCO2 Pulse Width", "%", 0.f, 100.f);
		configParam(PWM2_ATT_PARAM, -1.f, 1.f, 0.f, "VCO2 PWM CV Attenuverter", "%", 0.f, 100.f);
		configSwitch(SYNC2_PARAM, 0.f, 1.f, 0.f, "VCO2 Sync", {"Off", "Hard"});
		configParam(FM_PARAM, 0.f, 1.f, 0.f, "FM Amount");
		configParam(FM_ATT_PARAM, -1.f, 1.f, 0.f, "FM CV Attenuverter", "%", 0.f, 100.f);

		// Inputs
		configInput(VOCT_INPUT, "V/Oct");
		configInput(GATE_INPUT, "Gate");
		configInput(PWM1_INPUT, "VCO1 PWM CV");
		configInput(PWM2_INPUT, "VCO2 PWM CV");
		configInput(FM_INPUT, "FM CV");

		// Waveform volume CV inputs
		configInput(SAW1_CV_INPUT, "SAW1 Volume CV");
		configInput(SQR1_CV_INPUT, "SQR1 Volume CV");
		configInput(SUB_CV_INPUT, "Sub Volume CV");
		configInput(XOR_CV_INPUT, "XOR Volume CV");
		configInput(SQR2_CV_INPUT, "SQR2 Volume CV");
		configInput(SAW2_CV_INPUT, "SAW2 Volume CV");

		// Outputs
		configOutput(AUDIO_OUTPUT, "Polyphonic Audio");
		configOutput(MIX_OUTPUT, "Mix");
		configOutput(SUB_OUTPUT, "Sub-Oscillator");

		// Per-voice outputs
		configOutput(VOICE1_OUTPUT, "Voice 1");
		configOutput(VOICE2_OUTPUT, "Voice 2");
		configOutput(VOICE3_OUTPUT, "Voice 3");
		configOutput(VOICE4_OUTPUT, "Voice 4");
		configOutput(VOICE5_OUTPUT, "Voice 5");
		configOutput(VOICE6_OUTPUT, "Voice 6");
		configOutput(VOICE7_OUTPUT, "Voice 7");
		configOutput(VOICE8_OUTPUT, "Voice 8");

		// Per-voice gate outputs
		configOutput(GATE1_OUTPUT, "Gate 1");
		configOutput(GATE2_OUTPUT, "Gate 2");
		configOutput(GATE3_OUTPUT, "Gate 3");
		configOutput(GATE4_OUTPUT, "Gate 4");
		configOutput(GATE5_OUTPUT, "Gate 5");
		configOutput(GATE6_OUTPUT, "Gate 6");
		configOutput(GATE7_OUTPUT, "Gate 7");
		configOutput(GATE8_OUTPUT, "Gate 8");
	}

	void process(const ProcessArgs& args) override {
		// Get channel count from V/Oct input (bounded to valid range 1-16)
		int channels = clamp(inputs[VOCT_INPUT].getChannels(), 1, 16);

		float sampleTime = args.sampleTime;
		float sampleRate = args.sampleRate;

		// Read pitch control parameters (outside loop - same for all voices)
		float octave1 = std::round(params[OCTAVE1_PARAM].getValue());  // -2 to +2
		float octave2 = std::round(params[OCTAVE2_PARAM].getValue());  // -2 to +2
		float detuneKnob = params[DETUNE1_PARAM].getValue();           // 0 to 1
		float detuneVolts = detuneKnob * (50.f / 1200.f);              // 0-50 cents in V/Oct

		// Read VCO1 parameters
		float pwm1 = params[PWM1_PARAM].getValue();
		float triVol1 = params[TRI1_PARAM].getValue();
		float sinVol1 = params[SIN1_PARAM].getValue();

		// Read VCO2 parameters
		float pwm2 = params[PWM2_PARAM].getValue();
		float triVol2 = params[TRI2_PARAM].getValue();
		float sinVol2 = params[SIN2_PARAM].getValue();

		// Read PWM CV attenuverters
		float pwm1Att = params[PWM1_ATT_PARAM].getValue();
		float pwm2Att = params[PWM2_ATT_PARAM].getValue();

		// Read FM parameters
		float fmKnob = params[FM_PARAM].getValue();  // 0 to 1
		float fmAtt = params[FM_ATT_PARAM].getValue();  // -1 to 1

		// Read sub-oscillator parameters
		float subWave = params[SUB_WAVE_PARAM].getValue();  // 0 = square, 1 = sine

		// Fixed output scaling - divide by 3 (typical number of active waveforms)
		// User controls final level via individual waveform volumes
		const float outputScale = 1.f / 3.f;

		// Read sync switch states
		bool sync1Enabled = params[SYNC1_PARAM].getValue() > 0.5f;  // VCO1 syncs to VCO2
		bool sync2Enabled = params[SYNC2_PARAM].getValue() > 0.5f;  // VCO2 syncs to VCO1

		// Read waveform volume knobs (for CV-replaces-knob pattern)
		float saw1Knob = params[SAW1_PARAM].getValue();
		float sqr1Knob = params[SQR1_PARAM].getValue();
		float subKnob = params[SUB_LEVEL_PARAM].getValue();
		float xorKnob = params[XOR_PARAM].getValue();
		float sqr2Knob = params[SQR2_PARAM].getValue();
		float saw2Knob = params[SAW2_PARAM].getValue();

		// Check CV connections (outside loop for efficiency)
		bool saw1CVConnected = inputs[SAW1_CV_INPUT].isConnected();
		bool sqr1CVConnected = inputs[SQR1_CV_INPUT].isConnected();
		bool subCVConnected = inputs[SUB_CV_INPUT].isConnected();
		bool xorCVConnected = inputs[XOR_CV_INPUT].isConnected();
		bool sqr2CVConnected = inputs[SQR2_CV_INPUT].isConnected();
		bool saw2CVConnected = inputs[SAW2_CV_INPUT].isConnected();

		// Process in SIMD groups of 4 voices
		for (int c = 0; c < channels; c += 4) {
			int groupChannels = std::min(channels - c, 4);
			int g = c / 4;  // SIMD group index

			// Load 4 channels of V/Oct using SIMD
			float_4 basePitch = inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c);

			// VCO1: base + octave + detune (VCO1 gets detune for thickness)
			float_4 pitch1 = basePitch + octave1 + detuneVolts;
			float_4 freq1 = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch1);
			freq1 = simd::clamp(freq1, 0.1f, sampleRate / 2.f);

			// VCO2: base + octave (reference oscillator, no detune)
			float_4 pitch2 = basePitch + octave2;
			float_4 freq2 = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch2);

			// Through-zero linear FM: VCO1 modulates VCO2 frequency
			// Read FM CV (auto-detect poly/mono)
			float_4 fmCV;
			int fmChannels = inputs[FM_INPUT].getChannels();
			if (fmChannels > 1) {
				// Polyphonic: per-voice modulation
				fmCV = inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c);
			} else {
				// Monophonic: broadcast to all voices
				fmCV = float_4(inputs[FM_INPUT].getVoltage());
			}

			// Calculate per-voice FM depth: knob + (CV * attenuverter * scale)
			float_4 fmDepth = fmKnob + fmCV * fmAtt * 0.1f;
			fmDepth = simd::clamp(fmDepth, 0.f, 2.f);

			// Apply linear FM: freq2 += freq1 * fmDepth
			freq2 += freq1 * fmDepth;

			// Clamp to prevent extreme values
			freq2 = simd::clamp(freq2, 0.1f, sampleRate / 2.f);

			// Read polyphonic PWM CV
			float_4 pwm1CV = inputs[PWM1_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 pwm2CV = inputs[PWM2_INPUT].getPolyVoltageSimd<float_4>(c);

			// Apply attenuverter: +/-5V * att * 0.1 = +/-0.5 contribution (full sweep range)
			float_4 pwm1_4 = pwm1 + pwm1CV * pwm1Att * 0.1f;
			float_4 pwm2_4 = pwm2 + pwm2CV * pwm2Att * 0.1f;

			// Clamp to safe PWM range (avoid DC at extremes)
			pwm1_4 = simd::clamp(pwm1_4, 0.01f, 0.99f);
			pwm2_4 = simd::clamp(pwm2_4, 0.01f, 0.99f);

			// Read waveform volume CVs (polyphonic)
			float_4 saw1CV = inputs[SAW1_CV_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 sqr1CV = inputs[SQR1_CV_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 subCV = inputs[SUB_CV_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 xorCV = inputs[XOR_CV_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 sqr2CV = inputs[SQR2_CV_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 saw2CV = inputs[SAW2_CV_INPUT].getPolyVoltageSimd<float_4>(c);

			// CV replaces knob when patched, 0-10V maps to 0-10 volume
			float_4 saw1Vol_4 = saw1CVConnected ? simd::clamp(saw1CV, 0.f, 10.f) : float_4(saw1Knob);
			float_4 sqr1Vol_4 = sqr1CVConnected ? simd::clamp(sqr1CV, 0.f, 10.f) : float_4(sqr1Knob);
			float_4 subVol_4 = subCVConnected ? simd::clamp(subCV, 0.f, 10.f) : float_4(subKnob);
			float_4 xorVol_4 = xorCVConnected ? simd::clamp(xorCV, 0.f, 10.f) : float_4(xorKnob);
			float_4 sqr2Vol_4 = sqr2CVConnected ? simd::clamp(sqr2CV, 0.f, 10.f) : float_4(sqr2Knob);
			float_4 saw2Vol_4 = saw2CVConnected ? simd::clamp(saw2CV, 0.f, 10.f) : float_4(saw2Knob);

			// Phase 1: Process both VCO engines to get wrap masks (stores oldPhase, deltaPhase internally)
			float_4 saw1, sqr1, tri1, sine1;
			float_4 saw2, sqr2, tri2, sine2, xorOut;
			int vco1WrapMask, vco2WrapMask;
			vco1.process(g, freq1, sampleTime, pwm1_4, saw1, sqr1, tri1, sine1, vco1WrapMask);
			vco2.process(g, freq2, sampleTime, pwm2_4, saw2, sqr2, tri2, sine2, vco2WrapMask, sqr1, &xorOut);

			// Track VCO1 square edges for XOR MinBLEP (XOR = sqr1 * sqr2)
			// When sqr1 transitions, XOR changes by 2 * sqr2

			// VCO1 rising edge (wrap)
			if (vco1WrapMask) {
				for (int i = 0; i < 4; i++) {
					if ((vco1WrapMask & (1 << i)) && vco1.deltaPhase[g][i] > 0.f) {
						float subsample = (1.f - vco1.oldPhase[g][i]) / vco1.deltaPhase[g][i] - 1.f;
						// sqr1: -1 -> +1, so XOR changes by 2 * sqr2
						float xorDisc = 2.f * sqr2[i];
						xorFromVco1MinBlep[g].insertDiscontinuity(subsample, xorDisc, i);
					}
				}
			}

			// VCO1 falling edge (PWM threshold)
			float_4 vco1FallingEdge = (vco1.oldPhase[g] < pwm1_4) & (vco1.phase[g] >= pwm1_4);
			int vco1FallMask = simd::movemask(vco1FallingEdge);
			if (vco1FallMask) {
				for (int i = 0; i < 4; i++) {
					if ((vco1FallMask & (1 << i)) && vco1.deltaPhase[g][i] > 0.f) {
						float subsample = (pwm1_4[i] - vco1.oldPhase[g][i]) / vco1.deltaPhase[g][i] - 1.f;
						// sqr1: +1 -> -1, so XOR changes by -2 * sqr2
						float xorDisc = -2.f * sqr2[i];
						xorFromVco1MinBlep[g].insertDiscontinuity(subsample, xorDisc, i);
					}
				}
			}

			// Combine MinBLEP corrections from both VCO1 and VCO2 edges
			xorOut += xorFromVco1MinBlep[g].process();

			// Phase 2: Apply sync resets AFTER both VCOs have processed (order matters for bidirectional)
			if (sync1Enabled && vco2WrapMask) {
				// VCO1 syncs to VCO2: when VCO2 wraps, reset VCO1
				vco1.applySync(g, vco2WrapMask, vco2.oldPhase[g], vco2.deltaPhase[g], pwm1_4,
				               saw1, sqr1, tri1);  // Output params modified by reference
			}
			if (sync2Enabled && vco1WrapMask) {
				// VCO2 syncs to VCO1: when VCO1 wraps, reset VCO2
				vco2.applySync(g, vco1WrapMask, vco1.oldPhase[g], vco1.deltaPhase[g], pwm2_4,
				               saw2, sqr2, tri2);  // Output params modified by reference
			}

			// Sub-oscillator: -1 octave below VCO1 base (simplified, no MinBLEP)
			float_4 subPitch = basePitch + octave1 - 1.f;
			float_4 subFreq = dsp::FREQ_C4 * dsp::exp2_taylor5(subPitch);
			subFreq = simd::clamp(subFreq, 1.f, 20000.f);

			// Simple phase accumulation
			subPhase[g] += subFreq * sampleTime;
			subPhase[g] -= simd::floor(subPhase[g]);

			// Generate waveforms (simple, no MinBLEP)
			float_4 subSquare = simd::ifelse(subPhase[g] < 0.5f, 1.f, -1.f);
			float_4 subSine = simd::sin(2.f * float(M_PI) * subPhase[g]);

			// Select based on switch
			float_4 subOut = (subWave < 0.5f) ? subSquare : subSine;

			// Output to dedicated SUB jack (reduced to ±2V for testing)
			// Sanitize: subOut is mathematically bounded but defend against upstream NaN
			float_4 subVoltage = subOut * 2.f;
			for (int i = 0; i < 4; i++) {
				if (!std::isfinite(subVoltage[i])) subVoltage[i] = 0.f;
			}
			outputs[SUB_OUTPUT].setVoltageSimd(subVoltage, c);

			// Mix both VCOs with CV-controlled volumes, plus sub-oscillator and XOR
			// Note: tri and sine still use scalar knob values (no CV per Context decision)
			float_4 mixed = (tri1 * triVol1 + sqr1 * sqr1Vol_4 + sine1 * sinVol1 + saw1 * saw1Vol_4
			              + tri2 * triVol2 + sqr2 * sqr2Vol_4 + sine2 * sinVol2 + saw2 * saw2Vol_4
			              + subOut * subVol_4
			              + xorOut * xorVol_4
			              ) * outputScale;

			// DC filtering - process per-voice
			for (int i = 0; i < groupChannels; i++) {
				dcFilters[c + i].setCutoffFreq(10.f / sampleRate);
				dcFilters[c + i].process(mixed[i]);
				float out = dcFilters[c + i].highpass() * 2.f;  // Reduced to ±2V for testing
				// Sanitize output: replace NaN/Inf with 0 to prevent propagation
				mixed[i] = std::isfinite(out) ? out : 0.f;

				// Per-voice outputs (only for voices 1-8)
				int voiceIdx = c + i;
				if (voiceIdx < 8) {
					outputs[VOICE1_OUTPUT + voiceIdx].setVoltage(mixed[i]);
				}
			}

			outputs[AUDIO_OUTPUT].setVoltageSimd(mixed, c);
		}

		// Set output channel count (CRITICAL for polyphonic operation)
		outputs[AUDIO_OUTPUT].setChannels(channels);
		outputs[SUB_OUTPUT].setChannels(channels);

		// Per-voice gate pass-through (voices 1-8)
		int gateChannels = inputs[GATE_INPUT].getChannels();
		for (int i = 0; i < 8; i++) {
			if (i < gateChannels) {
				outputs[GATE1_OUTPUT + i].setVoltage(inputs[GATE_INPUT].getVoltage(i));
			} else {
				outputs[GATE1_OUTPUT + i].setVoltage(0.f);
			}
		}

		// Mix output using horizontal sum for efficiency
		float_4 mixSum = 0.f;
		for (int g = 0; g < (channels + 3) / 4; g++) {
			mixSum += outputs[AUDIO_OUTPUT].getVoltageSimd<float_4>(g * 4);
		}
		mixSum.v = _mm_hadd_ps(mixSum.v, mixSum.v);
		mixSum.v = _mm_hadd_ps(mixSum.v, mixSum.v);
		// Proportional mix: voices sum together (more voices = louder mix)
		float mixOut = mixSum[0];
		// Sanitize mix output
		outputs[MIX_OUTPUT].setVoltage(std::isfinite(mixOut) ? mixOut : 0.f);

		// PWM CV activity indicators
		if (inputs[PWM1_INPUT].isConnected()) {
			float peakCV = 0.f;
			for (int i = 0; i < channels; i++) {
				peakCV = std::max(peakCV, std::abs(inputs[PWM1_INPUT].getVoltage(i)));
			}
			lights[PWM1_CV_LIGHT].setBrightness(peakCV / 5.f);
		} else {
			lights[PWM1_CV_LIGHT].setBrightness(0.f);
		}

		if (inputs[PWM2_INPUT].isConnected()) {
			float peakCV = 0.f;
			for (int i = 0; i < channels; i++) {
				peakCV = std::max(peakCV, std::abs(inputs[PWM2_INPUT].getVoltage(i)));
			}
			lights[PWM2_CV_LIGHT].setBrightness(peakCV / 5.f);
		} else {
			lights[PWM2_CV_LIGHT].setBrightness(0.f);
		}

		// FM CV activity indicator
		if (inputs[FM_INPUT].isConnected()) {
			float peakCV = 0.f;
			int fmChannels = inputs[FM_INPUT].getChannels();
			for (int i = 0; i < fmChannels; i++) {
				peakCV = std::max(peakCV, std::abs(inputs[FM_INPUT].getVoltage(i)));
			}
			lights[FM_CV_LIGHT].setBrightness(peakCV / 5.f);
		} else {
			lights[FM_CV_LIGHT].setBrightness(0.f);
		}
	}
};


struct HydraQuartetVCOWidget : ModuleWidget {
	HydraQuartetVCOWidget(HydraQuartetVCO* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/HydraQuartetVCO.svg")));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// VCO1 Section (left side) - positions match SVG component layer
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(15.24, 28.0)), module, HydraQuartetVCO::OCTAVE1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(45.0, 28.0)), module, HydraQuartetVCO::DETUNE1_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 48.0)), module, HydraQuartetVCO::TRI1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.4, 48.0)), module, HydraQuartetVCO::SQR1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(40.64, 48.0)), module, HydraQuartetVCO::SIN1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(55.88, 48.0)), module, HydraQuartetVCO::SAW1_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 68.0)), module, HydraQuartetVCO::PWM1_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(35.56, 68.0)), module, HydraQuartetVCO::SYNC1_PARAM));
		// PWM1 attenuverter - small knob above the CV input
		addParam(createParamCentered<Trimpot>(mm2px(Vec(55.88, 58.0)), module, HydraQuartetVCO::PWM1_ATT_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(55.88, 68.0)), module, HydraQuartetVCO::PWM1_INPUT));
		// PWM1 CV activity LED - positioned near the input port
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(60.0, 68.0)), module, HydraQuartetVCO::PWM1_CV_LIGHT));

		// Sub-oscillator controls (VCO1 section, near bottom)
		// Sub level knob
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 88.0)), module, HydraQuartetVCO::SUB_LEVEL_PARAM));
		// Sub waveform switch (toggle between square and sine)
		addParam(createParamCentered<CKSS>(mm2px(Vec(25.4, 88.0)), module, HydraQuartetVCO::SUB_WAVE_PARAM));
		// Sub output jack
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(40.64, 88.0)), module, HydraQuartetVCO::SUB_OUTPUT));

		// Center Global Section
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(91.44, 85.0)), module, HydraQuartetVCO::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(91.44, 100.0)), module, HydraQuartetVCO::GATE_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(91.44, 115.0)), module, HydraQuartetVCO::AUDIO_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(91.44, 125.0)), module, HydraQuartetVCO::MIX_OUTPUT));

		// VCO2 Section (right side) - positions match SVG component layer
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(137.0, 28.0)), module, HydraQuartetVCO::OCTAVE2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(167.64, 28.0)), module, HydraQuartetVCO::FINE2_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(127.0, 48.0)), module, HydraQuartetVCO::TRI2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(142.24, 48.0)), module, HydraQuartetVCO::SQR2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(157.48, 48.0)), module, HydraQuartetVCO::SIN2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(172.72, 48.0)), module, HydraQuartetVCO::SAW2_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(127.0, 68.0)), module, HydraQuartetVCO::PWM2_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(147.32, 68.0)), module, HydraQuartetVCO::SYNC2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(167.64, 68.0)), module, HydraQuartetVCO::FM_PARAM));
		// PWM2 attenuverter - small knob above the CV input
		addParam(createParamCentered<Trimpot>(mm2px(Vec(127.0, 75.0)), module, HydraQuartetVCO::PWM2_ATT_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(127.0, 85.0)), module, HydraQuartetVCO::PWM2_INPUT));
		// PWM2 CV activity LED - positioned near the input port
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(131.0, 85.0)), module, HydraQuartetVCO::PWM2_CV_LIGHT));
		// FM attenuverter - small knob above the CV input
		addParam(createParamCentered<Trimpot>(mm2px(Vec(147.32, 75.0)), module, HydraQuartetVCO::FM_ATT_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(147.32, 85.0)), module, HydraQuartetVCO::FM_INPUT));
		// FM CV activity LED - positioned near the input port
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(151.32, 85.0)), module, HydraQuartetVCO::FM_CV_LIGHT));

		// Per-voice outputs (bottom of panel, split left/right around global section)
		// Voice outputs row (y=103) - voices 1-4 left, 5-8 right
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.0, 103.0)), module, HydraQuartetVCO::VOICE1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(23.0, 103.0)), module, HydraQuartetVCO::VOICE2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(36.0, 103.0)), module, HydraQuartetVCO::VOICE3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(49.0, 103.0)), module, HydraQuartetVCO::VOICE4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(135.0, 103.0)), module, HydraQuartetVCO::VOICE5_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(148.0, 103.0)), module, HydraQuartetVCO::VOICE6_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(161.0, 103.0)), module, HydraQuartetVCO::VOICE7_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(174.0, 103.0)), module, HydraQuartetVCO::VOICE8_OUTPUT));

		// Gate outputs row (y=118) - gates 1-4 left, 5-8 right
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.0, 118.0)), module, HydraQuartetVCO::GATE1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(23.0, 118.0)), module, HydraQuartetVCO::GATE2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(36.0, 118.0)), module, HydraQuartetVCO::GATE3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(49.0, 118.0)), module, HydraQuartetVCO::GATE4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(135.0, 118.0)), module, HydraQuartetVCO::GATE5_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(148.0, 118.0)), module, HydraQuartetVCO::GATE6_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(161.0, 118.0)), module, HydraQuartetVCO::GATE7_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(174.0, 118.0)), module, HydraQuartetVCO::GATE8_OUTPUT));
	}
};


Model* modelHydraQuartetVCO = createModel<HydraQuartetVCO, HydraQuartetVCOWidget>("HydraQuartetVCO");
