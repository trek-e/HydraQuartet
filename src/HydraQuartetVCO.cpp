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
		PWM1_ATT_PARAM,
		SYNC1_PARAM,
		// VCO2 Section
		OCTAVE2_PARAM,
		FINE2_PARAM,
		TRI2_PARAM,
		SQR2_PARAM,
		SIN2_PARAM,
		SAW2_PARAM,
		PWM2_PARAM,
		PWM2_ATT_PARAM,
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
		PWM1_CV_LIGHT,
		PWM2_CV_LIGHT,
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
		configParam(PWM1_ATT_PARAM, -1.f, 1.f, 0.f, "VCO1 PWM CV Attenuverter", "%", 0.f, 100.f);
		configSwitch(SYNC1_PARAM, 0.f, 1.f, 0.f, "VCO1 Sync", {"Off", "Hard"});

		// VCO2 Parameters
		configSwitch(OCTAVE2_PARAM, -2.f, 2.f, 0.f, "VCO2 Octave", {"-2", "-1", "0", "+1", "+2"});
		configParam(FINE2_PARAM, -1.f, 1.f, 0.f, "VCO2 Fine Tune", " cents", 0.f, 100.f);
		configParam(TRI2_PARAM, 0.f, 1.f, 0.f, "VCO2 Triangle");
		configParam(SQR2_PARAM, 0.f, 1.f, 1.f, "VCO2 Square");
		configParam(SIN2_PARAM, 0.f, 1.f, 0.f, "VCO2 Sine");
		configParam(SAW2_PARAM, 0.f, 1.f, 0.f, "VCO2 Sawtooth");
		configParam(PWM2_PARAM, 0.f, 1.f, 0.5f, "VCO2 Pulse Width", "%", 0.f, 100.f);
		configParam(PWM2_ATT_PARAM, -1.f, 1.f, 0.f, "VCO2 PWM CV Attenuverter", "%", 0.f, 100.f);
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
		float sqrVol1 = params[SQR1_PARAM].getValue();
		float sinVol1 = params[SIN1_PARAM].getValue();
		float sawVol1 = params[SAW1_PARAM].getValue();

		// Read VCO2 parameters
		float pwm2 = params[PWM2_PARAM].getValue();
		float triVol2 = params[TRI2_PARAM].getValue();
		float sqrVol2 = params[SQR2_PARAM].getValue();
		float sinVol2 = params[SIN2_PARAM].getValue();
		float sawVol2 = params[SAW2_PARAM].getValue();

		// Read PWM CV attenuverters
		float pwm1Att = params[PWM1_ATT_PARAM].getValue();
		float pwm2Att = params[PWM2_ATT_PARAM].getValue();

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

			// Process both VCO engines
			float_4 saw1, sqr1, tri1, sine1;
			float_4 saw2, sqr2, tri2, sine2;
			vco1.process(g, freq1, sampleTime, pwm1_4, saw1, sqr1, tri1, sine1);
			vco2.process(g, freq2, sampleTime, pwm2_4, saw2, sqr2, tri2, sine2);

			// Mix both VCOs with independent volume controls
			float_4 mixed = tri1 * triVol1 + sqr1 * sqrVol1 + sine1 * sinVol1 + saw1 * sawVol1
			              + tri2 * triVol2 + sqr2 * sqrVol2 + sine2 * sinVol2 + saw2 * sawVol2;

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
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(147.32, 85.0)), module, HydraQuartetVCO::FM_INPUT));
	}
};


Model* modelHydraQuartetVCO = createModel<HydraQuartetVCO, HydraQuartetVCOWidget>("HydraQuartetVCO");
