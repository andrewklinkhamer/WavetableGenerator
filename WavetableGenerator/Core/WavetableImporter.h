#ifndef WAVETABLEIMPORTER_H
#define WAVETABLEIMPORTER_H

#include <vector>
#include <string>
#include <cstdint>

namespace WavetableGen {
	namespace IO {
		// Structure to hold imported wavetable data
		struct ImportedWavetable {
			std::vector<float> samples;     // All samples (frames * samplesPerFrame)
			int numFrames;                   // Number of frames in the wavetable
			int samplesPerFrame;             // Samples per frame (usually 2048)
			uint32_t sampleRate;             // Sample rate (for info display)
			std::string filename;            // Original filename

			// Helper to get a specific frame
			std::vector<float> GetFrame(int frameIndex) const {
				if (frameIndex < 0 || frameIndex >= numFrames) {
					return std::vector<float>();
				}

				size_t startIdx = frameIndex * samplesPerFrame;
				size_t endIdx = startIdx + samplesPerFrame;

				if (endIdx > samples.size()) {
					return std::vector<float>();
				}

				return std::vector<float>(samples.begin() + startIdx, samples.begin() + endIdx);
			}

			// Check if valid
			bool IsValid() const {
				return !samples.empty() && numFrames > 0 && samplesPerFrame > 0;
			}
		};

		// Result codes for import operations
		enum class ImportResult {
			Success,
			ErrorFileNotFound,
			ErrorInvalidFormat,
			ErrorUnsupportedFormat,
			ErrorReadFailed,
			ErrorInvalidSampleCount
		};

		// Wavetable importer - reads .wt and .wav files
		class WavetableImporter {
		public:
			WavetableImporter() = default;
			~WavetableImporter() = default;

			// Import a wavetable from file (auto-detects format from extension)
			ImportResult Import(const std::string& filename, ImportedWavetable& outWavetable);

			// Import specific formats
			ImportResult ImportWT(const std::string& filename, ImportedWavetable& outWavetable);
			ImportResult ImportWAV(const std::string& filename, ImportedWavetable& outWavetable);

			// Get human-readable error message
			static const char* GetErrorMessage(ImportResult result);

		private:
			// Helper methods for reading binary data (little-endian)
			static bool ReadUInt16(std::ifstream& file, uint16_t& value);
			static bool ReadUInt32(std::ifstream& file, uint32_t& value);
			static bool ReadFloat32(std::ifstream& file, float& value);
		};
	}
}

#endif // WAVETABLEIMPORTER_H
