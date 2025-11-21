#ifndef IFILEWRITER_H
#define IFILEWRITER_H

#include <string>
#include <vector>
#include "../Core/WaveGenerator.h"

namespace WavetableGen {
	namespace IO {
		// Interface for file writers (Strategy Pattern)
		class IFileWriter {
		public:
			virtual ~IFileWriter() = default;

			// Write wavetable data to file
			virtual Core::GenerationResult Write(
				const std::string& filename,
				const std::vector<float>& samples,
				int numFrames,
				uint32_t sampleRate = 44100) = 0;
		};
	}
}

#endif // IFILEWRITER_H
