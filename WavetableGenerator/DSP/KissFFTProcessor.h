#ifndef KISSFFTPROCESSOR_H
#define KISSFFTPROCESSOR_H

#include "IFrequencyProcessor.h"
#include <memory>

// Forward declarations to avoid exposing KissFFT in header
struct kiss_fftr_state;
typedef struct kiss_fftr_state* kiss_fftr_cfg;

namespace WavetableGen {
	namespace DSP {
		// KissFFT wrapper implementation (Single Responsibility Principle)
		// Encapsulates all KissFFT-specific details
		class KissFFTProcessor : public IFrequencyProcessor {
		public:
			// Constructor with FFT size (must be power of 2)
			explicit KissFFTProcessor(int fftSize = 2048);
			~KissFFTProcessor() override;

			// IFrequencyProcessor implementation
			void Forward(const std::vector<float>& timeDomain,
				std::vector<FrequencyBin>& frequencyDomain) override;

			void Inverse(const std::vector<FrequencyBin>& frequencyDomain,
				std::vector<float>& timeDomain) override;

			int GetFFTSize() const override { return m_fftSize; }

			// Reconfigure for different FFT size
			void SetFFTSize(int fftSize);

		private:
			void InitializeFFT();
			void CleanupFFT();

			int m_fftSize;
			kiss_fftr_cfg m_fftForward;
			kiss_fftr_cfg m_fftInverse;
		};
	}
}

#endif // KISSFFTPROCESSOR_H
