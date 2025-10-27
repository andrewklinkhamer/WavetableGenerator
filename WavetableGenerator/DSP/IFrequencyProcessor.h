#ifndef IFREQUENCYPROCESSOR_H
#define IFREQUENCYPROCESSOR_H

#include <vector>

namespace WavetableGen {
	namespace DSP {
		// Represents a frequency bin with magnitude and phase
		struct FrequencyBin {
			float magnitude;
			float phase;

			FrequencyBin() : magnitude(0.0f), phase(0.0f) {}
			FrequencyBin(float mag, float ph) : magnitude(mag), phase(ph) {}
		};

		// Interface for frequency domain processing (Dependency Inversion Principle)
		// Abstracts the FFT implementation details
		class IFrequencyProcessor {
		public:
			virtual ~IFrequencyProcessor() = default;

			// Transform time domain to frequency domain
			// Input: time domain samples (must be power of 2 size)
			// Output: frequency bins (size will be inputSize/2 + 1 for real FFT)
			virtual void Forward(const std::vector<float>& timeDomain,
				std::vector<FrequencyBin>& frequencyDomain) = 0;

			// Transform frequency domain back to time domain
			// Input: frequency bins
			// Output: time domain samples
			virtual void Inverse(const std::vector<FrequencyBin>& frequencyDomain,
				std::vector<float>& timeDomain) = 0;

			// Get the FFT size this processor is configured for
			virtual int GetFFTSize() const = 0;
		};
	}
}

#endif // IFREQUENCYPROCESSOR_H
