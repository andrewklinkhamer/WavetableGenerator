#ifndef FILEWRITERFACTORY_H
#define FILEWRITERFACTORY_H

#include "IFileWriter.h"
#include "../Core/WaveGenerator.h"  // For OutputFormat
#include <memory>

namespace WavetableGen {
	namespace IO {
		// Factory to create appropriate file writer based on format
		class FileWriterFactory {
		public:
			static std::unique_ptr<IFileWriter> Create(Core::OutputFormat format);
		};
	}
}

#endif // FILEWRITERFACTORY_H
