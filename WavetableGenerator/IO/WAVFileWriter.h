#ifndef WAVFILEWRITER_H
#define WAVFILEWRITER_H

#include "IFileWriter.h"
#include <fstream>

namespace WavetableGen {
	namespace IO {
		// Writes audio files in WAV format
		class WAVFileWriter : public IFileWriter {
		public:
			WAVFileWriter() = default;
			~WAVFileWriter() override = default;

			Core::GenerationResult Write(
				const std::string& filename,
				const std::vector<float>& samples,
				int numFrames,
				uint32_t sampleRate = 44100) override;

		private:
			// Helper methods to write binary data in little-endian format (portable)
			static void WriteUInt16(std::ofstream& file, uint16_t value);
			static void WriteUInt32(std::ofstream& file, uint32_t value);
		};
	}
}

#endif // WAVFILEWRITER_H
