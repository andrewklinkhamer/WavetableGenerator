#ifndef WAVEFORMEFFECTS_H
#define WAVEFORMEFFECTS_H

#include <vector>
#include <cmath>

namespace WavetableGen {
	namespace Core {
		// Effect types
		enum class DistortionType {
			None,
			Soft,      // Soft saturation (tanh)
			Hard,      // Hard clipping
			Asymmetric // Asymmetric distortion
		};

		enum class MorphCurve {
			Linear,
			Exponential,
			Logarithmic,
			SCurve
		};

		// Settings structure for effects pipeline
		struct EffectsSettings {
			// Distortion
			DistortionType distortionType = DistortionType::None;
			float distortionAmount = 0.0f; // 0.0-1.0

			// Filtering
			bool enableLowPass = false;
			float lowPassCutoff = 1.0f; // 0.0-1.0 (fraction of Nyquist)
			bool enableHighPass = false;
			float highPassCutoff = 0.0f; // 0.0-1.0

			// Bit crushing
			bool enableBitCrush = false;
			int bitDepth = 16; // 1-16 bits

			// Symmetry operations
			bool mirrorHorizontal = false;
			bool mirrorVertical = false;
			bool invert = false;
			bool reverse = false;

			// Wavefold
			bool enableWavefold = false;
			float wavefoldAmount = 0.0f; // 0.0-1.0

			// Spectral decay
			bool enableSpectralDecay = false;
			float spectralDecayAmount = 0.0f; // 0.0-1.0
			float spectralDecayCurve = 1.0f;  // 1.0 = linear, >1.0 = exponential

			// Spectral tilt
			bool enableSpectralTilt = false;
			float spectralTiltAmount = 0.0f; // -1.0 (bass cut) to 1.0 (treble cut)

			// Spectral gate
			bool enableSpectralGate = false;
			float spectralGateThreshold = 0.0f; // 0.0-1.0 (fraction of max magnitude)

			// Phase randomization
			bool enablePhaseRandomize = false;
			float phaseRandomizeAmount = 0.0f; // 0.0-1.0

			// Sample Rate Reduction
			bool enableSampleRateReduction = false;
			int sampleRateReductionFactor = 1; // 1 (no effect) to 32

			// Spectral Shift
			bool enableSpectralShift = false;
			int spectralShiftAmount = 0; // -100 to +100 bins
		};

		// Waveform effects processor with anti-aliasing
		class WaveformEffects {
		public:
			// Apply all effects in proper order to avoid aliasing
			static void ApplyEffects(std::vector<float>& samples, const EffectsSettings& settings);

			// Individual effects (public for flexibility)

			// Safe effects (no oversampling needed)
			static void ApplyLowPassFilter(std::vector<float>& samples, float cutoff);
			static void ApplyHighPassFilter(std::vector<float>& samples, float cutoff);
			static void ApplyMirrorHorizontal(std::vector<float>& samples);
			static void ApplyMirrorVertical(std::vector<float>& samples);
			static void ApplyInvert(std::vector<float>& samples);
			static void ApplyReverse(std::vector<float>& samples);

			// Aliasing-prone effects (use oversampling internally)
			static void ApplyDistortion(std::vector<float>& samples, DistortionType type, float amount);
			static void ApplyBitCrush(std::vector<float>& samples, int bits);
			static void ApplySampleRateReduction(std::vector<float>& samples, int factor);
			static void ApplyWavefold(std::vector<float>& samples, float amount);

			// Spectral effects (frequency domain processing)
			static void ApplySpectralDecay(std::vector<float>& samples, float amount, float curve);
			static void ApplySpectralTilt(std::vector<float>& samples, float amount);
			static void ApplySpectralGate(std::vector<float>& samples, float threshold);
			static void ApplyPhaseRandomization(std::vector<float>& samples, float amount);
			static void ApplySpectralShift(std::vector<float>& samples, int shiftAmount);

			// Morph curve interpolation
			static float ApplyMorphCurve(float t, MorphCurve curve);

		private:
			// Oversampling infrastructure
			static std::vector<float> Oversample4x(const std::vector<float>& input);
			static std::vector<float> Downsample4x(const std::vector<float>& input);

			// Anti-aliasing filter for downsampling
			static void ApplyAntiAliasingFilter(std::vector<float>& samples);

			// Low-level distortion implementations (called at oversampled rate)
			static void ApplySoftDistortion(std::vector<float>& samples, float amount);
			static void ApplyHardDistortion(std::vector<float>& samples, float amount);
			static void ApplyAsymmetricDistortion(std::vector<float>& samples, float amount);
		};
	}
}

#endif // WAVEFORMEFFECTS_H
