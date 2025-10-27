#ifndef RANDOMWAVETABLEGENERATOR_H
#define RANDOMWAVETABLEGENERATOR_H

#include <vector>
#include <string>
#include <functional>
#include "IWavetableGenerator.h"
#include "../Utils/XorShift128Plus.h"

namespace WavetableGen {
	namespace Services {
		using namespace Core;
		using namespace Utils;

		// Handles random wavetable generation logic (extracted from WinApplication for SRP)
		class RandomWavetableGenerator {
		public:
			// Structure to define available waveforms with weight range
			struct AvailableWaveform {
				WaveType type;
				float minWeight;  // From start slider (0.0-1.0)
				float maxWeight;  // From end slider (0.0-1.0)
			};

			explicit RandomWavetableGenerator(IWavetableGenerator& wavetableGenerator, XorShift128Plus& rng);
			~RandomWavetableGenerator() = default;

			// Generate multiple random wavetables
			void GenerateBatch(
				const std::string& outputFolder,
				int count,
				int minWaves,
				int maxWaves,
				const std::vector<AvailableWaveform>& availableWaveforms,
				const char* extension,
				OutputFormat format,
				bool isAudioPreview,
				const EffectsSettings& effects,
				MorphCurve morphCurve,
				double pulseDuty,
				int maxHarmonics = 8,
				std::function<bool(int, int)> progressCallback = nullptr);

		private:
			std::vector<std::pair<WaveType, float>> GenerateRandomWaveSelection(
				int minWaves,
				int maxWaves,
				const std::vector<AvailableWaveform>& availableWaveforms);

			IWavetableGenerator& m_wavetableGenerator;
			XorShift128Plus& m_rng;
		};
	}
}
#endif // RANDOMWAVETABLEGENERATOR_H
