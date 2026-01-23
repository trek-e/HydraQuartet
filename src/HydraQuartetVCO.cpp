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
	MinBlepBuffer<32> sawMinBlepBuffer[4];
	MinBlepBuffer<32> sqrMinBlepBuffer[4];
	float_4 triState[4] = {};

	// Process one SIMD group (4 voices), returns 4 waveforms via output parameters
	// g: SIMD group index (0-3)
	// freq: frequency for 4 voices
	// sampleTime: 1/sampleRate
	// pwm: pulse width for 4 voices
	void process(int g, float_4 freq, float sampleTime, float_4 pwm,
	             float_4& saw, float_4& sqr, float_4& tri, float_4& sine) {
		// Phase accumulation with SIMD
		float_4 deltaPhase = simd::clamp(freq * sampleTime, 0.f, 0.49f);
		float_4 oldPhase = phase[g];
		phase[g] += deltaPhase;

		// Detect phase wrap
		float_4 wrapped = phase[g] >= 1.f;
		phase[g] -= simd::floor(phase[g]);  // Handles large FM jumps

		// === SAWTOOTH with strided MinBLEP ===
		int wrapMask = simd::movemask(wrapped);
		if (wrapMask) {
			for (int i = 0; i < 4; i++) {
				if (wrapMask & (1 << i)) {
					float subsample = (1.f - oldPhase[i]) / deltaPhase[i] - 1.f;
					sawMinBlepBuffer[g].insertDiscontinuity(subsample, -2.f, i);
				}
			}
		}
		saw = 2.f * phase[g] - 1.f + sawMinBlepBuffer[g].process();

		// === SQUARE with PWM using strided MinBLEP ===
		// Falling edge detection (phase crosses PWM threshold)
		float_4 fallingEdge = (oldPhase < pwm) & (phase[g] >= pwm);
		int fallMask = simd::movemask(fallingEdge);
		if (fallMask) {
			for (int i = 0; i < 4; i++) {
				if (fallMask & (1 << i)) {
					float subsample = (pwm[i] - oldPhase[i]) / deltaPhase[i] - 1.f;
					sqrMinBlepBuffer[g].insertDiscontinuity(subsample, -2.f, i);
				}
			}
		}

		// Rising edge on wrap
		if (wrapMask) {
			for (int i = 0; i < 4; i++) {
				if (wrapMask & (1 << i)) {
					float subsample = (1.f - oldPhase[i]) / deltaPhase[i] - 1.f;
					sqrMinBlepBuffer[g].insertDiscontinuity(subsample, 2.f, i);
				}
			}
		}

		sqr = simd::ifelse(phase[g] < pwm, 1.f, -1.f) + sqrMinBlepBuffer[g].process();

		// === TRIANGLE via integration ===
		triState[g] = triState[g] * 0.999f + sqr * 4.f * freq * sampleTime;
		tri = triState[g];

		// === SINE (no antialiasing needed) ===
		sine = simd::sin(2.f * float(M_PI) * phase[g]);
	}
};

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
		SYNC1_PARAM,
		// VCO2 Section
		OCTAVE2_PARAM,
		FINE2_PARAM,
		TRI2_PARAM,
		SQR2_PARAM,
		SIN2_PARAM,
		SAW2_PARAM,
		PWM2_PARAM,
		SYNC2_PARAM,
		FM_PARAM,
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
		INPUTS_LEN
	};
	enum OutputId {
		AUDIO_OUTPUT,
		MIX_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	// Dual VCO engines (each encapsulates phase, MinBLEP buffers, tri state)
	VcoEngine vco1;
	VcoEngine vco2;

	// DC filters kept scalar (not in hot path, operate on mixed output)
	dsp::TRCFilter<float> dcFilters[16];

	HydraQuartetVCO() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// VCO1 Parameters
		configSwitch(OCTAVE1_PARAM, -2.f, 2.f, 0.f, "VCO1 Octave", {"-2", "-1", "0", "+1", "+2"});
		configParam(DETUNE1_PARAM, 0.f, 1.f, 0.f, "VCO1 Detune");
		configParam(TRI1_PARAM, 0.f, 1.f, 0.f, "VCO1 Triangle");
		configParam(SQR1_PARAM, 0.f, 1.f, 1.f, "VCO1 Square");
		configParam(SIN1_PARAM, 0.f, 1.f, 1.f, "VCO1 Sine");
		configParam(SAW1_PARAM, 0.f, 1.f, 0.f, "VCO1 Sawtooth");
		configParam(PWM1_PARAM, 0.f, 1.f, 0.5f, "VCO1 Pulse Width", "%", 0.f, 100.f);
		configSwitch(SYNC1_PARAM, 0.f, 1.f, 0.f, "VCO1 Sync", {"Off", "Hard"});

		// VCO2 Parameters
		configSwitch(OCTAVE2_PARAM, -2.f, 2.f, 0.f, "VCO2 Octave", {"-2", "-1", "0", "+1", "+2"});
		configParam(FINE2_PARAM, -1.f, 1.f, 0.f, "VCO2 Fine Tune", " cents", 0.f, 100.f);
		configParam(TRI2_PARAM, 0.f, 1.f, 0.f, "VCO2 Triangle");
		configParam(SQR2_PARAM, 0.f, 1.f, 1.f, "VCO2 Square");
		configParam(SIN2_PARAM, 0.f, 1.f, 0.f, "VCO2 Sine");
		configParam(SAW2_PARAM, 0.f, 1.f, 0.f, "VCO2 Sawtooth");
		configParam(PWM2_PARAM, 0.f, 1.f, 0.5f, "VCO2 Pulse Width", "%", 0.f, 100.f);
		configSwitch(SYNC2_PARAM, 0.f, 1.f, 0.f, "VCO2 Sync", {"Off", "Hard"});
		configParam(FM_PARAM, 0.f, 1.f, 0.f, "FM Amount");

		// Inputs
		configInput(VOCT_INPUT, "V/Oct");
		configInput(GATE_INPUT, "Gate");
		configInput(PWM1_INPUT, "VCO1 PWM CV");
		configInput(PWM2_INPUT, "VCO2 PWM CV");
		configInput(FM_INPUT, "FM CV");

		// Outputs
		configOutput(AUDIO_OUTPUT, "Polyphonic Audio");
		configOutput(MIX_OUTPUT, "Mix");
	}

	void process(const ProcessArgs& args) override {
		// Get channel count from V/Oct input (minimum 1)
		int channels = std::max(1, inputs[VOCT_INPUT].getChannels());

		// Read VCO1 parameters (outside loop - same for all voices)
		float pwm1 = params[PWM1_PARAM].getValue();
		float triVol = params[TRI1_PARAM].getValue();
		float sqrVol = params[SQR1_PARAM].getValue();
		float sinVol = params[SIN1_PARAM].getValue();
		float sawVol = params[SAW1_PARAM].getValue();
		float sampleTime = args.sampleTime;
		float sampleRate = args.sampleRate;

		// Broadcast scalar PWM to SIMD
		float_4 pwm4 = pwm1;

		// Process in SIMD groups of 4 voices
		for (int c = 0; c < channels; c += 4) {
			int groupChannels = std::min(channels - c, 4);
			int g = c / 4;  // SIMD group index

			// Load 4 channels of V/Oct using SIMD
			float_4 pitch = inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c);

			// Convert V/Oct to frequency (0V = C4 = 261.6 Hz)
			float_4 freq = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch);

			// Clamp frequency to safe range
			freq = simd::clamp(freq, 0.1f, sampleRate / 2.f);

			// Process VCO1 through engine
			float_4 saw1, sqr1, tri1, sine1;
			vco1.process(g, freq, sampleTime, pwm4, saw1, sqr1, tri1, sine1);

			// Mix VCO1 waveforms with volume controls
			float_4 mixed = tri1 * triVol + sqr1 * sqrVol + sine1 * sinVol + saw1 * sawVol;

			// DC filtering - process per-voice (not in critical path)
			for (int i = 0; i < groupChannels; i++) {
				dcFilters[c + i].setCutoffFreq(10.f / sampleRate);
				dcFilters[c + i].process(mixed[i] * 5.f);
				mixed[i] = dcFilters[c + i].highpass();
			}

			outputs[AUDIO_OUTPUT].setVoltageSimd(mixed, c);
		}

		// Set output channel count (CRITICAL for polyphonic operation)
		outputs[AUDIO_OUTPUT].setChannels(channels);

		// Mix output using horizontal sum for efficiency
		float_4 mixSum = 0.f;
		for (int g = 0; g < (channels + 3) / 4; g++) {
			mixSum += outputs[AUDIO_OUTPUT].getVoltageSimd<float_4>(g * 4);
		}
		mixSum.v = _mm_hadd_ps(mixSum.v, mixSum.v);
		mixSum.v = _mm_hadd_ps(mixSum.v, mixSum.v);
		outputs[MIX_OUTPUT].setVoltage(mixSum[0] / channels);
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
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(55.88, 68.0)), module, HydraQuartetVCO::PWM1_INPUT));

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
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(127.0, 85.0)), module, HydraQuartetVCO::PWM2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(147.32, 85.0)), module, HydraQuartetVCO::FM_INPUT));
	}
};


Model* modelHydraQuartetVCO = createModel<HydraQuartetVCO, HydraQuartetVCOWidget>("HydraQuartetVCO");
