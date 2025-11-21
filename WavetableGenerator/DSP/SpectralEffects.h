#ifndef SPECTRALEFFECTS_H
#define SPECTRALEFFECTS_H

#include "IFrequencyProcessor.h"
#include <vector>
#include <memory>
#include <functional>

namespace WavetableGen {
	namespace Core {
		// Spectral (frequency domain) effects processor
		// Uses dependency injection for FFT implementation (SOLID: Dependency Inversion)
		class SpectralEffects {
		public:
			// Constructor with dependency injection
			explicit SpectralEffects(std::shared_ptr<DSP::IFrequencyProcessor> fftProcessor);

			// Apply spectral decay - progressively attenuates higher frequencies
			// amount: 0.0 (no decay) to 1.0 (maximum decay)
			// curve: controls the steepness of decay (1.0 = linear, >1.0 = exponential)
			void ApplySpectralDecay(std::vector<float>& samples, float amount, float curve);

			// Apply spectral tilt - linear frequency slope
			// amount: -1.0 (darker) to 1.0 (brighter)
			void ApplySpectralTilt(std::vector<float>& samples, float amount);

			// Apply spectral gate - removes quiet frequency bins
			// threshold: 0.0 to 1.0 (relative to max magnitude)
			void ApplySpectralGate(std::vector<float>& samples, float threshold);

			// Apply spectral shift - shifts frequency bins
			// shiftAmount: number of bins to shift (positive or negative)
			void ApplySpectralShift(std::vector<float>& samples, int shiftAmount);

			// Apply phase randomization - smears transients
			// amount: 0.0 to 1.0 (mix of random phase)
			void ApplyPhaseRandomization(std::vector<float>& samples, float amount);

		private:
			// Helper to process in frequency domain
			void ProcessInFrequencyDomain(
				std::vector<float>& samples,
				std::function<void(std::vector<DSP::FrequencyBin>&, int)> processor);

			// Ensure samples are padded to power of 2
			int GetPaddedSize(int size) const;

			std::shared_ptr<DSP::IFrequencyProcessor> m_fftProcessor;
		};
	}
}

#endif // SPECTRALEFFECTS_H
