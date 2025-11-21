#include "FileWriterFactory.h"
#include "WTFileWriter.h"
#include "WAVFileWriter.h"

namespace WavetableGen {
	namespace IO {
		using namespace Core;

		std::unique_ptr<IFileWriter> FileWriterFactory::Create(OutputFormat format) {
			switch (format) {
			case OutputFormat::WT:
				return std::make_unique<WTFileWriter>();
			case OutputFormat::WAV:
				return std::make_unique<WAVFileWriter>();
			default:
				return std::make_unique<WTFileWriter>();  // Default to WT format
			}
		}
	}
}