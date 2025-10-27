#include "WavetableImporter.h"
#include <fstream>
#include <cstring>
#include <algorithm>

namespace WavetableGen {
	namespace IO {
		// Helper: Read 16-bit unsigned integer (little-endian)
		bool WavetableImporter::ReadUInt16(std::ifstream& file, uint16_t& value) {
			unsigned char bytes[2];
			if (!file.read(reinterpret_cast<char*>(bytes), 2)) {
				return false;
			}
			value = bytes[0] | (bytes[1] << 8);
			return true;
		}

		// Helper: Read 32-bit unsigned integer (little-endian)
		bool WavetableImporter::ReadUInt32(std::ifstream& file, uint32_t& value) {
			unsigned char bytes[4];
			if (!file.read(reinterpret_cast<char*>(bytes), 4)) {
				return false;
			}
			value = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
			return true;
		}

		// Helper: Read 32-bit float (little-endian)
		bool WavetableImporter::ReadFloat32(std::ifstream& file, float& value) {
			uint32_t rawValue;
			if (!ReadUInt32(file, rawValue)) {
				return false;
			}
			std::memcpy(&value, &rawValue, sizeof(float));
			return true;
		}

		// Import wavetable (auto-detect format)
		ImportResult WavetableImporter::Import(const std::string& filename, ImportedWavetable& outWavetable) {
			// Detect format from extension
			size_t dotPos = filename.find_last_of('.');
			if (dotPos == std::string::npos) {
				return ImportResult::ErrorInvalidFormat;
			}

			std::string extension = filename.substr(dotPos + 1);
			std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

			if (extension == "wt") {
				return ImportWT(filename, outWavetable);
			}
			else if (extension == "wav") {
				return ImportWAV(filename, outWavetable);
			}
			return ImportResult::ErrorUnsupportedFormat;
		}

		// Import .wt format (Serum/Bitwig wavetable)
		ImportResult WavetableImporter::ImportWT(const std::string& filename, ImportedWavetable& outWavetable) {
			std::ifstream file(filename, std::ios::binary);
			if (!file) {
				return ImportResult::ErrorFileNotFound;
			}

			// Read magic number "vawt" (4 bytes)
			char magic[5] = { 0 };
			if (!file.read(magic, 4)) {
				return ImportResult::ErrorReadFailed;
			}

			if (std::strncmp(magic, "vawt", 4) != 0) {
				return ImportResult::ErrorInvalidFormat;
			}

			// Read samples per frame (4 bytes, uint32_t)
			uint32_t samplesPerFrame;
			if (!ReadUInt32(file, samplesPerFrame)) {
				return ImportResult::ErrorReadFailed;
			}

			// Validate samples per frame is a power of 2
			if (samplesPerFrame == 0 || (samplesPerFrame & (samplesPerFrame - 1)) != 0) {
				return ImportResult::ErrorInvalidSampleCount;
			}

			// Read number of frames (4 bytes, uint32_t)
			uint32_t numFrames;
			if (!ReadUInt32(file, numFrames)) {
				return ImportResult::ErrorReadFailed;
			}

			if (numFrames == 0 || numFrames > 16384) { // Sanity check
				return ImportResult::ErrorInvalidFormat;
			}

			// Read all samples (32-bit floats)
			size_t totalSamples = samplesPerFrame * numFrames;
			std::vector<float> samples(totalSamples);

			for (size_t i = 0; i < totalSamples; ++i) {
				if (!ReadFloat32(file, samples[i])) {
					return ImportResult::ErrorReadFailed;
				}
			}

			// Fill output structure
			outWavetable.samples = std::move(samples);
			outWavetable.numFrames = static_cast<int>(numFrames);
			outWavetable.samplesPerFrame = static_cast<int>(samplesPerFrame);
			outWavetable.sampleRate = 44100; // .wt format doesn't store sample rate
			outWavetable.filename = filename;

			return ImportResult::Success;
		}

