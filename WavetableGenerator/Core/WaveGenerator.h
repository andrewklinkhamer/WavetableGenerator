#ifndef WAVEGENERATOR_H
#define WAVEGENERATOR_H

#include <vector>
#include <string>
#include <cstdint>
#include "IWavetableGenerator.h"
#include "WaveType.h"
#include "../DSP/WaveformEffects.h"

namespace WavetableGen {
	namespace Core {
		constexpr double PI = 3.14159265358979323846;
		constexpr int SAMPLE_RATE = 44100;
		constexpr int SAMPLES_PER_WAVE = 2048;

		// Output format enum
		enum class OutputFormat {
			WT,          // .wt format (Serum/Bitwig wavetable)
			WAV          // .wav format (standard audio file)
		};

		// Result codes for wavetable generation operations
		enum class GenerationResult {
			Success,
			ErrorEmptyWaveforms,
			ErrorFileOpenFailed,
			ErrorInvalidSampleCount,
			ErrorAllSamplesZero
		};

		// Structure to define a wavetable frame configuration
		struct WavetableFrame {
			std::vector<std::pair<WaveType, float>> waveforms; // Waveform types and their weights
		};

		class WaveGenerator : public IWavetableGenerator {
		public:
			WaveGenerator() = default;
			~WaveGenerator() = default;

			// Main public API: Generate a wavetable with the specified parameters
			GenerationResult GenerateWavetable(
				const std::vector<std::pair<WaveType, float>>& startWaves,
				const std::vector<std::pair<WaveType, float>>& endWaves,
				const std::string& filename,
				OutputFormat format,
				bool isAudioPreview,
				bool enableMorphing,
				int numFrames,
				const EffectsSettings& effects = EffectsSettings(),
				MorphCurve morphCurve = MorphCurve::Linear,
				double pulseDuty = 0.5,
				int maxHarmonics = 8);

			// Generate filename from waveform settings (used by WinApplication)
			std::string GenerateFilenameFromSettings(
				const std::vector<std::pair<WaveType, float>>& startWaves,
				const std::vector<std::pair<WaveType, float>>& endWaves,
				bool enableMorphing,
				const EffectsSettings& effects = EffectsSettings(),
				MorphCurve morphCurve = MorphCurve::Linear,
				double pulseDuty = 0.5) override;

			// Analyze an imported frame and find best matching waveforms (time-domain)
			std::vector<std::pair<WaveType, float>> AnalyzeFrame(const std::vector<float>& frameData) override;

			// Analyze an imported frame using spectral matching (frequency-domain)
			std::vector<std::pair<WaveType, float>> AnalyzeFrameSpectral(const std::vector<float>& frameData) override;

		private:
			// Generate a single waveform cycle
			std::vector<float> GenerateWave(WaveType type, size_t numSamples, double pulseDuty = 0.5, int maxHarmonics = 8);

			// PolyBLEP (Polynomial Band-Limited Step) for anti-aliasing
			static float PolyBLEP(float t, float dt);

			// Combine multiple waves with weights
			std::vector<float> CombineWaves(const std::vector<std::pair<WaveType, float>>& waves, size_t numSamples);

			// Generate a multi-frame wavetable with morphing
			std::vector<float> GenerateMultiFrameWavetable(const WavetableFrame& startFrame, const WavetableFrame& endFrame,
				int numFrames, MorphCurve morphCurve = MorphCurve::Linear);

			// Helper methods for GenerateWavetable
			std::vector<float> GenerateAudioPreview(const std::vector<std::pair<WaveType, float>>& startWaves,
				const EffectsSettings& effects);

			std::vector<float> GenerateMorphingWavetable(const std::vector<std::pair<WaveType, float>>& startWaves,
				const std::vector<std::pair<WaveType, float>>& endWaves, int numFrames, const EffectsSettings& effects,
				MorphCurve morphCurve);

			std::vector<float> GenerateSingleFrameWavetable(const std::vector<std::pair<WaveType, float>>& startWaves,
				const EffectsSettings& effects);

			void NormalizeSamples(std::vector<float>& samples);

			WavetableFrame CreateEndFrame(const std::vector<std::pair<WaveType, float>>& startWaves,
				const std::vector<std::pair<WaveType, float>>& endWaves);

			// Store pulse duty cycle for PWM
			double m_pulseDuty = 0.5;

			// Store max harmonics for harmonic waveforms
			int m_maxHarmonics = 8;
		};
	} // namespace Core
} // namespace WavetableGen

#endif // WAVEGENERATOR_H
