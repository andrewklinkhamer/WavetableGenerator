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
			// amount: -1.0 (bass cut) to 1.0 (treble cut)
			void ApplySpectralTilt(std::vector<float>& samples, float amount);

			// Apply spectral gate - removes frequencies below threshold
			// threshold: 0.0 to 1.0 (fraction of maximum magnitude)
			void ApplySpectralGate(std::vector<float>& samples, float threshold);

			// Apply phase randomization - randomizes phase while preserving magnitude
			// amount: 0.0 (no randomization) to 1.0 (full randomization)
			void ApplyPhaseRandomization(std::vector<float>& samples, float amount);

		private:
			// Reusable frequency domain processing template (DRY principle)
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
