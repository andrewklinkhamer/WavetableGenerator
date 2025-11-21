#include "WaveformEffects.h"
#include "SpectralEffects.h"
#include "KissFFTProcessor.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace WavetableGen {
	namespace Core {
		constexpr double PI = 3.14159265358979323846;

		// Apply all effects in proper order
		void WaveformEffects::ApplyEffects(std::vector<float>& samples, const EffectsSettings& settings) {
			if (samples.empty()) return;

			// Order matters for quality:
			// 1. Symmetry operations (no frequency content changes)
			// 2. Aliasing-prone effects (with oversampling)
			// 3. Filtering (removes unwanted frequencies)

			// Step 1: Symmetry operations (safe, no oversampling needed)
			if (settings.reverse) {
				ApplyReverse(samples);
			}
			if (settings.mirrorHorizontal) {
				ApplyMirrorHorizontal(samples);
			}
			if (settings.mirrorVertical) {
				ApplyMirrorVertical(samples);
			}
			if (settings.invert) {
				ApplyInvert(samples);
			}

			// Step 2: Non-linear effects (require oversampling)
			if (settings.distortionType != DistortionType::None && settings.distortionAmount > 0.001f) {
				ApplyDistortion(samples, settings.distortionType, settings.distortionAmount);
			}
			if (settings.enableWavefold && settings.wavefoldAmount > 0.001f) {
				ApplyWavefold(samples, settings.wavefoldAmount);
			}
			if (settings.enableBitCrush && settings.bitDepth < 16) {
				ApplyBitCrush(samples, settings.bitDepth);
			}
			if (settings.enableSampleRateReduction && settings.sampleRateReductionFactor > 1) {
				ApplySampleRateReduction(samples, settings.sampleRateReductionFactor);
			}


			// Step 3: Filtering (removes aliasing and shapes spectrum)
			if (settings.enableHighPass && settings.highPassCutoff > 0.001f) {
				ApplyHighPassFilter(samples, settings.highPassCutoff);
			}
			if (settings.enableLowPass && settings.lowPassCutoff < 0.999f) {
				ApplyLowPassFilter(samples, settings.lowPassCutoff);
			}

			// Step 4: Spectral effects (frequency domain processing)
			if (settings.enableSpectralDecay && settings.spectralDecayAmount > 0.001f) {
				ApplySpectralDecay(samples, settings.spectralDecayAmount, settings.spectralDecayCurve);
			}
			if (settings.enableSpectralTilt && std::abs(settings.spectralTiltAmount) > 0.001f) {
				ApplySpectralTilt(samples, settings.spectralTiltAmount);
			}
			if (settings.enableSpectralGate && settings.spectralGateThreshold > 0.001f) {
				ApplySpectralGate(samples, settings.spectralGateThreshold);
			}
			if (settings.enablePhaseRandomize && settings.phaseRandomizeAmount > 0.001f) {
				ApplyPhaseRandomization(samples, settings.phaseRandomizeAmount);
			}
			if (settings.enableSpectralShift && settings.spectralShiftAmount != 0) {
				ApplySpectralShift(samples, settings.spectralShiftAmount);
			}

		}

		// === SAFE EFFECTS (No oversampling needed) ===

		void WaveformEffects::ApplyLowPassFilter(std::vector<float>& samples, float cutoff) {
			// Simple one-pole IIR low-pass filter
			// cutoff: 0.0-1.0 (fraction of Nyquist frequency)
			float alpha = cutoff;
			float prev = samples[0];

			for (size_t i = 1; i < samples.size(); ++i) {
				samples[i] = prev + alpha * (samples[i] - prev);
				prev = samples[i];
			}
		}

		void WaveformEffects::ApplyHighPassFilter(std::vector<float>& samples, float cutoff) {
			// Simple one-pole IIR high-pass filter
			float alpha = 1.0f - cutoff;
			float prev_input = samples[0];
			float prev_output = 0.0f;

			for (size_t i = 1; i < samples.size(); ++i) {
				float input = samples[i];
				float output = alpha * (prev_output + input - prev_input);
				samples[i] = output;
				prev_input = input;
				prev_output = output;
			}
		}

		void WaveformEffects::ApplyMirrorHorizontal(std::vector<float>& samples) {
			size_t half = samples.size() / 2;
			for (size_t i = 0; i < half; ++i) {
				samples[half + i] = samples[half - i - 1];
			}
		}

		void WaveformEffects::ApplyMirrorVertical(std::vector<float>& samples) {
			for (float& s : samples) {
				s = -s;
			}
		}

		void WaveformEffects::ApplyInvert(std::vector<float>& samples) {
			for (float& s : samples) {
				s = -s;
			}
		}

		void WaveformEffects::ApplyReverse(std::vector<float>& samples) {
			std::reverse(samples.begin(), samples.end());
		}

		// === ALIASING-PRONE EFFECTS (Use oversampling) ===

		void WaveformEffects::ApplyDistortion(std::vector<float>& samples, DistortionType type, float amount) {
			if (type == DistortionType::None || amount < 0.001f) return;

			// Oversample for anti-aliasing
			std::vector<float> oversampled = Oversample4x(samples);

			// Apply distortion at higher sample rate
			switch (type) {
			case DistortionType::Soft:
				ApplySoftDistortion(oversampled, amount);
				break;
			case DistortionType::Hard:
				ApplyHardDistortion(oversampled, amount);
				break;
			case DistortionType::Asymmetric:
				ApplyAsymmetricDistortion(oversampled, amount);
				break;
			default:
				break;
			}

			// Downsample with anti-aliasing filter
			samples = Downsample4x(oversampled);
		}

		void WaveformEffects::ApplyBitCrush(std::vector<float>& samples, int bits) {
			if (bits >= 16) return;

			// Oversample for anti-aliasing
			std::vector<float> oversampled = Oversample4x(samples);

			// Apply bit crushing at higher sample rate
			int levels = (1 << bits);
			float step = 2.0f / (float)levels;

			for (float& s : oversampled) {
				s = std::floor(s / step) * step;
			}

			// Downsample with anti-aliasing filter
			samples = Downsample4x(oversampled);
		}

		void WaveformEffects::ApplyWavefold(std::vector<float>& samples, float amount) {
			if (amount < 0.001f) return;

			// Oversample for anti-aliasing
			std::vector<float> oversampled = Oversample4x(samples);

			// Apply wavefolding at higher sample rate
			float gain = 1.0f + amount * 3.0f; // 1x to 4x gain
			for (float& s : oversampled) {
				float folded = s * gain;
				// Triangle folding
				while (folded > 1.0f || folded < -1.0f) {
					if (folded > 1.0f) folded = 2.0f - folded;
					if (folded < -1.0f) folded = -2.0f - folded;
				}
				s = folded;
			}

			// Downsample with anti-aliasing filter
			samples = Downsample4x(oversampled);
		}

		void WaveformEffects::ApplySampleRateReduction(std::vector<float>& samples, int factor) {
			if (factor <= 1) return;

			// Oversample for anti-aliasing
			std::vector<float> oversampled = Oversample4x(samples);

			// Apply sample rate reduction at higher sample rate
			// This simulates a lower sample rate by holding values.
			// The 'factor' applies to the original sample rate.
			// In the 4x oversampled domain, we need to hold for 'factor * 4' samples.
			float current_sample_value = 0.0f;
			int hold_counter = 0; // Counts down from (factor * 4)
			for (size_t i = 0; i < oversampled.size(); ++i) {
				if (hold_counter == 0) {
					current_sample_value = oversampled[i];
					hold_counter = factor * 4; // Reset hold counter
				}
				oversampled[i] = current_sample_value;
				hold_counter--;
			}

			// Downsample with anti-aliasing filter
			samples = Downsample4x(oversampled);
		}

		// === DISTORTION IMPLEMENTATIONS ===

		void WaveformEffects::ApplySoftDistortion(std::vector<float>& samples, float amount) {
			float drive = 1.0f + amount * 9.0f; // 1x to 10x drive
			for (float& s : samples) {
				s = std::tanh(s * drive);
			}
		}

		void WaveformEffects::ApplyHardDistortion(std::vector<float>& samples, float amount) {
			float threshold = 1.0f - amount * 0.9f; // Threshold from 1.0 to 0.1
			for (float& s : samples) {
				if (s > threshold) s = threshold;
				else if (s < -threshold) s = -threshold;
			}
		}

		void WaveformEffects::ApplyAsymmetricDistortion(std::vector<float>& samples, float amount) {
			float drive = 1.0f + amount * 4.0f;
			for (float& s : samples) {
				if (s > 0.0f) {
					s = std::tanh(s * drive);
				}
				else {
					s = std::tanh(s * drive * 0.5f); // Asymmetric - less drive on negative
				}
			}
		}

		// === OVERSAMPLING INFRASTRUCTURE ===

		std::vector<float> WaveformEffects::Oversample4x(const std::vector<float>& input) {
			size_t inputSize = input.size();
			size_t outputSize = inputSize * 4;
			std::vector<float> output(outputSize, 0.0f);

			// Insert zeros between samples
			for (size_t i = 0; i < inputSize; ++i) {
				output[i * 4] = input[i] * 4.0f; // Compensate for energy loss
			}

			// Apply interpolation filter (simple windowed-sinc approximation)
			// Using a simple 16-tap FIR filter for speed
			const int filterTaps = 16;
			std::vector<float> filtered(outputSize);

			for (size_t i = 0; i < outputSize; ++i) {
				float sum = 0.0f;
				for (int j = -filterTaps / 2; j < filterTaps / 2; ++j) {
					int idx = (int)i + j;
					if (idx >= 0 && idx < (int)outputSize) {
						// Lanczos kernel
						float x = (float)j / 4.0f;
						float sinc = (x == 0.0f) ? 1.0f : (float)(std::sin(PI * x) / (PI * x));
						float window = (x == 0.0f) ? 1.0f : (float)(std::sin(PI * x / 4.0f) / (PI * x / 4.0f));
						sum += output[idx] * sinc * window;
					}
				}
				filtered[i] = sum;
			}

			return filtered;
		}

		std::vector<float> WaveformEffects::Downsample4x(const std::vector<float>& input) {
			// First apply anti-aliasing filter
			std::vector<float> filtered = input;
			ApplyAntiAliasingFilter(filtered);

			// Then decimate (keep every 4th sample)
			size_t outputSize = input.size() / 4;
			std::vector<float> output(outputSize);

			for (size_t i = 0; i < outputSize; ++i) {
				output[i] = filtered[i * 4];
			}

			return output;
		}

		void WaveformEffects::ApplyAntiAliasingFilter(std::vector<float>& samples) {
			// Low-pass filter at 1/4 Nyquist (for 4x downsampling)
			// Simple multi-pass moving average approximates sinc filter
			const int passes = 4;
			const int windowSize = 8;

			for (int pass = 0; pass < passes; ++pass) {
				std::vector<float> temp = samples;
				for (size_t i = windowSize; i < samples.size() - windowSize; ++i) {
					float sum = 0.0f;
					for (int j = -windowSize / 2; j < windowSize / 2; ++j) {
						sum += temp[i + j];
					}
					samples[i] = sum / (float)windowSize;
				}
			}
		}

		// === MORPH CURVE FUNCTIONS ===

		float WaveformEffects::ApplyMorphCurve(float t, MorphCurve curve) {
			// t is 0.0 to 1.0 (frame position in morph)
			// Returns adjusted t value based on curve type

			switch (curve) {
			case MorphCurve::Linear:
				return t;

			case MorphCurve::Exponential:
				// Slow start, fast end
				return t * t;

			case MorphCurve::Logarithmic:
				// Fast start, slow end
				return std::sqrt(t);

			case MorphCurve::SCurve:
				// Smooth ease in/out (cubic hermite)
				return t * t * (3.0f - 2.0f * t);

			default:
				return t;
			}
		}

		// === SPECTRAL EFFECTS ===

		void WaveformEffects::ApplySpectralDecay(std::vector<float>& samples, float amount, float curve) {
			// Lazy-initialized singleton for FFT processor (matches existing pattern)
			static std::shared_ptr<DSP::IFrequencyProcessor> fftProcessor =
				std::make_shared<DSP::KissFFTProcessor>(2048);
			static Core::SpectralEffects spectralFX(fftProcessor);

			spectralFX.ApplySpectralDecay(samples, amount, curve);
		}

		void WaveformEffects::ApplySpectralTilt(std::vector<float>& samples, float amount) {
			static std::shared_ptr<DSP::IFrequencyProcessor> fftProcessor =
				std::make_shared<DSP::KissFFTProcessor>(2048);
			static Core::SpectralEffects spectralFX(fftProcessor);

			spectralFX.ApplySpectralTilt(samples, amount);
		}

		void WaveformEffects::ApplySpectralGate(std::vector<float>& samples, float threshold) {
			static std::shared_ptr<DSP::IFrequencyProcessor> fftProcessor =
				std::make_shared<DSP::KissFFTProcessor>(2048);
			static Core::SpectralEffects spectralFX(fftProcessor);

			spectralFX.ApplySpectralGate(samples, threshold);
		}

		void WaveformEffects::ApplyPhaseRandomization(std::vector<float>& samples, float amount) {
			static std::shared_ptr<DSP::IFrequencyProcessor> fftProcessor =
				std::make_shared<DSP::KissFFTProcessor>(2048);
			static Core::SpectralEffects spectralFX(fftProcessor);

			spectralFX.ApplyPhaseRandomization(samples, amount);
		}

		void WaveformEffects::ApplySpectralShift(std::vector<float>& samples, int shiftAmount) {
			static std::shared_ptr<DSP::IFrequencyProcessor> fftProcessor =
				std::make_shared<DSP::KissFFTProcessor>(2048);
			static Core::SpectralEffects spectralFX(fftProcessor);

			spectralFX.ApplySpectralShift(samples, shiftAmount);
		}

	} // namespace Core
} // namespace WavetableGen