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
		// VCO1 Section (3x3 grid)
		// Row 1: Detune, Octave (Pipe Length), FM Source
		DETUNE1_PARAM,
		OCTAVE1_PARAM,
		FM_SOURCE_PARAM,
		// Row 2: Sub, Triangle, Sine
		SUB_LEVEL_PARAM,
		TRI1_PARAM,
		SIN1_PARAM,
		// Row 3: PW, Square, Saw
		PWM1_PARAM,
		SQR1_PARAM,
		SAW1_PARAM,
		// Other VCO1 controls
		SYNC1_PARAM,
		SUB_WAVE_PARAM,
		VIBRATO1_PARAM,
		// VCO2 Section (3x3 grid)
		// Row 1: FM, Pipe Length (Octave), Fine Tune
		FM_PARAM,
		OCTAVE2_PARAM,
		FINE2_PARAM,
		// Row 2: Sin, Triangle, XOR
		SIN2_PARAM,
		TRI2_PARAM,
		XOR_PARAM,
		// Row 3: Saw, Square, PWM
		SAW2_PARAM,
		SQR2_PARAM,
		PWM2_PARAM,
		// Other VCO2 controls
		SYNC2_PARAM,
		VIBRATO2_PARAM,
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
		GATE_MIX_OUTPUT,  // Mono gate (OR of all gates)
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

	// Vibrato LFO state (shared sine LFO at ~5.5Hz)
	float vibratoPhase = 0.f;

	HydraQuartetVCO() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// VCO1 Parameters (3x3 grid layout)
		// Row 1: Detune, Octave (Pipe Length), FM Source
		configParam(DETUNE1_PARAM, 0.f, 1.f, 0.f, "VCO1 Detune");
		configSwitch(OCTAVE1_PARAM, -2.f, 2.f, 0.f, "VCO1 Pipe Length", {"16'", "8'", "4'", "2'", "1'"});
		configSwitch(FM_SOURCE_PARAM, 0.f, 4.f, 0.f, "FM Source", {"Sine", "Triangle", "Saw", "Square", "Sub"});
		// Row 2: Sub, Triangle, Sine
		configParam(SUB_LEVEL_PARAM, 0.f, 10.f, 0.f, "Sub Level");
		configParam(TRI1_PARAM, 0.f, 10.f, 0.f, "VCO1 Triangle");
		configParam(SIN1_PARAM, 0.f, 10.f, 1.f, "VCO1 Sine");
		// Row 3: PW, Square, Saw
		configParam(PWM1_PARAM, 0.f, 1.f, 0.5f, "VCO1 Pulse Width", "%", 0.f, 100.f);
		configParam(SQR1_PARAM, 0.f, 10.f, 1.f, "VCO1 Square");
		configParam(SAW1_PARAM, 0.f, 10.f, 0.f, "VCO1 Sawtooth");
		// Other VCO1 controls
		configSwitch(SYNC1_PARAM, 0.f, 2.f, 1.f, "VCO1 Sync", {"Hard", "Off", "Soft"});  // Center = Off
		configSwitch(SUB_WAVE_PARAM, 0.f, 1.f, 0.f, "Sub Waveform", {"Square", "Sine"});
		configParam(VIBRATO1_PARAM, 0.f, 1.f, 0.f, "VCO1 Vibrato", "%", 0.f, 100.f);

		// VCO2 Parameters (3x3 grid layout)
		// Row 1: FM, Pipe Length (Octave), Fine Tune
		configParam(FM_PARAM, 0.f, 10.f, 0.f, "FM Amount");
		configSwitch(OCTAVE2_PARAM, -2.f, 1.f, 0.f, "VCO2 Pipe Length", {"16'", "8'", "4'", "2'"});
		configParam(FINE2_PARAM, 0.f, 10.f, 0.f, "VCO2 Fine Tune");  // 0-5=0-1st, 5-10=+12st
		// Row 2: Sin, Triangle, XOR
		configParam(SIN2_PARAM, 0.f, 10.f, 0.f, "VCO2 Sine");
		configParam(TRI2_PARAM, 0.f, 10.f, 0.f, "VCO2 Triangle");
		configParam(XOR_PARAM, 0.f, 10.f, 0.f, "XOR Volume");
		// Row 3: Saw, Square, PWM
		configParam(SAW2_PARAM, 0.f, 10.f, 0.f, "VCO2 Sawtooth");
		configParam(SQR2_PARAM, 0.f, 10.f, 1.f, "VCO2 Square");
		configParam(PWM2_PARAM, 0.f, 1.f, 0.5f, "VCO2 Pulse Width", "%", 0.f, 100.f);
		// Other VCO2 controls
		configSwitch(SYNC2_PARAM, 0.f, 2.f, 1.f, "VCO2 Sync", {"Hard", "Off", "Soft"});  // Center = Off
		configParam(VIBRATO2_PARAM, 0.f, 1.f, 0.f, "VCO2 Vibrato", "%", 0.f, 100.f);

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
		configOutput(GATE_MIX_OUTPUT, "Gate Mix");
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

		// Read FM parameters
		float fmKnob = params[FM_PARAM].getValue() * 0.1f;  // 0-10 knob scaled to 0-1
		int fmSource = (int)std::round(params[FM_SOURCE_PARAM].getValue());  // 0=Sin, 1=Tri, 2=Saw, 3=Sqr, 4=Sub

		// Read VCO2 fine tune with special scaling
		// 0-5 = 0-1 semitone (fine), 5-10 = +1 to +13 semitones (coarse)
		float fineTuneKnob = params[FINE2_PARAM].getValue();
		float fineTuneSemitones;
		if (fineTuneKnob <= 5.f) {
			// 0-5 maps to 0-1 semitone (fine tuning)
			fineTuneSemitones = fineTuneKnob / 5.f;
		} else {
			// 5-10 maps to 1-13 semitones (+12 more)
			fineTuneSemitones = 1.f + (fineTuneKnob - 5.f) * 12.f / 5.f;
		}
		float fineTuneVolts = fineTuneSemitones / 12.f;  // Convert semitones to V/Oct

		// Read sub-oscillator parameters
		float subWave = params[SUB_WAVE_PARAM].getValue();  // 0 = square, 1 = sine

		// Fixed output scaling - divide by 3 (typical number of active waveforms)
		// User controls final level via individual waveform volumes
		const float outputScale = 1.f / 3.f;

		// Read sync switch states (0=Hard, 1=Off, 2=Soft)
		int sync1Mode = (int)std::round(params[SYNC1_PARAM].getValue());  // VCO1 syncs to VCO2
		int sync2Mode = (int)std::round(params[SYNC2_PARAM].getValue());  // VCO2 syncs to VCO1
		bool sync1Hard = (sync1Mode == 0);
		bool sync1Soft = (sync1Mode == 2);
		bool sync2Hard = (sync2Mode == 0);
		bool sync2Soft = (sync2Mode == 2);

		// Read vibrato parameters (0-1 range)
		float vibrato1Depth = params[VIBRATO1_PARAM].getValue();
		float vibrato2Depth = params[VIBRATO2_PARAM].getValue();

		// Update vibrato LFO (5.5 Hz sine, typical vibrato rate)
		const float vibratoRate = 5.5f;
		vibratoPhase += vibratoRate * sampleTime;
		if (vibratoPhase >= 1.f) vibratoPhase -= 1.f;
		float vibratoLfo = std::sin(vibratoPhase * 2.f * M_PI);

		// Vibrato modulation in V/Oct (max +/- 0.5 semitone = +/- 1/24 volt)
		float vibratoMod1 = vibratoLfo * vibrato1Depth * (0.5f / 12.f);
		float vibratoMod2 = vibratoLfo * vibrato2Depth * (0.5f / 12.f);

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

			// VCO1: base + octave + detune + vibrato (VCO1 gets detune for thickness)
			float_4 pitch1 = basePitch + octave1 + detuneVolts + vibratoMod1;
			float_4 freq1 = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch1);
			freq1 = simd::clamp(freq1, 0.1f, sampleRate / 2.f);

			// VCO2: base + octave + fine tune + vibrato
			float_4 pitch2 = basePitch + octave2 + fineTuneVolts + vibratoMod2;
			float_4 freq2Base = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch2);

			// Read polyphonic PWM CV
			float_4 pwm1CV = inputs[PWM1_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 pwm2CV = inputs[PWM2_INPUT].getPolyVoltageSimd<float_4>(c);

			// Apply CV: +/-5V * 0.1 = +/-0.5 contribution (full sweep range)
			float_4 pwm1_4 = pwm1 + pwm1CV * 0.1f;
			float_4 pwm2_4 = pwm2 + pwm2CV * 0.1f;

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

			// Phase 1: Process VCO1 first to get waveforms for FM source
			float_4 saw1, sqr1, tri1, sine1;
			int vco1WrapMask;
			vco1.process(g, freq1, sampleTime, pwm1_4, saw1, sqr1, tri1, sine1, vco1WrapMask);

			// Sub-oscillator: -1 octave below VCO1 base (need this early for FM source)
			float_4 subPitch = basePitch + octave1 - 1.f;
			float_4 subFreq = dsp::FREQ_C4 * dsp::exp2_taylor5(subPitch);
			subFreq = simd::clamp(subFreq, 1.f, 20000.f);
			subPhase[g] += subFreq * sampleTime;
			subPhase[g] -= simd::floor(subPhase[g]);
			float_4 subSquare = simd::ifelse(subPhase[g] < 0.5f, 1.f, -1.f);
			float_4 subSine = simd::sin(2.f * float(M_PI) * subPhase[g]);
			float_4 subOut = (subWave < 0.5f) ? subSquare : subSine;

			// Select FM source waveform (0=Sin, 1=Tri, 2=Saw, 3=Sqr, 4=Sub)
			float_4 fmModulator;
			switch (fmSource) {
				case 0: fmModulator = sine1; break;
				case 1: fmModulator = tri1; break;
				case 2: fmModulator = saw1; break;
				case 3: fmModulator = sqr1; break;
				case 4: fmModulator = subOut; break;
				default: fmModulator = sine1; break;
			}

			// Through-zero linear FM: selected VCO1 waveform modulates VCO2 frequency
			// Read FM CV (auto-detect poly/mono)
			float_4 fmCV;
			int fmChannels = inputs[FM_INPUT].getChannels();
			if (fmChannels > 1) {
				fmCV = inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c);
			} else {
				fmCV = float_4(inputs[FM_INPUT].getVoltage());
			}

			// Calculate per-voice FM depth: knob + (CV * scale)
			float_4 fmDepth = fmKnob + fmCV * 0.1f;
			fmDepth = simd::clamp(fmDepth, 0.f, 2.f);

			// Apply linear FM using selected waveform as modulator
			// fmModulator is ±1, so freq2 = freq2Base * (1 + fmModulator * fmDepth)
			float_4 freq2 = freq2Base + freq2Base * fmModulator * fmDepth;
			freq2 = simd::clamp(freq2, 0.1f, sampleRate / 2.f);

			// Phase 2: Process VCO2 with FM-modulated frequency
			float_4 saw2, sqr2, tri2, sine2, xorOut;
			int vco2WrapMask;
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
			// Hard sync: oscillator resets at the start of the other oscillator's cycle
			// Soft sync: sync amount based on master waveform magnitude
			if (sync1Hard && vco2WrapMask) {
				// VCO1 hard syncs to VCO2: when VCO2 wraps, reset VCO1
				vco1.applySync(g, vco2WrapMask, vco2.oldPhase[g], vco2.deltaPhase[g], pwm1_4,
				               saw1, sqr1, tri1);
			}
			if (sync1Soft && vco2WrapMask) {
				// VCO1 soft syncs to VCO2: sync amount proportional to VCO2 waveform magnitude
				for (int i = 0; i < 4; i++) {
					if (vco2WrapMask & (1 << i)) {
						// Use sine2 magnitude to determine sync strength (0-1)
						float magnitude = std::abs(sine2[i]);
						// Blend between current phase and reset phase based on magnitude
						vco1.phase[g][i] = vco1.phase[g][i] * (1.f - magnitude);
					}
				}
			}
			if (sync2Hard && vco1WrapMask) {
				// VCO2 hard syncs to VCO1: when VCO1 wraps, reset VCO2
				vco2.applySync(g, vco1WrapMask, vco1.oldPhase[g], vco1.deltaPhase[g], pwm2_4,
				               saw2, sqr2, tri2);
			}
			if (sync2Soft && vco1WrapMask) {
				// VCO2 soft syncs to VCO1: sync amount proportional to VCO1 waveform magnitude
				for (int i = 0; i < 4; i++) {
					if (vco1WrapMask & (1 << i)) {
						// Use sine1 magnitude to determine sync strength (0-1)
						float magnitude = std::abs(sine1[i]);
						// Blend between current phase and reset phase based on magnitude
						vco2.phase[g][i] = vco2.phase[g][i] * (1.f - magnitude);
					}
				}
			}

			// Output sub to dedicated SUB jack (reduced to ±2V for testing)
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

			// DC filtering and soft clipping - process per-voice
			for (int i = 0; i < groupChannels; i++) {
				dcFilters[c + i].setCutoffFreq(10.f / sampleRate);
				dcFilters[c + i].process(mixed[i]);
				float dcFiltered = dcFilters[c + i].highpass();

				// Soft clipping with tanh
				// Scale factor 3.0: saturates at approximately +/-3V input
				// This prevents harsh digital clipping when many waveforms sum
				float softClipped = 3.f * std::tanh(dcFiltered / 3.f);

				// Apply output scaling (+/-2V for testing, +/-5V for production)
				float out = softClipped * 2.f;

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
		float gateMix = 0.f;
		for (int i = 0; i < 8; i++) {
			if (i < gateChannels) {
				float gateVoltage = inputs[GATE_INPUT].getVoltage(i);
				outputs[GATE1_OUTPUT + i].setVoltage(gateVoltage);
				gateMix = std::max(gateMix, gateVoltage);  // OR-like behavior
			} else {
				outputs[GATE1_OUTPUT + i].setVoltage(0.f);
			}
		}
		outputs[GATE_MIX_OUTPUT].setVoltage(gateMix);

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

		// VCO1 Section - 3x3 grid in upper left
		// Grid spacing: 15mm horizontal, 20mm vertical
		// Starting position: x=12mm, y=25mm
		const float vco1X1 = 12.f, vco1X2 = 27.f, vco1X3 = 42.f;
		const float vco1X4 = 57.f;  // Extra column to right of grid for Vibrato
		const float vco1Y1 = 25.f, vco1Y2 = 45.f, vco1Y3 = 65.f;

		// Row 1: Detune, Pipe Length (Octave), FM Source
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco1X1, vco1Y1)), module, HydraQuartetVCO::DETUNE1_PARAM));
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(vco1X2, vco1Y1)), module, HydraQuartetVCO::OCTAVE1_PARAM));
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(vco1X3, vco1Y1)), module, HydraQuartetVCO::FM_SOURCE_PARAM));

		// Row 2: Sub, Triangle, Sine, Vibrato
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco1X1, vco1Y2)), module, HydraQuartetVCO::SUB_LEVEL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco1X2, vco1Y2)), module, HydraQuartetVCO::TRI1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco1X3, vco1Y2)), module, HydraQuartetVCO::SIN1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco1X4, vco1Y2)), module, HydraQuartetVCO::VIBRATO1_PARAM));

		// Row 3: Square, Saw (PWM1 moved to lower left corner)
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco1X1, vco1Y3)), module, HydraQuartetVCO::SQR1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco1X2, vco1Y3)), module, HydraQuartetVCO::SAW1_PARAM));

		// VCO1 CV inputs and additional controls (below 3x3 grid)
		const float vco1Y4 = 82.f;
		// Sub waveform switch
		addParam(createParamCentered<CKSS>(mm2px(Vec(vco1X1, vco1Y4)), module, HydraQuartetVCO::SUB_WAVE_PARAM));

		// CV inputs row
		const float vco1Y5 = 95.f;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(vco1X1, vco1Y5)), module, HydraQuartetVCO::SUB_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(vco1X2, vco1Y5)), module, HydraQuartetVCO::SQR1_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(vco1X3, vco1Y5)), module, HydraQuartetVCO::SAW1_CV_INPUT));
		// PWM CV input
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(vco1X1 + 10.f, vco1Y5)), module, HydraQuartetVCO::PWM1_INPUT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(vco1X1 + 14.f, vco1Y5)), module, HydraQuartetVCO::PWM1_CV_LIGHT));

		// Center Sync Section (top middle, 40HP center = 101.6mm)
		// 3-position horizontal switches: Hard - Off - Soft
		// Using CKSSThree rotated via SVG or custom component for horizontal
		// VCO1 Sync on top, VCO2 Sync below
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(101.6, 25.0)), module, HydraQuartetVCO::SYNC1_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(101.6, 40.0)), module, HydraQuartetVCO::SYNC2_PARAM));

		// Center Global Section (40HP center = 101.6mm)
		// Gate input in center
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(101.6, 60.0)), module, HydraQuartetVCO::GATE_INPUT));
		// Polyphonic audio output
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(101.6, 80.0)), module, HydraQuartetVCO::AUDIO_OUTPUT));
		// Sub output in center area
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(101.6, 95.0)), module, HydraQuartetVCO::SUB_OUTPUT));

		// Lower left corner: PWM1 knob above V/Oct input
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.0, 110.0)), module, HydraQuartetVCO::PWM1_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.0, 123.0)), module, HydraQuartetVCO::VOCT_INPUT));

		// VCO2 Section - 3x3 grid in upper right (40HP = 203.2mm)
		// Grid spacing: 15mm horizontal, 20mm vertical
		// Starting position: x=161mm, y=25mm (mirroring VCO1 from right)
		const float vco2X0 = 146.f;  // Extra column to left of grid for Vibrato
		const float vco2X1 = 161.f, vco2X2 = 176.f, vco2X3 = 191.f;
		const float vco2Y1 = 25.f, vco2Y2 = 45.f, vco2Y3 = 65.f;

		// Row 1: FM, Pipe Length (Octave), Fine Tune
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco2X1, vco2Y1)), module, HydraQuartetVCO::FM_PARAM));
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(vco2X2, vco2Y1)), module, HydraQuartetVCO::OCTAVE2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco2X3, vco2Y1)), module, HydraQuartetVCO::FINE2_PARAM));

		// Row 2: Vibrato (left of grid), Sin, Triangle, XOR
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco2X0, vco2Y2)), module, HydraQuartetVCO::VIBRATO2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco2X1, vco2Y2)), module, HydraQuartetVCO::SIN2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco2X2, vco2Y2)), module, HydraQuartetVCO::TRI2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco2X3, vco2Y2)), module, HydraQuartetVCO::XOR_PARAM));

		// Row 3: Saw, Square, PWM
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco2X1, vco2Y3)), module, HydraQuartetVCO::SAW2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco2X2, vco2Y3)), module, HydraQuartetVCO::SQR2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(vco2X3, vco2Y3)), module, HydraQuartetVCO::PWM2_PARAM));

		// VCO2 additional controls (below 3x3 grid)
		const float vco2Y4 = 82.f;

		// CV inputs row
		const float vco2Y5 = 95.f;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(vco2X1, vco2Y5)), module, HydraQuartetVCO::SAW2_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(vco2X2, vco2Y5)), module, HydraQuartetVCO::SQR2_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(vco2X3, vco2Y5)), module, HydraQuartetVCO::XOR_CV_INPUT));
		// PWM CV input
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(vco2X3 - 10.f, vco2Y4)), module, HydraQuartetVCO::PWM2_INPUT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(vco2X3 - 6.f, vco2Y4)), module, HydraQuartetVCO::PWM2_CV_LIGHT));
		// FM CV input
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(vco2X2, vco2Y4)), module, HydraQuartetVCO::FM_INPUT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(vco2X2 + 4.f, vco2Y4)), module, HydraQuartetVCO::FM_CV_LIGHT));

		// Bottom output section - centered layout
		// Row 1 (y=110): Gates 1-4 | Gate Mix | Gates 5-8
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.0, 110.0)), module, HydraQuartetVCO::GATE1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.0, 110.0)), module, HydraQuartetVCO::GATE2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(51.0, 110.0)), module, HydraQuartetVCO::GATE3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(64.0, 110.0)), module, HydraQuartetVCO::GATE4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(101.6, 110.0)), module, HydraQuartetVCO::GATE_MIX_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(139.0, 110.0)), module, HydraQuartetVCO::GATE5_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(152.0, 110.0)), module, HydraQuartetVCO::GATE6_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(165.0, 110.0)), module, HydraQuartetVCO::GATE7_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(178.0, 110.0)), module, HydraQuartetVCO::GATE8_OUTPUT));

		// Row 2 (y=123): Voices 1-4 | Audio Mix | Voices 5-8
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.0, 123.0)), module, HydraQuartetVCO::VOICE1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.0, 123.0)), module, HydraQuartetVCO::VOICE2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(51.0, 123.0)), module, HydraQuartetVCO::VOICE3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(64.0, 123.0)), module, HydraQuartetVCO::VOICE4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(101.6, 123.0)), module, HydraQuartetVCO::MIX_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(139.0, 123.0)), module, HydraQuartetVCO::VOICE5_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(152.0, 123.0)), module, HydraQuartetVCO::VOICE6_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(165.0, 123.0)), module, HydraQuartetVCO::VOICE7_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(178.0, 123.0)), module, HydraQuartetVCO::VOICE8_OUTPUT));
	}
};


Model* modelHydraQuartetVCO = createModel<HydraQuartetVCO, HydraQuartetVCOWidget>("HydraQuartetVCO");
