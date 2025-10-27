#include <algorithm>
#include <io.h>
#include "RandomWavetableGenerator.h"
#include "WaveGenerator.h"

namespace WavetableGen {
	namespace Services {
		using namespace Core;
		using namespace Utils;

		RandomWavetableGenerator::RandomWavetableGenerator(IWavetableGenerator& wavetableGenerator, XorShift128Plus& rng)
			: m_wavetableGenerator(wavetableGenerator), m_rng(rng) {
		}

		// Generate a random selection of waveforms with random weights (from UI-specified available waveforms)
		std::vector<std::pair<WaveType, float>> RandomWavetableGenerator::GenerateRandomWaveSelection(
			int minWaves,
			int maxWaves,
			const std::vector<AvailableWaveform>& availableWaveforms) {
			std::vector<std::pair<WaveType, float>> selection;

			if (availableWaveforms.empty()) {
				return selection;
			}

			// Randomly decide how many waveforms to include (between min and max)
			// But don't exceed the number of available waveforms
			int numAvailable = (int)availableWaveforms.size();
			int actualMax = (std::min)(maxWaves, numAvailable);
			int actualMin = (std::min)(minWaves, actualMax);
			int numWaves = m_rng.NextInt(actualMin, actualMax);

			// Randomly select waveforms (without duplicates)
			std::vector<bool> used(numAvailable, false);
			for (int i = 0; i < numWaves; ++i) {
				int idx;
				do {
					idx = m_rng.NextInt(0, numAvailable - 1);
				} while (used[idx]);
				used[idx] = true;

				const AvailableWaveform& available = availableWaveforms[idx];

				// Random weight within the slider range for this waveform
				float weight = m_rng.NextFloat(available.minWeight, available.maxWeight);
				selection.push_back({ available.type, weight });
			}

			return selection;
		}

		// Generate multiple random wavetables
		void RandomWavetableGenerator::GenerateBatch(
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
			int maxHarmonics,
			std::function<bool(int, int)> progressCallback) {
			// If no waveforms are available, cannot generate
			if (availableWaveforms.empty()) {
				return;
			}

			// Random frame counts for morphing
			int frameOptions[] = { 64, 128, 256, 512 };

			int generatedCount = 0;
			int maxAttempts = count * 1000; // Safety limit to prevent infinite loops
			int attempts = 0;

			while (generatedCount < count && attempts < maxAttempts) {
				attempts++;

				// Random morphing enabled/disabled per wavetable (70% chance of morphing)
				bool enableMorphing = m_rng.NextBool(0.7f);

				// Generate random start waveform selection
				auto startWaves = GenerateRandomWaveSelection(minWaves, maxWaves, availableWaveforms);

				// Generate random end waveform selection
				auto endWaves = GenerateRandomWaveSelection(minWaves, maxWaves, availableWaveforms);

				// Random number of frames
				int numFrames = frameOptions[m_rng.NextInt(0, 3)];

				// Generate filename from start and end wave settings (include effects)
				std::string baseFilename = m_wavetableGenerator.GenerateFilenameFromSettings(startWaves, endWaves, enableMorphing, effects, morphCurve, pulseDuty);
				std::string fullPath = outputFolder + baseFilename + extension;

				// Check if file already exists (_access returns 0 if file exists)
				if (_access(fullPath.c_str(), 0) == 0) {
					// File exists, try another random combination
					continue;
				}

				// File doesn't exist, generate it
				GenerationResult result = m_wavetableGenerator.GenerateWavetable(startWaves, endWaves, fullPath, format, isAudioPreview, enableMorphing, numFrames, effects, morphCurve, pulseDuty, maxHarmonics);

				// Check result
				if (result == GenerationResult::Success) {
					generatedCount++;

					// Call progress callback if provided
					if (progressCallback) {
						bool shouldContinue = progressCallback(generatedCount, count);
						if (!shouldContinue) {
							// User cancelled generation
							break;
						}
					}
				}
				else if (result == GenerationResult::ErrorFileOpenFailed) {
					// Fatal error - can't write to output folder, stop immediately
					break;
				}
				// For other errors (empty waveforms, invalid samples, etc.), continue trying other combinations
			}
		}
	}
}