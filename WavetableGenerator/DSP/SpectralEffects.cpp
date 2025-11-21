#include "SpectralEffects.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <random>

namespace WavetableGen {
	namespace Core {
		SpectralEffects::SpectralEffects(std::shared_ptr<DSP::IFrequencyProcessor> fftProcessor)
			: m_fftProcessor(fftProcessor) {
			if (!m_fftProcessor) {
				throw std::invalid_argument("FFT processor cannot be null");
			}
		}

		int SpectralEffects::GetPaddedSize(int size) const {
			// Find next power of 2
			int padded = 1;
			while (padded < size) {
				padded <<= 1;
			}
			return padded;
		}

		void SpectralEffects::ProcessInFrequencyDomain(
			std::vector<float>& samples,
			std::function<void(std::vector<DSP::FrequencyBin>&, int)> processor) {
			if (samples.empty()) return;

			int originalSize = static_cast<int>(samples.size());
			int paddedSize = GetPaddedSize(originalSize);

			// Pad to power of 2 if necessary
			std::vector<float> paddedSamples = samples;
			if (paddedSize > originalSize) {
				paddedSamples.resize(paddedSize, 0.0f);
			}

			// Reconfigure FFT if size changed
			if (paddedSize != m_fftProcessor->GetFFTSize()) {
				std::vector<DSP::FrequencyBin> dummyOutput;
				m_fftProcessor->Forward(paddedSamples, dummyOutput);
			}

			// Forward FFT
			std::vector<DSP::FrequencyBin> frequencyDomain;
			m_fftProcessor->Forward(paddedSamples, frequencyDomain);

			// Apply frequency domain processing
			processor(frequencyDomain, paddedSize);

			// Inverse FFT
			std::vector<float> output;
			m_fftProcessor->Inverse(frequencyDomain, output);

			// Copy back original size (remove padding)
			samples.resize(originalSize);
			for (int i = 0; i < originalSize; ++i) {
				samples[i] = output[i];
			}

			// Normalize to prevent clipping from FFT round-trip
			float maxVal = 0.0f;
			for (float s : samples) {
				maxVal = std::max(maxVal, std::abs(s));
			}
			if (maxVal > 1.0f) {
				float scale = 1.0f / maxVal;
				for (float& s : samples) {
					s *= scale;
				}
			}
		}

		void SpectralEffects::ApplySpectralDecay(std::vector<float>& samples,
			float amount, float curve) {
			if (amount < 0.001f) return; // Skip if amount is negligible

			ProcessInFrequencyDomain(samples,
				[amount, curve](std::vector<DSP::FrequencyBin>& bins, int fftSize) {
					int numBins = static_cast<int>(bins.size());

					// Apply decay curve to each frequency bin
					for (int i = 0; i < numBins; ++i) {
						// Normalized frequency (0.0 to 1.0)
						float freq = static_cast<float>(i) / static_cast<float>(numBins - 1);

						// Calculate decay factor using power curve
						// Higher frequencies decay more
						float decay = 1.0f - (amount * std::pow(freq, curve));
						decay = std::max(0.0f, std::min(1.0f, decay));

						// Apply decay to magnitude
						bins[i].magnitude *= decay;
					}
				});
		}

		void SpectralEffects::ApplySpectralTilt(std::vector<float>& samples, float amount) {
			if (std::abs(amount) < 0.001f) return;

			ProcessInFrequencyDomain(samples,
				[amount](std::vector<DSP::FrequencyBin>& bins, int fftSize) {
					int numBins = static_cast<int>(bins.size());

					for (int i = 0; i < numBins; ++i) {
						// Normalized frequency (0.0 to 1.0)
						float freq = static_cast<float>(i) / static_cast<float>(numBins - 1);

						// Linear tilt: -6dB/octave to +6dB/octave
						float tilt = 1.0f + (amount * (freq - 0.5f) * 2.0f);
						tilt = std::max(0.0f, std::min(2.0f, tilt));

						bins[i].magnitude *= tilt;
					}
				});
		}

		void SpectralEffects::ApplySpectralGate(std::vector<float>& samples, float threshold) {
			if (threshold < 0.001f) return;

			ProcessInFrequencyDomain(samples,
				[threshold](std::vector<DSP::FrequencyBin>& bins, int fftSize) {
					// Find maximum magnitude
					float maxMag = 0.0f;
					for (const auto& bin : bins) {
						maxMag = std::max(maxMag, bin.magnitude);
					}

					// Apply gate
					float gateThreshold = maxMag * threshold;
					for (auto& bin : bins) {
						if (bin.magnitude < gateThreshold) {
							bin.magnitude = 0.0f;
						}
					}
				});
		}
		
		void SpectralEffects::ApplySpectralShift(std::vector<float>& samples, int shiftAmount) {
			if (shiftAmount == 0) return;

			ProcessInFrequencyDomain(samples,
				[shiftAmount](std::vector<DSP::FrequencyBin>& bins, int fftSize) {
					int numBins = static_cast<int>(bins.size());
					std::vector<DSP::FrequencyBin> shiftedBins(numBins);

					// Initialize with silence
					for (int i = 0; i < numBins; ++i) {
						shiftedBins[i] = { 0.0f, 0.0f };
					}

					// DC component (index 0) stays at 0
					shiftedBins[0] = bins[0];

					// Shift other bins
					for (int i = 1; i < numBins; ++i) {
						int newIndex = i + shiftAmount;
						if (newIndex >= 1 && newIndex < numBins) {
							shiftedBins[newIndex] = bins[i];
						}
					}

					// Copy back
					bins = shiftedBins;
				});
		}

		void SpectralEffects::ApplyPhaseRandomization(std::vector<float>& samples, float amount) {
			if (amount < 0.001f) return;

			ProcessInFrequencyDomain(samples,
				[amount](std::vector<DSP::FrequencyBin>& bins, int fftSize) {
					// Create random number generator inside lambda
					static std::random_device rd;
					static std::mt19937 gen(rd());
					std::uniform_real_distribution<float> dis(0.0f, 1.0f);

					constexpr float TWO_PI = 6.28318530718f;

					// Randomize phase for each bin (except DC component)
					for (size_t i = 1; i < bins.size(); ++i) {
						// Generate random phase between -PI and PI
						float randomPhase = (dis(gen) * 2.0f - 1.0f) * 3.14159265359f;

						// Blend between original and random phase
						bins[i].phase = bins[i].phase * (1.0f - amount) + randomPhase * amount;
					}

					// Keep DC component (i=0) phase unchanged (should be 0)
				});
		}

	}
}