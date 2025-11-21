#ifndef IWAVETABLEGENERATOR_H
#define IWAVETABLEGENERATOR_H

#include <vector>
#include <string>
#include <utility>
#include "WaveType.h"
#include "../DSP/WaveformEffects.h"

namespace WavetableGen {
	namespace Core {
		// Forward declarations for types used in the interface
		enum class GenerationResult;
		enum class OutputFormat;
		enum class MorphCurve;
		struct EffectsSettings;

		// Interface for wavetable generation service (Dependency Inversion Principle)
		class IWavetableGenerator {
		public:
			virtual ~IWavetableGenerator() = default;

			// Generate a wavetable with the specified parameters
			virtual GenerationResult GenerateWavetable(
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
				int maxHarmonics = 8) = 0;

			// Generate filename from waveform settings
			virtual std::string GenerateFilenameFromSettings(
				const std::vector<std::pair<WaveType, float>>& startWaves,
				const std::vector<std::pair<WaveType, float>>& endWaves,
				bool enableMorphing,
				const EffectsSettings& effects = EffectsSettings(),
				MorphCurve morphCurve = MorphCurve::Linear,
				double pulseDuty = 0.5) = 0;

			// Analyze an imported frame and find best matching waveforms (time-domain correlation)
			virtual std::vector<std::pair<WaveType, float>> AnalyzeFrame(const std::vector<float>& frameData) = 0;

			// Analyze an imported frame using spectral matching (frequency-domain)
			virtual std::vector<std::pair<WaveType, float>> AnalyzeFrameSpectral(const std::vector<float>& frameData) = 0;
		};
	}
}

#endif // IWAVETABLEGENERATOR_H
