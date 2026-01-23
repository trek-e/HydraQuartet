#include "plugin.hpp"
#include <cmath>

using simd::float_4;


struct TriaxVCO : Module {
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

	// Phase state for 16 channels (4 groups of 4 for SIMD)
	float_4 phase[4] = {};

	TriaxVCO() {
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

		// Read SIN1 volume knob (placeholder - Phase 2 will add all waveforms)
		float sinVolume = params[SIN1_PARAM].getValue();

		// Mix accumulator for mono sum
		float mix = 0.f;

		// Process 4 channels at a time (SIMD)
		for (int c = 0; c < channels; c += 4) {
			// Get V/Oct pitch (polyphonic, spreads mono to all if mono source)
			float_4 pitch = inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c);

			// Convert V/Oct to frequency
			// 0V = C4 (261.6256 Hz), 1V = C5, etc.
			float_4 freq = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch);

			// Accumulate phase
			float_4 deltaPhase = freq * args.sampleTime;
			phase[c / 4] += deltaPhase;
			phase[c / 4] -= simd::floor(phase[c / 4]); // Wrap to [0, 1)

			// Generate simple sine wave scaled by SIN1 volume knob
			// Phase 2 will add triangle, square, sawtooth with antialiasing
			float_4 output = simd::sin(2.f * M_PI * phase[c / 4]) * 5.f * sinVolume;

			// Write to polyphonic output
			outputs[AUDIO_OUTPUT].setVoltageSimd(output, c);

			// Accumulate to mix (sum all active channels in this SIMD block)
			int blockChannels = std::min(4, channels - c);
			for (int i = 0; i < blockChannels; i++) {
				mix += output[i];
			}
		}

		// Set output channel count (CRITICAL for polyphonic operation)
		outputs[AUDIO_OUTPUT].setChannels(channels);

		// Mix output: average of all voices
		outputs[MIX_OUTPUT].setVoltage(mix / channels);
	}
};


struct TriaxVCOWidget : ModuleWidget {
	TriaxVCOWidget(TriaxVCO* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/TriaxVCO.svg")));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// VCO1 Section (left side) - positions match SVG component layer
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(15.24, 28.0)), module, TriaxVCO::OCTAVE1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(45.0, 28.0)), module, TriaxVCO::DETUNE1_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 48.0)), module, TriaxVCO::TRI1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.4, 48.0)), module, TriaxVCO::SQR1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(40.64, 48.0)), module, TriaxVCO::SIN1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(55.88, 48.0)), module, TriaxVCO::SAW1_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 68.0)), module, TriaxVCO::PWM1_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(35.56, 68.0)), module, TriaxVCO::SYNC1_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(55.88, 68.0)), module, TriaxVCO::PWM1_INPUT));

		// Center Global Section
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(91.44, 85.0)), module, TriaxVCO::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(91.44, 100.0)), module, TriaxVCO::GATE_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(91.44, 115.0)), module, TriaxVCO::AUDIO_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(91.44, 125.0)), module, TriaxVCO::MIX_OUTPUT));

		// VCO2 Section (right side) - positions match SVG component layer
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(137.0, 28.0)), module, TriaxVCO::OCTAVE2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(167.64, 28.0)), module, TriaxVCO::FINE2_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(127.0, 48.0)), module, TriaxVCO::TRI2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(142.24, 48.0)), module, TriaxVCO::SQR2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(157.48, 48.0)), module, TriaxVCO::SIN2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(172.72, 48.0)), module, TriaxVCO::SAW2_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(127.0, 68.0)), module, TriaxVCO::PWM2_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(147.32, 68.0)), module, TriaxVCO::SYNC2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(167.64, 68.0)), module, TriaxVCO::FM_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(127.0, 85.0)), module, TriaxVCO::PWM2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(147.32, 85.0)), module, TriaxVCO::FM_INPUT));
	}
};


Model* modelTriaxVCO = createModel<TriaxVCO, TriaxVCOWidget>("TriaxVCO");
