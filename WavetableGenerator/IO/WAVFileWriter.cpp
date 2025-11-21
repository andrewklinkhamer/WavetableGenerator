#include "WAVFileWriter.h"

namespace WavetableGen {
	namespace IO {
		using namespace Core;

		// Portable helper: Write 16-bit unsigned integer in little-endian format
		void WAVFileWriter::WriteUInt16(std::ofstream& file, uint16_t value) {
			unsigned char bytes[2];
			bytes[0] = value & 0xFF;
			bytes[1] = (value >> 8) & 0xFF;
			file.write(reinterpret_cast<const char*>(bytes), 2);
		}

		// Portable helper: Write 32-bit unsigned integer in little-endian format
		void WAVFileWriter::WriteUInt32(std::ofstream& file, uint32_t value) {
			unsigned char bytes[4];
			bytes[0] = value & 0xFF;
			bytes[1] = (value >> 8) & 0xFF;
			bytes[2] = (value >> 16) & 0xFF;
			bytes[3] = (value >> 24) & 0xFF;
			file.write(reinterpret_cast<const char*>(bytes), 4);
		}

		// Write WAV file (portable - no struct packing required)
		GenerationResult WAVFileWriter::Write(
			const std::string& filename,
			const std::vector<float>& samples,
			int numFrames,
			uint32_t sampleRate) {
			std::ofstream file(filename, std::ios::binary);
			if (!file) {
				return GenerationResult::ErrorFileOpenFailed;
			}

			// Calculate header values
			const uint16_t bitsPerSample = 16;
			const uint16_t numChannels = 1;
			const uint16_t blockAlign = numChannels * bitsPerSample / 8;
			const uint32_t byteRate = sampleRate * blockAlign;
			const uint32_t subchunk2Size = static_cast<uint32_t>(samples.size() * blockAlign);
			const uint32_t chunkSize = 36 + subchunk2Size;

			// Write RIFF header
			file.write("RIFF", 4);
			WriteUInt32(file, chunkSize);
			file.write("WAVE", 4);

			// Write fmt subchunk
			file.write("fmt ", 4);
			WriteUInt32(file, 16);                  // Subchunk1Size (16 for PCM)
			WriteUInt16(file, 1);                   // AudioFormat (1 = PCM)
			WriteUInt16(file, numChannels);         // NumChannels
			WriteUInt32(file, sampleRate);          // SampleRate
			WriteUInt32(file, byteRate);            // ByteRate
			WriteUInt16(file, blockAlign);          // BlockAlign
			WriteUInt16(file, bitsPerSample);       // BitsPerSample

			// Write data subchunk
			file.write("data", 4);
			WriteUInt32(file, subchunk2Size);

			// Write sample data (convert float to 16-bit PCM)
			for (float s : samples) {
				if (s > 1.0f) s = 1.0f;
				if (s < -1.0f) s = -1.0f;
				int16_t sampleInt = static_cast<int16_t>(s * 32767);
				WriteUInt16(file, static_cast<uint16_t>(sampleInt));
			}

			return GenerationResult::Success;
		}
	}
}