		// Import .wav format
		ImportResult WavetableImporter::ImportWAV(const std::string& filename, ImportedWavetable& outWavetable) {
			std::ifstream file(filename, std::ios::binary);
			if (!file) {
				return ImportResult::ErrorFileNotFound;
			}

			// Read RIFF header
			char riffMagic[5] = { 0 };
			if (!file.read(riffMagic, 4)) {
				return ImportResult::ErrorReadFailed;
			}

			if (std::strncmp(riffMagic, "RIFF", 4) != 0) {
				return ImportResult::ErrorInvalidFormat;
			}

			// Skip chunk size (4 bytes)
			uint32_t chunkSize;
			if (!ReadUInt32(file, chunkSize)) {
				return ImportResult::ErrorReadFailed;
			}

			// Read WAVE magic
			char waveMagic[5] = { 0 };
			if (!file.read(waveMagic, 4)) {
				return ImportResult::ErrorReadFailed;
			}

			if (std::strncmp(waveMagic, "WAVE", 4) != 0) {
				return ImportResult::ErrorInvalidFormat;
			}

			// Find fmt chunk
			uint16_t audioFormat = 0;
			uint16_t numChannels = 0;
			uint32_t sampleRate = 0;
			uint16_t bitsPerSample = 0;

			while (file) {
				char chunkID[5] = { 0 };
				if (!file.read(chunkID, 4)) {
					break;
				}

				uint32_t chunkDataSize;
				if (!ReadUInt32(file, chunkDataSize)) {
					return ImportResult::ErrorReadFailed;
				}

				if (std::strncmp(chunkID, "fmt ", 4) == 0) {
					// Read format data
					if (!ReadUInt16(file, audioFormat)) return ImportResult::ErrorReadFailed;
					if (!ReadUInt16(file, numChannels)) return ImportResult::ErrorReadFailed;
					if (!ReadUInt32(file, sampleRate)) return ImportResult::ErrorReadFailed;

					// Skip byte rate (4 bytes) and block align (2 bytes)
					file.seekg(6, std::ios::cur);

					if (!ReadUInt16(file, bitsPerSample)) return ImportResult::ErrorReadFailed;

					// Skip any remaining fmt data
					uint32_t remaining = chunkDataSize - 16;
					file.seekg(remaining, std::ios::cur);
				}
				else if (std::strncmp(chunkID, "data", 4) == 0) {
					// Found data chunk - read samples
					if (audioFormat == 0) {
						return ImportResult::ErrorInvalidFormat; // fmt must come before data
					}

					// Only support 16-bit PCM mono for now
					if (audioFormat != 1 || numChannels != 1) {
						return ImportResult::ErrorUnsupportedFormat;
					}

					if (bitsPerSample != 16) {
						return ImportResult::ErrorUnsupportedFormat;
					}

					// Calculate number of samples
					size_t numSamples = chunkDataSize / (bitsPerSample / 8);

					// Read 16-bit PCM samples and convert to float
					std::vector<float> samples(numSamples);
					for (size_t i = 0; i < numSamples; ++i) {
						uint16_t sampleInt;
						if (!ReadUInt16(file, sampleInt)) {
							return ImportResult::ErrorReadFailed;
						}

						// Convert from 16-bit signed to float [-1.0, 1.0]
						int16_t signedSample = static_cast<int16_t>(sampleInt);
						samples[i] = signedSample / 32768.0f;
					}

					// Try to infer frame structure
					// Common wavetable sample counts: 2048, 1024, 512, 256
					int samplesPerFrame = 2048;
					int commonSizes[] = { 2048, 1024, 512, 256, 128 };

					for (int size : commonSizes) {
						if (numSamples % size == 0) {
							samplesPerFrame = size;
							break;
						}
					}

					int numFrames = static_cast<int>(numSamples / samplesPerFrame);

					// If we can't determine frames, treat as single-frame wavetable
					if (numFrames == 0) {
						numFrames = 1;
						samplesPerFrame = static_cast<int>(numSamples);
					}

					// Fill output structure
					outWavetable.samples = std::move(samples);
					outWavetable.numFrames = numFrames;
					outWavetable.samplesPerFrame = samplesPerFrame;
					outWavetable.sampleRate = sampleRate;
					outWavetable.filename = filename;

					return ImportResult::Success;
				}
				else {
					// Skip unknown chunk
					file.seekg(chunkDataSize, std::ios::cur);
				}
			}

			return ImportResult::ErrorInvalidFormat;
		}

		// Get error message
		const char* WavetableImporter::GetErrorMessage(ImportResult result) {
			switch (result) {
			case ImportResult::Success:
				return "Success";
			case ImportResult::ErrorFileNotFound:
				return "File not found";
			case ImportResult::ErrorInvalidFormat:
				return "Invalid file format";
			case ImportResult::ErrorUnsupportedFormat:
				return "Unsupported format (only 16-bit mono WAV supported)";
			case ImportResult::ErrorReadFailed:
				return "Failed to read file";
			case ImportResult::ErrorInvalidSampleCount:
				return "Invalid sample count";
			default:
				return "Unknown error";
			}
		}
	}
}