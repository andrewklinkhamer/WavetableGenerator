#include "WTFileWriter.h"
#include "../Core/WaveGenerator.h"  // For SAMPLES_PER_WAVE constant
#include <cstring>

namespace WavetableGen {
	namespace IO {
		using namespace Core;

		// Portable helper: Write 32-bit unsigned integer in little-endian format
		void WTFileWriter::WriteUInt32(std::ofstream& file, uint32_t value) {
			unsigned char bytes[4];
			bytes[0] = value & 0xFF;
			bytes[1] = (value >> 8) & 0xFF;
			bytes[2] = (value >> 16) & 0xFF;
			bytes[3] = (value >> 24) & 0xFF;
			file.write(reinterpret_cast<const char*>(bytes), 4);
		}

		// Portable helper: Write 32-bit float in little-endian format
		void WTFileWriter::WriteFloat32(std::ofstream& file, float value) {
			// Reinterpret float as uint32_t to get raw bytes
			uint32_t rawValue;
			std::memcpy(&rawValue, &value, sizeof(float));
			WriteUInt32(file, rawValue);
		}

		// Write wavetable in .wt format (Serum/Bitwig format, portable - no struct packing required)
		GenerationResult WTFileWriter::Write(
			const std::string& filename,
			const std::vector<float>& samples,
			int numFrames,
			uint32_t sampleRate) {
			// Validate: samples must be exact multiple of SAMPLES_PER_WAVE
			size_t expectedSamples = numFrames * SAMPLES_PER_WAVE;
			if (samples.size() != expectedSamples) {
				return GenerationResult::ErrorInvalidSampleCount;
			}

			// Validate: check if samples contain valid data
			int zeroCount = 0;
			for (float s : samples) {
				if (s == 0.0f) zeroCount++;
			}

			if (zeroCount == samples.size()) {
				return GenerationResult::ErrorAllSamplesZero;
			}

			std::ofstream file(filename, std::ios::binary);
			if (!file) {
				return GenerationResult::ErrorFileOpenFailed;
			}

			// Write .wt header (12 bytes total)
			// Magic number: "vawt" (4 bytes)
			file.write("vawt", 4);

			// Samples per frame: 2048 (4 bytes, uint32_t)
			WriteUInt32(file, SAMPLES_PER_WAVE);

			// Number of frames (4 bytes, uint32_t)
			WriteUInt32(file, static_cast<uint32_t>(numFrames));

			// Write 32-bit float samples
			for (float s : samples) {
				if (s > 1.0f) s = 1.0f;
				if (s < -1.0f) s = -1.0f;
				WriteFloat32(file, s);
			}

			return GenerationResult::Success;
		}
	}
}