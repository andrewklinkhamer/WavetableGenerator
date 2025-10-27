#ifndef WTFILEWRITER_H
#define WTFILEWRITER_H

#include "IFileWriter.h"
#include <fstream>

namespace WavetableGen {
	namespace IO {
		// Writes wavetables in .wt format (Serum/Bitwig format)
		class WTFileWriter : public IFileWriter {
		public:
			WTFileWriter() = default;
			~WTFileWriter() override = default;

			Core::GenerationResult Write(
				const std::string& filename,
				const std::vector<float>& samples,
				int numFrames,
				uint32_t sampleRate = 44100) override;

		private:
			// Helper methods to write binary data in little-endian format (portable)
			static void WriteUInt32(std::ofstream& file, uint32_t value);
			static void WriteFloat32(std::ofstream& file, float value);
		};
	}
}

#endif // WTFILEWRITER_H
