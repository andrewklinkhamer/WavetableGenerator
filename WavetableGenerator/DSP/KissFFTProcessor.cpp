#include "KissFFTProcessor.h"
#include "../../third_party/kiss_fft/kiss_fft.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace WavetableGen {
	namespace DSP {
		KissFFTProcessor::KissFFTProcessor(int fftSize)
			: m_fftSize(fftSize)
			, m_fftForward(nullptr)
			, m_fftInverse(nullptr) {
			InitializeFFT();
		}

		KissFFTProcessor::~KissFFTProcessor() {
			CleanupFFT();
		}

		void KissFFTProcessor::InitializeFFT() {
			CleanupFFT();

			// Validate FFT size (must be power of 2)
			if (m_fftSize <= 0 || (m_fftSize & (m_fftSize - 1)) != 0) {
				throw std::invalid_argument("FFT size must be a power of 2");
			}

			// Allocate FFT configurations
			m_fftForward = kiss_fftr_alloc(m_fftSize, 0, nullptr, nullptr);
			m_fftInverse = kiss_fftr_alloc(m_fftSize, 1, nullptr, nullptr);

			if (!m_fftForward || !m_fftInverse) {
				CleanupFFT();
				throw std::runtime_error("Failed to allocate FFT configuration");
			}
		}

		void KissFFTProcessor::CleanupFFT() {
			if (m_fftForward) {
				kiss_fftr_free(m_fftForward);
				m_fftForward = nullptr;
			}
			if (m_fftInverse) {
				kiss_fftr_free(m_fftInverse);
				m_fftInverse = nullptr;
			}
		}

		void KissFFTProcessor::SetFFTSize(int fftSize) {
			if (fftSize != m_fftSize) {
				m_fftSize = fftSize;
				InitializeFFT();
			}
		}

		void KissFFTProcessor::Forward(const std::vector<float>& timeDomain,
			std::vector<FrequencyBin>& frequencyDomain) {
			int inputSize = static_cast<int>(timeDomain.size());

			// Ensure FFT is configured for this size
			if (inputSize != m_fftSize) {
				SetFFTSize(inputSize);
			}

			// Allocate complex output (real FFT produces N/2+1 complex bins)
			int numBins = m_fftSize / 2 + 1;
			std::vector<kiss_fft_cpx> fftOutput(numBins);

			// Perform forward FFT
			kiss_fftr(m_fftForward, timeDomain.data(), fftOutput.data());

			// Convert to magnitude/phase representation
			frequencyDomain.resize(numBins);
			for (int i = 0; i < numBins; ++i) {
				float real = fftOutput[i].r;
				float imag = fftOutput[i].i;

				// Calculate magnitude and phase
				frequencyDomain[i].magnitude = std::sqrt(real * real + imag * imag);
				frequencyDomain[i].phase = std::atan2(imag, real);
			}
		}

		void KissFFTProcessor::Inverse(const std::vector<FrequencyBin>& frequencyDomain,
			std::vector<float>& timeDomain) {
			int numBins = static_cast<int>(frequencyDomain.size());
			int expectedSize = (numBins - 1) * 2;

			// Ensure FFT is configured for this size
			if (expectedSize != m_fftSize) {
				SetFFTSize(expectedSize);
			}

			// Convert magnitude/phase back to complex representation
			std::vector<kiss_fft_cpx> fftInput(numBins);
			for (int i = 0; i < numBins; ++i) {
				float mag = frequencyDomain[i].magnitude;
				float phase = frequencyDomain[i].phase;

				fftInput[i].r = mag * std::cos(phase);
				fftInput[i].i = mag * std::sin(phase);
			}

			// Allocate output buffer
			timeDomain.resize(m_fftSize);

			// Perform inverse FFT
			kiss_fftri(m_fftInverse, fftInput.data(), timeDomain.data());
		}
	}
}