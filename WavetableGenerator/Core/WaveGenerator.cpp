#include "WaveGenerator.h"
#include "../IO/FileWriterFactory.h"
#include "../DSP/KissFFTProcessor.h"
#include "WaveTypeName.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <sstream>

namespace WavetableGen {
	namespace Core {
		using namespace IO;

		// PolyBLEP (Polynomial Band-Limited Step) for anti-aliasing
		float WaveGenerator::PolyBLEP(float t, float dt) {
			if (t < dt) {
				t = t / dt;
				return t + t - t * t - 1.0f;
			}
			else if (t > 1.0f - dt) {
				t = (t - 1.0f) / dt;
				return t * t + t + t + 1.0f;
			}
			return 0.0f;
		}

		// Generate a single waveform cycle (bandlimited)
		std::vector<float> WaveGenerator::GenerateWave(
			WaveType type,
			size_t numSamples,
			double pulseDuty,
			int maxHarmonics
		) {
			std::vector<float> samples(numSamples);
			float dt = 1.0f / (float)numSamples;

			for (size_t n = 0; n < numSamples; ++n) {
				double t = (double)n / numSamples;
				float val = 0.0f;

				switch (type) {
					// ===== TAB 0: BASIC WAVES =====
				case WaveType::Sine:
					samples[n] = (float)sin(2 * PI * t);
					break;

				case WaveType::Square: {
					float value = (float)t < 0.5f ? 1.0f : -1.0f;
					value -= PolyBLEP((float)t, dt);
					value += PolyBLEP(fmodf((float)t + 0.5f, 1.0f), dt);
					samples[n] = value;
					break;
				}

				case WaveType::Triangle: {
					float value = 1.0f - 4.0f * std::abs((float)t - 0.5f);
					value += dt * PolyBLEP((float)t, dt);
					value -= dt * PolyBLEP(fmodf((float)t + 0.5f, 1.0f), dt);
					samples[n] = value;
					break;
				}

				case WaveType::Saw: {
					float value = 2.0f * (float)t - 1.0f;
					value -= 2.0f * PolyBLEP((float)t, dt);
					samples[n] = value;
					break;
				}

				case WaveType::ReverseSaw: {
					float value = 1.0f - 2.0f * (float)t;
					value += 2.0f * PolyBLEP((float)t, dt);
					samples[n] = value;
					break;
				}

				case WaveType::Pulse: {
					float value = (float)t < pulseDuty ? 1.0f : -1.0f;
					value -= PolyBLEP((float)t, dt);
					value += PolyBLEP(fmodf((float)t - (float)pulseDuty + 1.0f, 1.0f), dt);
					samples[n] = value;
					break;
				}

				// ===== TAB 1: CHAOS THEORY =====
				case WaveType::Lorenz: {
					// Lorenz attractor - project X coordinate
					float sigma = 10.0f, rho = 28.0f, beta = 8.0f / 3.0f;
					float x = 0.1f, y = 0.0f, z = 0.0f;
					float dt_sim = 0.01f;

					// Run simulation to get to this point in cycle
					int steps = (int)(t * 200.0f);
					for (int s = 0; s < steps; ++s) {
						float dx = sigma * (y - x);
						float dy = x * (rho - z) - y;
						float dz = x * y - beta * z;
						x += dx * dt_sim;
						y += dy * dt_sim;
						z += dz * dt_sim;
					}
					samples[n] = std::tanh(x / 15.0f); // Normalize
					break;
				}

				case WaveType::Rossler: {
					// Rössler attractor - project X coordinate
					float a = 0.2f, b = 0.2f, c = 5.7f;
					float x = 0.1f, y = 0.0f, z = 0.0f;
					float dt_sim = 0.05f;

					int steps = (int)(t * 100.0f);
					for (int s = 0; s < steps; ++s) {
						float dx = -y - z;
						float dy = x + a * y;
						float dz = b + z * (x - c);
						x += dx * dt_sim;
						y += dy * dt_sim;
						z += dz * dt_sim;
					}
					samples[n] = std::tanh(x / 5.0f);
					break;
				}

				case WaveType::Henon: {
					// Hénon map
					float a = 1.4f, b = 0.3f;
					float x = 0.1f, y = 0.1f;

					int iterations = (int)(t * 50.0f);
					for (int i = 0; i < iterations; ++i) {
						float xnew = 1.0f - a * x * x + y;
						y = b * x;
						x = xnew;
					}
					samples[n] = std::tanh(x);
					break;
				}

				case WaveType::Duffing: {
					// Duffing oscillator - forced nonlinear oscillator
					float alpha = -1.0f, beta = 1.0f, delta = 0.3f, gamma = 0.37f, omega = 1.2f;
					float x = 0.1f, v = 0.0f;
					float dt_sim = 0.05f;
					float time = (float)t * 10.0f;

					for (float t_sim = 0; t_sim < time; t_sim += dt_sim) {
						float force = gamma * (float)cos(omega * t_sim);
						float dv = -delta * v - alpha * x - beta * x * x * x + force;
						v += dv * dt_sim;
						x += v * dt_sim;
					}
					samples[n] = std::tanh(x);
					break;
				}

				case WaveType::Chua: {
					// Chua's circuit - simplified version
					float alpha = 15.6f, beta = 28.0f;
					float x = 0.1f, y = 0.0f, z = 0.0f;
					float dt_sim = 0.01f;

					int steps = (int)(t * 150.0f);
					for (int s = 0; s < steps; ++s) {
						float h = -1.143f * x + 0.714f * (std::abs(x + 1.0f) - std::abs(x - 1.0f));
						float dx = alpha * (y - x - h);
						float dy = x - y + z;
						float dz = -beta * y;
						x += dx * dt_sim;
						y += dy * dt_sim;
						z += dz * dt_sim;
					}
					samples[n] = std::tanh(x / 2.0f);
					break;
				}

				case WaveType::LogisticChaos: {
					// Logistic map in chaotic regime (r = 3.9)
					float r = 3.9f;
					float x = 0.5f;

					int iterations = (int)(t * 100.0f) + 50; // Skip transient
					for (int i = 0; i < iterations; ++i) {
						x = r * x * (1.0f - x);
					}
					samples[n] = (x - 0.5f) * 2.0f;
					break;
				}

				// ===== TAB 2: FRACTALS =====
				case WaveType::Weierstrass: {
					// Weierstrass function: sum of cos(b^n * pi * x) * a^n
					// a controls smoothness (0 < a < 1), b must be odd integer
					float a = 0.5f; // Smoothness parameter
					int b = 7;      // Frequency multiplier (odd)
					int iterations = 8;

					float weierstrassVal = 0.0f;
					float a_n = 1.0f;
					int b_n = 1;

					for (int k = 0; k < iterations; ++k) {
						weierstrassVal += a_n * (float)cos(b_n * PI * t);
						a_n *= a;
						b_n *= b;
					}
					samples[n] = weierstrassVal;
					break;
				}

				case WaveType::Cantor: {
					// Cantor (Devil's Staircase) function
					float cantorVal = 0.0f;
					float tScaled = fmodf((float)t, 1.0f);
					int iterations = 6;

					for (int k = 0; k < iterations; ++k) {
						float power = (float)pow(3.0, k);
						float segment = fmodf(tScaled * power, 1.0f);

						if (segment < 0.333f) {
							cantorVal += 0.0f / power;
						}
						else if (segment > 0.666f) {
							cantorVal += 1.0f / power;
						}
						else {
							cantorVal += 0.5f / power;
						}
					}
					samples[n] = (cantorVal - 0.5f) * 2.0f; // Center around zero
					break;
				}

				case WaveType::Koch: {
					// Koch curve projection - creates crystalline patterns
					float kochVal = 0.0f;
					int iterations = 5;

					for (int k = 1; k <= iterations; ++k) {
						float freq = (float)pow(4.0, k - 1);
						float amp = 1.0f / freq;
						// Create angular, fractal pattern
						float phase = fmodf((float)t * freq, 1.0f);
						if (phase < 0.25f) {
							kochVal += amp * (phase * 4.0f);
						}
						else if (phase < 0.5f) {
							kochVal += amp * (2.0f - phase * 4.0f);
						}
						else if (phase < 0.75f) {
							kochVal += amp * ((phase - 0.5f) * 4.0f);
						}
						else {
							kochVal += amp * (1.0f - (phase - 0.75f) * 4.0f);
						}
					}
					samples[n] = (kochVal - 0.5f) * 2.0f;
					break;
				}

				case WaveType::Mandelbrot: {
					// Sample from Mandelbrot set boundary
					// Map waveform position to complex plane
					float cReal = -0.7f + (float)t * 0.6f; // Scan interesting region
					float cImag = 0.0f;

					float zReal = 0.0f;
					float zImag = 0.0f;
					int maxIterations = 20;
					int iteration = 0;

					// Mandelbrot iteration: z = z^2 + c
					while (iteration < maxIterations && (zReal * zReal + zImag * zImag) < 4.0f) {
						float zRealNew = zReal * zReal - zImag * zImag + cReal;
						float zImagNew = 2.0f * zReal * zImag + cImag;
						zReal = zRealNew;
						zImag = zImagNew;
						iteration++;
					}

					// Use iteration count to create waveform
					float mandelbrotVal = (float)iteration / (float)maxIterations;
					samples[n] = (mandelbrotVal - 0.5f) * 2.0f;
					break;
				}

				// ===== TAB 3: HARMONIC WAVES =====
				case WaveType::OddHarmonics:
					for (int k = 1; k <= maxHarmonics * 2; k += 2) {
						if (k <= maxHarmonics * 2 - 1)
							val += (float)sin(2 * PI * k * t) / k;
					}
					samples[n] = val;
					break;

				case WaveType::EvenHarmonics:
					for (int k = 2; k <= maxHarmonics; k += 2) {
						val += (float)sin(2 * PI * k * t) / k;
					}
					samples[n] = val;
					break;

				case WaveType::HarmonicSeries:
					for (int k = 1; k <= maxHarmonics; ++k) {
						val += (float)sin(2 * PI * k * t) / k;
					}
					samples[n] = val;
					break;

				case WaveType::SubHarmonics:
					val = (float)sin(2 * PI * t);
					val += 0.5f * (float)sin(2 * PI * t + PI / 2.0);
					val += 0.25f * (float)sin(2 * PI * t + PI);
					samples[n] = val;
					break;

				case WaveType::Formant: {
					// Bandlimited vocal formants
					val = 0.0f;
					val += (float)sin(2 * PI * 2 * t);
					val += (float)sin(2 * PI * 3 * t) * 0.7f;
					samples[n] = val;
					break;
				}

				case WaveType::Additive:
					for (int k = 1; k <= maxHarmonics; ++k) {
						val += (float)sin(2 * PI * k * t) / k;
					}
					samples[n] = val;
					break;

				// ===== TAB 4: INHARMONIC SERIES =====
				case WaveType::StretchedHarm: {
					// Harmonics stretched wider than natural
					val = 0.0f;
					for (int k = 1; k <= maxHarmonics; ++k) {
						float stretch = (float)pow(k, 1.05); // Slightly stretched
						val += (float)sin(2 * PI * stretch * t) / k;
					}
					samples[n] = val;
					break;
				}

				case WaveType::CompressedHarm: {
					// Harmonics compressed closer together
					val = 0.0f;
					for (int k = 1; k <= maxHarmonics; ++k) {
						float compress = (float)pow(k, 0.95); // Slightly compressed
						val += (float)sin(2 * PI * compress * t) / k;
					}
					samples[n] = val;
					break;
				}

				case WaveType::Metallic: {
					// Bell-like inharmonic overtones
					val = (float)sin(2 * PI * t); // Fundamental
					val += 0.5f * (float)sin(2 * PI * 2.76f * t); // Inharmonic partial
					val += 0.3f * (float)sin(2 * PI * 5.40f * t);
					val += 0.2f * (float)sin(2 * PI * 8.93f * t);
					samples[n] = val / 2.0f;
					break;
				}

				case WaveType::Clangorous: {
					// Gong/cymbal-like inharmonic spectrum
					val = (float)sin(2 * PI * t);
					val += 0.6f * (float)sin(2 * PI * 1.593f * t);
					val += 0.4f * (float)sin(2 * PI * 2.136f * t);
					val += 0.3f * (float)sin(2 * PI * 2.653f * t);
					val += 0.2f * (float)sin(2 * PI * 3.593f * t);
					samples[n] = val / 2.5f;
					break;
				}

				case WaveType::KarplusStrong: {
					// Karplus-Strong plucked string algorithm
					static std::vector<float> delayLine;
					int delayLength = 50; // Short delay for higher pitch

					if (n == 0) {
						// Initialize with noise burst
						delayLine.clear();
						delayLine.resize(delayLength);
						for (int i = 0; i < delayLength; ++i) {
							delayLine[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
						}
					}

					// Karplus-Strong averaging filter
					float output = delayLine[n % delayLength];
					float next = (output + delayLine[(n + 1) % delayLength]) * 0.5f * 0.996f; // Damping
					delayLine[n % delayLength] = next;
					samples[n] = output;
					break;
				}

				case WaveType::StiffString: {
					// Piano-like inharmonicity (f_n = n * f0 * sqrt(1 + B*n^2))
					val = 0.0f;
					float B = 0.0001f; // Inharmonicity coefficient
					for (int k = 1; k <= maxHarmonics; ++k) {
						float freq = (float)k * (float)sqrt(1.0f + B * k * k);
						val += (float)sin(2 * PI * freq * t) / k;
					}
					samples[n] = val;
					break;
				}

				// ===== TAB 5: MODERN/DIGITAL + MATHEMATICAL =====
				case WaveType::Supersaw: {
					// Bandlimited supersaw
					float value = 2.0f * (float)t - 1.0f;
					value -= 2.0f * PolyBLEP((float)t, dt);
					value += 0.2f * (float)sin(2 * PI * 2 * t);
					value += 0.1f * (float)sin(2 * PI * 3 * t);
					samples[n] = value;
					break;
				}

				case WaveType::PWMSaw: {
					double duty = 0.25;
					float value = (float)t < duty ? 1.0f : -1.0f;
					value -= PolyBLEP((float)t, dt);
					value += PolyBLEP(fmodf((float)t - (float)duty + 1.0f, 1.0f), dt);
					samples[n] = value;
					break;
				}

				case WaveType::Parabolic:
					samples[n] = 1.0f - 4.0f * (float)((t - 0.5) * (t - 0.5));
					break;

				case WaveType::DoubleSine:
					samples[n] = (float)sin(2 * PI * t) * (float)cos(2 * PI * t);
					break;

				case WaveType::HalfSine:
					samples[n] = (float)std::abs(sin(2 * PI * t)) * 2.0f - 1.0f;
					break;

				case WaveType::Trapezoid: {
					float slope = 0.2f;
					float value;
					if (t < slope)
						value = (float)t / slope * 2.0f - 1.0f;
					else if (t < 0.5f)
						value = 1.0f;
					else if (t < 0.5f + slope)
						value = 1.0f - ((float)t - 0.5f) / slope * 2.0f;
					else
						value = -1.0f;
					value += dt * PolyBLEP((float)t, dt);
					value -= dt * PolyBLEP(fmodf((float)t - slope, 1.0f), dt);
					value -= dt * PolyBLEP(fmodf((float)t - 0.5f, 1.0f), dt);
					value += dt * PolyBLEP(fmodf((float)t - 0.5f - slope, 1.0f), dt);
					samples[n] = value;
					break;
				}

				case WaveType::Power: {
					float phase = (float)t * 2.0f;
					float x;
					if (phase < 1.0f) {
						x = phase;
						samples[n] = std::pow(x, 1.5f) * 2.0f - 1.0f;
					}
					else {
						x = phase - 1.0f;
						samples[n] = 1.0f - std::pow(x, 1.5f) * 2.0f;
					}
					break;
				}

				case WaveType::Exponential: {
					float phase = (float)t * 2.0f;
					if (phase < 1.0f) {
						samples[n] = 2.0f * (std::exp(phase) - 1.0f) / (std::exp(1.0f) - 1.0f) - 1.0f;
					}
					else {
						float x = phase - 1.0f;
						samples[n] = 1.0f - 2.0f * (std::exp(x) - 1.0f) / (std::exp(1.0f) - 1.0f);
					}
					break;
				}

				case WaveType::Logistic: {
					float phase = (float)t * 2.0f;
					if (phase < 1.0f) {
						float x = phase * 12.0f - 6.0f;
						float logistic = 1.0f / (1.0f + std::exp(-x));
						samples[n] = (logistic - 0.5f) * 2.0f;
					}
					else {
						float x = (phase - 1.0f) * 12.0f - 6.0f;
						float logistic = 1.0f / (1.0f + std::exp(-x));
						samples[n] = (0.5f - logistic) * 2.0f;
					}
					break;
				}

				case WaveType::Stepped: {
					int step = (int)(t * 8.0f) % 8;
					samples[n] = (step / 3.5f) - 1.0f;
					break;
				}

				case WaveType::Noise: {
					float noiseVal = 0.0f;
					for (int k = 1; k <= maxHarmonics; ++k) {
						float phaseOffset = (float)(k * 123456789u % 1000) / 1000.0f;
						float amplitude = 1.0f / (float)k;
						noiseVal += amplitude * (float)sin(2 * PI * k * t + phaseOffset * 2 * PI);
					}
					samples[n] = noiseVal;
					break;
				}

				case WaveType::Procedural:
					samples[n] = (float)std::tanh(3.0 * sin(2 * PI * t));
					break;

				case WaveType::Sinc: {
					// Sinc function: sin(x)/x
					// Map t (0..1) to range -8PI to 8PI to show multiple lobes
					float x = ((float)t - 0.5f) * 16.0f * (float)PI;
					if (std::abs(x) < 0.001f) {
						samples[n] = 1.0f;
					}
					else {
						samples[n] = (float)sin(x) / x;
					}
					break;
				}

				// ===== TAB 6: MODULATION SYNTHESIS =====
				case WaveType::RingMod: {
					// Ring modulation - classic metallic sound
					float carrier = (float)sin(2 * PI * t);
					float modulator = (float)sin(2 * PI * 3.7f * t); // Inharmonic ratio
					samples[n] = carrier * modulator;
					break;
				}

				case WaveType::AmplitudeMod: {
					// Amplitude modulation
					float carrier = (float)sin(2 * PI * t);
					float modulator = (float)sin(2 * PI * 0.3f * t); // Slow modulation
					samples[n] = carrier * (0.5f + 0.5f * modulator);
					break;
				}

				case WaveType::FrequencyMod: {
					// Deep FM synthesis
					float modIndex = 2.0f;
					float ratio = 2.5f; // C:M ratio
					float modulator = modIndex * (float)sin(2 * PI * ratio * t);
					samples[n] = (float)sin(2 * PI * t + modulator);
					break;
				}

				case WaveType::CrossMod: {
					// Cross modulation - bidirectional FM
					float mod1 = (float)sin(2 * PI * t);
					float mod2 = (float)sin(2 * PI * 1.5f * t);
					float result1 = (float)sin(2 * PI * t + mod2 * 0.5f);
					float result2 = (float)sin(2 * PI * 1.5f * t + mod1 * 0.5f);
					samples[n] = (result1 + result2) * 0.5f;
					break;
				}

				case WaveType::PhaseMod: {
					// Phase modulation synthesis
					float modIndex = 1.5f;
					float ratio = 3.0f;
					float modulator = modIndex * (float)sin(2 * PI * ratio * t);
					samples[n] = (float)sin(2 * PI * t + modulator);
					break;
				}

				// ===== TAB 7: PHYSICAL MODELS =====
				case WaveType::String:
					for (int k = 1; k <= maxHarmonics; ++k) {
						val += (float)sin(2 * PI * k * t) / (k * k);
					}
					samples[n] = val;
					break;

				case WaveType::Brass:
					for (int k = 1; k <= maxHarmonics * 2 - 1; k += 2) {
						if (k <= maxHarmonics)
							val += (float)sin(2 * PI * k * t) / (k * 0.8f);
					}
					samples[n] = val;
					break;

				case WaveType::Reed:
					// Bandlimited clarinet-like (odd harmonics)
					val = (float)sin(2 * PI * t);
					val += 0.3f * (float)sin(2 * PI * 3 * t);
					samples[n] = val;
					break;

				case WaveType::Vocal: {
					// Bandlimited vowel formants
					val = 0.0f;
					val += (float)sin(2 * PI * 2 * t) * 1.0f;
					val += (float)sin(2 * PI * 3 * t) * 0.6f;
					samples[n] = val;
					break;
				}

				case WaveType::Bell:
					// Bandlimited bell-like
					val = (float)sin(2 * PI * t);
					val += 0.5f * (float)sin(2 * PI * 2 * t);
					val += 0.35f * (float)sin(2 * PI * 3 * t);
					samples[n] = val;
					break;

				// ===== TAB 8: SYNTHESIS WAVES =====
				case WaveType::SimpleFM: {
					// Reduced modulation index for bandlimiting
					float carrier = (float)(2 * PI * t);
					float modIndex = 0.3f;
					float modulator = (float)sin(2 * PI * 2 * t) * modIndex;
					samples[n] = (float)sin(carrier + modulator);
					break;
				}

				case WaveType::ComplexFM: {
					// Reduced modulation for bandlimiting
					float carrier = (float)(2 * PI * t);
					float modIndex1 = 0.2f;
					float modIndex2 = 0.15f;
					float mod1 = (float)sin(2 * PI * 2 * t) * modIndex1;
					float mod2 = (float)sin(2 * PI * 3 * t) * modIndex2;
					samples[n] = (float)sin(carrier + mod1 + mod2);
					break;
				}

				case WaveType::PhaseDistortion: {
					// Reduced phase modulation for bandlimiting
					double phaseMod = 0.08;
					double phase = t + phaseMod * sin(2 * PI * t);
					samples[n] = (float)sin(2 * PI * phase);
					break;
				}

				case WaveType::Wavefold: {
					// Reduced wavefolding for bandlimiting
					float amplitude = 1.3f;
					float x = (float)sin(2 * PI * t) * amplitude;
					x = x > 1.0f ? 2.0f - x : x;
					x = x < -1.0f ? -2.0f - x : x;
					samples[n] = x;
					break;
				}

				case WaveType::HardSync: {
					// Bandlimited hard sync
					float value = 2.0f * (float)t - 1.0f;
					value -= 2.0f * PolyBLEP((float)t, dt);
					value += 0.4f * (float)sin(2 * PI * 2 * t);
					value += 0.2f * (float)sin(2 * PI * 3 * t);
					samples[n] = value;
					break;
				}

				case WaveType::Chebyshev: {
					// T3: 4x^3 - 3x (generates 3rd harmonic)
					float x = (float)sin(2 * PI * t);
					samples[n] = 4.0f * x * x * x - 3.0f * x;
					break;
				}

				// ===== TAB 9: VINTAGE SYNTH EMULATIONS =====
				case WaveType::ARPOdyssey: {
					// ARP Odyssey - raw, aggressive oscillators with wide range
					float saw = 2.0f * (float)t - 1.0f;
					float tri = 1.0f - 4.0f * std::abs((float)t - 0.5f);
					// Blend saw and triangle with emphasis on odd harmonics
					float blend = (saw * 0.7f + tri * 0.3f);
					blend += 0.15f * (float)sin(6 * PI * t); // Odd harmonic emphasis
					samples[n] = blend - 2.0f * PolyBLEP((float)t, dt);
					break;
				}

				case WaveType::CS80: {
					// Yamaha CS-80 - lush, warm, thick analog pads
					float saw1 = 2.0f * (float)t - 1.0f;
					float saw2 = 2.0f * fmodf((float)t * 1.003f, 1.0f) - 1.0f;
					float saw3 = 2.0f * fmodf((float)t * 0.997f, 1.0f) - 1.0f;
					float saw4 = 2.0f * fmodf((float)t * 1.001f, 1.0f) - 1.0f;
					// Four detuned oscillators with subtle chorus
					float lfo = (float)sin(2 * PI * 0.3f * t) * 0.002f;
					float blend = (saw1 + saw2 + saw3 + saw4 * (1.0f + lfo)) / 4.0f;
					// Add warmth with low-pass character
					blend = blend * 0.85f + 0.15f * (float)sin(2 * PI * t);
					samples[n] = blend - 2.0f * PolyBLEP((float)t, dt);
					break;
				}

				case WaveType::Juno: {
					// Juno-style saw with chorus character
					float saw1 = 2.0f * (float)t - 1.0f;
					float saw2 = 2.0f * fmodf((float)t * 1.005f, 1.0f) - 1.0f;
					float lfo = (float)sin(2 * PI * 0.5f * t) * 0.003f;
					float chorus = 2.0f * fmodf((float)t * (1.0f + lfo), 1.0f) - 1.0f;
					samples[n] = (saw1 * 0.5f + saw2 * 0.3f + chorus * 0.2f) - 2.0f * PolyBLEP((float)t, dt);
					break;
				}

				case WaveType::MiniMoog: {
					// Minimoog-style saw with slight detuning and warmth
					float saw1 = 2.0f * (float)t - 1.0f;
					float saw2 = 2.0f * fmodf((float)t * 1.002f, 1.0f) - 1.0f; // Slight detune
					float saw3 = 2.0f * fmodf((float)t * 0.998f, 1.0f) - 1.0f;
					float blend = (saw1 + saw2 * 0.7f + saw3 * 0.7f) / 2.4f;
					samples[n] = blend - 2.0f * PolyBLEP((float)t, dt);
					break;
				}

				case WaveType::MS20: {
					// Korg MS-20 - aggressive, acidic, raw with distinctive filter
					float value = 2.0f * (float)t - 1.0f;
					value -= 2.0f * PolyBLEP((float)t, dt);
					// Add resonant peak at different frequency than TB-303
					value = value + 0.4f * (float)sin(10 * PI * t);
					// Hard clipping for aggressive character
					samples[n] = std::tanh(value * 1.5f);
					break;
				}

				case WaveType::Oberheim: {
					// Oberheim SEM style - rich multi-mode character
					float saw = 2.0f * (float)t - 1.0f - 2.0f * PolyBLEP((float)t, dt);
					float pulse = ((float)t < 0.5f ? 1.0f : -1.0f) - PolyBLEP((float)t, dt) + PolyBLEP(fmodf((float)t + 0.5f, 1.0f), dt);
					samples[n] = (saw * 0.6f + pulse * 0.4f);
					break;
				}

				case WaveType::PPG: {
					// PPG Wave style - digital stepped waveshape
					int steps = 64;
					int step = (int)(t * steps) % steps;
					float phase = (float)step / (float)steps;
					samples[n] = (float)sin(2 * PI * phase) * 0.7f + (2.0f * phase - 1.0f) * 0.3f;
					break;
				}

				case WaveType::Prophet5: {
					// Prophet-5 style saw - smoother, less harsh
					float value = 2.0f * (float)t - 1.0f;
					value -= 2.0f * PolyBLEP((float)t, dt);
					// Add slight warmth with 2nd harmonic
					value = value * 0.8f + 0.2f * (float)sin(4 * PI * t);
					samples[n] = value;
					break;
				}

				case WaveType::TB303: {
					// TB-303 style saw - bright and acidic
					float value = 2.0f * (float)t - 1.0f;
					value -= 2.0f * PolyBLEP((float)t, dt);
					// Add resonant peak simulation
					value = value + 0.3f * (float)sin(8 * PI * t);
					samples[n] = std::tanh(value * 1.2f); // Slight saturation
					break;
				}

				// ===== TAB 10: VOWEL FORMANTS =====
				case WaveType::VowelA: {
					// Vowel "A" (as in "father") - F1=730Hz, F2=1090Hz, F3=2440Hz
					float f1 = (float)sin(2 * PI * 730.0 / 261.63 * t);
					float f2 = (float)sin(2 * PI * 1090.0 / 261.63 * t) * 0.7f;
					float f3 = (float)sin(2 * PI * 2440.0 / 261.63 * t) * 0.3f;
					samples[n] = (f1 + f2 + f3) / 2.0f;
					break;
				}

				case WaveType::VowelE: {
					// Vowel "E" (as in "bed") - F1=530Hz, F2=1840Hz, F3=2480Hz
					float f1 = (float)sin(2 * PI * 530.0 / 261.63 * t);
					float f2 = (float)sin(2 * PI * 1840.0 / 261.63 * t) * 0.8f;
					float f3 = (float)sin(2 * PI * 2480.0 / 261.63 * t) * 0.3f;
					samples[n] = (f1 + f2 + f3) / 2.1f;
					break;
				}

				case WaveType::VowelI: {
					// Vowel "I" (as in "see") - F1=270Hz, F2=2290Hz, F3=3010Hz
					float f1 = (float)sin(2 * PI * 270.0 / 261.63 * t);
					float f2 = (float)sin(2 * PI * 2290.0 / 261.63 * t) * 0.9f;
					float f3 = (float)sin(2 * PI * 3010.0 / 261.63 * t) * 0.4f;
					samples[n] = (f1 + f2 + f3) / 2.3f;
					break;
				}

				case WaveType::VowelO: {
					// Vowel "O" (as in "go") - F1=570Hz, F2=840Hz, F3=2410Hz
					float f1 = (float)sin(2 * PI * 570.0 / 261.63 * t);
					float f2 = (float)sin(2 * PI * 840.0 / 261.63 * t) * 0.7f;
					float f3 = (float)sin(2 * PI * 2410.0 / 261.63 * t) * 0.2f;
					samples[n] = (f1 + f2 + f3) / 1.9f;
					break;
				}

				case WaveType::VowelU: {
					// Vowel "U" (as in "boot") - F1=300Hz, F2=870Hz, F3=2240Hz
					float f1 = (float)sin(2 * PI * 300.0 / 261.63 * t);
					float f2 = (float)sin(2 * PI * 870.0 / 261.63 * t) * 0.6f;
					float f3 = (float)sin(2 * PI * 2240.0 / 261.63 * t) * 0.2f;
					samples[n] = (f1 + f2 + f3) / 1.8f;
					break;
				}

				case WaveType::Diphthong: {
					// Morphing vowel (A to I)
					float morph = (float)sin(2 * PI * 0.25f * t) * 0.5f + 0.5f; // 0 to 1
					float f1a = 730.0f, f1i = 270.0f;
					float f2a = 1090.0f, f2i = 2290.0f;
					float f1 = f1a + (f1i - f1a) * morph;
					float f2 = f2a + (f2i - f2a) * morph;
					float formant1 = (float)sin(2 * PI * f1 / 261.63 * t);
					float formant2 = (float)sin(2 * PI * f2 / 261.63 * t) * 0.8f;
					samples[n] = (formant1 + formant2) / 1.8f;
					break;
				}
				}
			}

			// Remove DC offset
			float dcOffset = 0.0f;
			for (float s : samples)
				dcOffset += s;

			dcOffset /= (float)numSamples;

			for (float& s : samples)
				s -= dcOffset;

			return samples;
		}

		// Combine multiple waves with weights (bandlimited, NO normalization)
		std::vector<float> WaveGenerator::CombineWaves(
			const std::vector<std::pair<WaveType, float>>& waves,
			size_t numSamples
		) {
			std::vector<float> result(numSamples, 0.0f);

			for (auto& w : waves) {
				std::vector<float> samples = GenerateWave(w.first, numSamples, m_pulseDuty, m_maxHarmonics);
				float weight = w.second;
				for (size_t i = 0; i < numSamples; ++i)
					result[i] += samples[i] * weight;
			}

			// Remove DC offset
			float dcOffset = 0.0f;
			for (float s : result)
				dcOffset += s;
			dcOffset /= (float)numSamples;

			for (float& s : result)
				s -= dcOffset;

			// NOTE: No normalization here! Will be done globally for entire wavetable
			return result;
		}

		// Generate a multi-frame wavetable with morphing (bandlimited)
		std::vector<float> WaveGenerator::GenerateMultiFrameWavetable(
			const WavetableFrame& startFrame,
			const WavetableFrame& endFrame,
			int numFrames,
			MorphCurve morphCurve
		) {
			std::vector<float> wavetable;
			wavetable.reserve(numFrames * SAMPLES_PER_WAVE);

			for (int frame = 0; frame < numFrames; ++frame) {
				float morphPositionLinear = (float)frame / (numFrames - 1);
				float morphPosition = WaveformEffects::ApplyMorphCurve(morphPositionLinear, morphCurve);

				std::vector<std::pair<WaveType, float>> frameWaves;

				for (const auto& startWave : startFrame.waveforms) {
					float startWeight = startWave.second * (1.0f - morphPosition);
					if (startWeight > 0.0f) {
						frameWaves.push_back({ startWave.first, startWeight });
					}
				}

				for (const auto& endWave : endFrame.waveforms) {
					float endWeight = endWave.second * morphPosition;
					if (endWeight > 0.0f) {
						bool found = false;
						for (auto& fw : frameWaves) {
							if (fw.first == endWave.first) {
								fw.second += endWeight;
								found = true;
								break;
							}
						}
						if (!found) {
							frameWaves.push_back({ endWave.first, endWeight });
						}
					}
				}

				auto frameSamples = CombineWaves(frameWaves, SAMPLES_PER_WAVE);
				wavetable.insert(wavetable.end(), frameSamples.begin(), frameSamples.end());
			}

			// GLOBAL normalization across ALL frames to preserve relative amplitude relationships
			float globalMaxVal = 0.0f;
			for (float s : wavetable)
				globalMaxVal = (std::max)(globalMaxVal, std::abs(s));

			if (globalMaxVal > 0.0f) {
				for (float& s : wavetable)
					s /= globalMaxVal;
			}

			return wavetable;
		}

		// Normalize samples to -1.0 to 1.0 range
		void WaveGenerator::NormalizeSamples(std::vector<float>& samples) {
			float maxVal = 0.0f;
			for (float s : samples)
				maxVal = (std::max)(maxVal, std::abs(s));
			if (maxVal > 0.0f) {
				for (float& s : samples)
					s /= maxVal;
			}
		}

		// Generate audio preview (multi-second looped sample with fades)
		std::vector<float> WaveGenerator::GenerateAudioPreview(const std::vector<std::pair<WaveType, float>>& startWaves,
			const EffectsSettings& effects) {
			auto singleCycle = CombineWaves(startWaves, SAMPLES_PER_WAVE);

			// Apply effects to single cycle
			WaveformEffects::ApplyEffects(singleCycle, effects);

			NormalizeSamples(singleCycle);

			// Generate 2 seconds of audio by repeating the cycle
			int numCycles = (SAMPLE_RATE * 2) / SAMPLES_PER_WAVE;
			std::vector<float> combined;
			combined.reserve(numCycles * SAMPLES_PER_WAVE);

			for (int i = 0; i < numCycles; ++i) {
				combined.insert(combined.end(), singleCycle.begin(), singleCycle.end());
			}

			// Apply fade in/out
			const int fadeLength = static_cast<int>(SAMPLE_RATE) / 20;
			for (size_t i = 0; i < fadeLength && i < combined.size(); ++i) {
				float fadeIn = (float)i / fadeLength;
				combined[i] *= fadeIn;
			}
			for (size_t i = 0; i < fadeLength && i < combined.size(); ++i) {
				float fadeOut = (float)i / fadeLength;
				combined[combined.size() - 1 - i] *= fadeOut;
			}

			return combined;
		}

		// Create end frame for morphing
		WavetableFrame WaveGenerator::CreateEndFrame(const std::vector<std::pair<WaveType, float>>& startWaves,
			const std::vector<std::pair<WaveType, float>>& endWaves) {
			WavetableFrame endFrame;

			if (!endWaves.empty()) {
				endFrame.waveforms = endWaves;
			}
			else {
				// Auto-generate end frame
				if (startWaves.size() > 1) {
					// Reverse order
					for (auto it = startWaves.rbegin(); it != startWaves.rend(); ++it) {
						endFrame.waveforms.push_back(*it);
					}
				}
				else {
					// Add additive variation
					auto baseWave = startWaves[0];
					endFrame.waveforms.push_back(baseWave);

					if (baseWave.first != WaveType::Additive) {
						endFrame.waveforms.push_back({ WaveType::Additive, baseWave.second * 0.5f });
					}
				}
			}

			return endFrame;
		}

		// Generate morphing wavetable
		std::vector<float> WaveGenerator::GenerateMorphingWavetable(const std::vector<std::pair<WaveType, float>>& startWaves,
			const std::vector<std::pair<WaveType, float>>& endWaves, int numFrames, const EffectsSettings& effects,
			MorphCurve morphCurve) {
			WavetableFrame startFrame;
			startFrame.waveforms = startWaves;

			WavetableFrame endFrame = CreateEndFrame(startWaves, endWaves);

			std::vector<float> wavetable = GenerateMultiFrameWavetable(startFrame, endFrame, numFrames, morphCurve);

			// Apply effects to each frame
			for (int frame = 0; frame < numFrames; ++frame) {
				std::vector<float> frameSamples(wavetable.begin() + frame * SAMPLES_PER_WAVE,
					wavetable.begin() + (frame + 1) * SAMPLES_PER_WAVE);
				WaveformEffects::ApplyEffects(frameSamples, effects);
				std::copy(frameSamples.begin(), frameSamples.end(), wavetable.begin() + frame * SAMPLES_PER_WAVE);
			}

			// Re-normalize globally after effects
			NormalizeSamples(wavetable);

			return wavetable;
		}

		// Generate single-frame wavetable
		std::vector<float> WaveGenerator::GenerateSingleFrameWavetable(const std::vector<std::pair<WaveType, float>>& startWaves,
			const EffectsSettings& effects) {
			std::vector<float> combined = CombineWaves(startWaves, SAMPLES_PER_WAVE);

			// Apply effects
			WaveformEffects::ApplyEffects(combined, effects);

			NormalizeSamples(combined);
			return combined;
		}

		// Generate a wavetable with the specified parameters (bandlimited)
		GenerationResult WaveGenerator::GenerateWavetable(
			const std::vector<std::pair<WaveType, float>>& startWaves,
			const std::vector<std::pair<WaveType, float>>& endWaves,
			const std::string& filename,
			OutputFormat format,
			bool isAudioPreview,
			bool enableMorphing,
			int numFrames,
			const EffectsSettings& effects,
			MorphCurve morphCurve,
			double pulseDuty,
			int maxHarmonics
		) {
			if (startWaves.empty()) {
				return GenerationResult::ErrorEmptyWaveforms;
			}

			// Store pulse duty cycle for PWM waves
			m_pulseDuty = pulseDuty;

			// Store max harmonics for harmonic waveforms
			m_maxHarmonics = maxHarmonics;

			std::vector<float> samples;

			// Generate audio preview (always writes WAV format)
			if (isAudioPreview) {
				samples = GenerateAudioPreview(startWaves, effects);
				auto writer = FileWriterFactory::Create(OutputFormat::WAV);
				return writer->Write(filename, samples, 1, SAMPLE_RATE);
			}

			// Generate wavetable data
			if (enableMorphing) {
				samples = GenerateMorphingWavetable(startWaves, endWaves, numFrames, effects, morphCurve);
			}
			else {
				samples = GenerateSingleFrameWavetable(startWaves, effects);
				numFrames = 1;
			}

			// Write in the requested format using Strategy Pattern
			auto writer = FileWriterFactory::Create(format);
			return writer->Write(filename, samples, numFrames, SAMPLE_RATE);
		}

		// Generate filename from waveform settings (including effects)
		std::string WaveGenerator::GenerateFilenameFromSettings(
			const std::vector<std::pair<WaveType, float>>& startWaves,
			const std::vector<std::pair<WaveType, float>>& endWaves,
			bool enableMorphing,
			const EffectsSettings& effects,
			MorphCurve morphCurve,
			double pulseDuty) {
			if (startWaves.empty()) {
				return "empty";
			}

			std::ostringstream filename;

			bool first = true;
			for (const auto& wave : startWaves) {
				if (!first) {
					filename << "_";
				}
				first = false;

				int percentage = static_cast<int>(wave.second * 100.0f + 0.5f);
				filename << WaveTypeName::Get(wave.first) << percentage;
			}

			if (enableMorphing && !endWaves.empty()) {
				filename << "_to_";

				first = true;
				for (const auto& wave : endWaves) {
					if (!first) {
						filename << "_";
					}
					first = false;

					int percentage = static_cast<int>(wave.second * 100.0f + 0.5f);
					filename << WaveTypeName::Get(wave.first) << percentage;
				}
			}

			// Add effects information (only if non-default)

			// PWM duty cycle (only if not 50%)
			if (std::abs(pulseDuty - 0.5) > 0.01) {
				int dutyPercent = static_cast<int>(pulseDuty * 100.0f + 0.5f);
				filename << "_PWM" << dutyPercent;
			}

			// Morph curve (only if not linear)
			if (morphCurve != MorphCurve::Linear) {
				filename << "_Curve";
				switch (morphCurve) {
				case MorphCurve::Exponential: filename << "Exp"; break;
				case MorphCurve::Logarithmic: filename << "Log"; break;
				case MorphCurve::SCurve: filename << "S"; break;
				default: break;
				}
			}

			// Distortion
			if (effects.distortionType != DistortionType::None && effects.distortionAmount > 0.0f) {
				filename << "_Dist";
				switch (effects.distortionType) {
				case DistortionType::Soft: filename << "Soft"; break;
				case DistortionType::Hard: filename << "Hard"; break;
				case DistortionType::Asymmetric: filename << "Asym"; break;
				default: break;
				}
				int distAmount = static_cast<int>(effects.distortionAmount * 100.0f + 0.5f);
				filename << distAmount;
			}

			// Filters
			if (effects.enableLowPass) {
				int cutoff = static_cast<int>(effects.lowPassCutoff * 100.0f + 0.5f);
				filename << "_LP" << cutoff;
			}
			if (effects.enableHighPass) {
				int cutoff = static_cast<int>(effects.highPassCutoff * 100.0f + 0.5f);
				filename << "_HP" << cutoff;
			}

			// Bit crushing
			if (effects.enableBitCrush && effects.bitDepth < 16) {
				filename << "_BC" << effects.bitDepth;
			}

			// Wavefold
			if (effects.enableWavefold && effects.wavefoldAmount > 0.0f) {
				int wfAmount = static_cast<int>(effects.wavefoldAmount * 100.0f + 0.5f);
				filename << "_WF" << wfAmount;
			}

			// Symmetry operations
			if (effects.mirrorHorizontal) {
				filename << "_MirrorH";
			}
			if (effects.mirrorVertical) {
				filename << "_MirrorV";
			}
			if (effects.invert) {
				filename << "_Invert";
			}
			if (effects.reverse) {
				filename << "_Reverse";
			}

			return filename.str();
		}

		// Analyze an imported frame and find best matching waveforms using correlation
		std::vector<std::pair<WaveType, float>> WaveGenerator::AnalyzeFrame(const std::vector<float>& frameData) {
			if (frameData.empty()) {
				return {};
			}

			// Resample/normalize to our standard sample count if needed
			std::vector<float> normalizedFrame;
			if (frameData.size() != SAMPLES_PER_WAVE) {
				normalizedFrame.resize(SAMPLES_PER_WAVE);
				for (size_t i = 0; i < SAMPLES_PER_WAVE; ++i) {
					double srcPos = (double)i * frameData.size() / SAMPLES_PER_WAVE;
					size_t idx = (size_t)srcPos;
					if (idx >= frameData.size() - 1) {
						normalizedFrame[i] = frameData.back();
					}
					else {
						// Linear interpolation
						double frac = srcPos - idx;
						normalizedFrame[i] = (float)((1.0 - frac) * frameData[idx] + frac * frameData[idx + 1]);
					}
				}
			}
			else {
				normalizedFrame = frameData;
			}

			// Normalize the frame to [-1, 1] range
			float maxAbs = 0.0f;
			for (float sample : normalizedFrame) {
				maxAbs = std::max(maxAbs, std::abs(sample));
			}
			if (maxAbs > 0.0f) {
				for (float& sample : normalizedFrame) {
					sample /= maxAbs;
				}
			}

			// List of all waveform types to test
			std::vector<WaveType> allWaveTypes = {
				WaveType::Sine, WaveType::Square, WaveType::Triangle, WaveType::Saw,
				WaveType::ReverseSaw, WaveType::Pulse, WaveType::OddHarmonics,
				WaveType::EvenHarmonics, WaveType::HarmonicSeries, WaveType::SubHarmonics,
				WaveType::Formant, WaveType::Additive, WaveType::SimpleFM,
				WaveType::ComplexFM, WaveType::PhaseDistortion, WaveType::Wavefold,
				WaveType::HardSync, WaveType::Chebyshev, WaveType::String,
				WaveType::Brass, WaveType::Reed, WaveType::Vocal, WaveType::Bell,
				WaveType::Supersaw, WaveType::PWMSaw, WaveType::Parabolic,
				WaveType::DoubleSine, WaveType::HalfSine, WaveType::Trapezoid,
				WaveType::Power, WaveType::Exponential, WaveType::Logistic,
				WaveType::Stepped, WaveType::Noise, WaveType::Procedural
			};

			// Calculate correlation for each waveform type
			std::vector<std::pair<WaveType, float>> correlations;
			for (WaveType type : allWaveTypes) {
				// Generate reference waveform (using default harmonics=8 for analysis)
				std::vector<float> refWave = GenerateWave(type, SAMPLES_PER_WAVE, 0.5, 8);

				// Normalize reference wave
				float refMaxAbs = 0.0f;
				for (float sample : refWave) {
					refMaxAbs = std::max(refMaxAbs, std::abs(sample));
				}
				if (refMaxAbs > 0.0f) {
					for (float& sample : refWave) {
						sample /= refMaxAbs;
					}
				}

				// Calculate correlation (dot product of normalized signals)
				float correlation = 0.0f;
				for (size_t i = 0; i < SAMPLES_PER_WAVE; ++i) {
					correlation += normalizedFrame[i] * refWave[i];
				}
				correlation /= SAMPLES_PER_WAVE; // Average

				// Use absolute correlation (we care about similarity, not phase)
				float absCorrelation = std::abs(correlation);

				// Only include if correlation is significant
				if (absCorrelation > 0.1f) {
					correlations.push_back({ type, absCorrelation });
				}
			}

			// Sort by correlation (highest first)
			std::sort(correlations.begin(), correlations.end(),
				[](const auto& a, const auto& b) { return a.second > b.second; });

			// Take top 3-5 matches
			std::vector<std::pair<WaveType, float>> result;
			int maxMatches = std::min(5, (int)correlations.size());

			// Normalize weights so they sum to 1.0
			float totalWeight = 0.0f;
			for (int i = 0; i < maxMatches; ++i) {
				totalWeight += correlations[i].second;
			}

			if (totalWeight > 0.0f) {
				for (int i = 0; i < maxMatches; ++i) {
					float normalizedWeight = correlations[i].second / totalWeight;
					result.push_back({ correlations[i].first, normalizedWeight });
				}
			}

			// If no good matches found, default to sine wave
			if (result.empty()) {
				result.push_back({ WaveType::Sine, 1.0f });
			}

			return result;
		}

		// Analyze an imported frame using spectral matching (frequency domain)
		std::vector<std::pair<WaveType, float>> WaveGenerator::AnalyzeFrameSpectral(const std::vector<float>& frameData) {
			if (frameData.empty()) {
				return {};
			}

			// Resample/normalize to our standard sample count if needed
			std::vector<float> normalizedFrame;
			if (frameData.size() != SAMPLES_PER_WAVE) {
				normalizedFrame.resize(SAMPLES_PER_WAVE);
				for (size_t i = 0; i < SAMPLES_PER_WAVE; ++i) {
					double srcPos = (double)i * frameData.size() / SAMPLES_PER_WAVE;
					size_t idx = (size_t)srcPos;
					if (idx >= frameData.size() - 1) {
						normalizedFrame[i] = frameData.back();
					}
					else {
						// Linear interpolation
						double frac = srcPos - idx;
						normalizedFrame[i] = (float)((1.0 - frac) * frameData[idx] + frac * frameData[idx + 1]);
					}
				}
			}
			else {
				normalizedFrame = frameData;
			}

			// Normalize the frame to [-1, 1] range
			float maxAbs = 0.0f;
			for (float sample : normalizedFrame) {
				maxAbs = std::max(maxAbs, std::abs(sample));
			}
			if (maxAbs > 0.0f) {
				for (float& sample : normalizedFrame) {
					sample /= maxAbs;
				}
			}

			// Get magnitude spectrum of imported frame
			static std::shared_ptr<DSP::IFrequencyProcessor> fftProcessor =
				std::make_shared<DSP::KissFFTProcessor>(SAMPLES_PER_WAVE);

			std::vector<DSP::FrequencyBin> importedSpectrum;
			fftProcessor->Forward(normalizedFrame, importedSpectrum);

			// List of all waveform types to test
			std::vector<WaveType> allWaveTypes = {
				WaveType::Sine, WaveType::Square, WaveType::Triangle, WaveType::Saw,
				WaveType::ReverseSaw, WaveType::Pulse, WaveType::OddHarmonics,
				WaveType::EvenHarmonics, WaveType::HarmonicSeries, WaveType::SubHarmonics,
				WaveType::Formant, WaveType::Additive, WaveType::SimpleFM,
				WaveType::ComplexFM, WaveType::PhaseDistortion, WaveType::Wavefold,
				WaveType::HardSync, WaveType::Chebyshev, WaveType::String,
				WaveType::Brass, WaveType::Reed, WaveType::Vocal, WaveType::Bell,
				WaveType::Supersaw, WaveType::PWMSaw, WaveType::Parabolic,
				WaveType::DoubleSine, WaveType::HalfSine, WaveType::Trapezoid,
				WaveType::Power, WaveType::Exponential, WaveType::Logistic,
				WaveType::Stepped, WaveType::Noise, WaveType::Procedural
			};

			// Calculate spectral distance for each waveform type
			std::vector<std::pair<WaveType, float>> spectralMatches;

			for (WaveType type : allWaveTypes) {
				// Generate reference waveform
				std::vector<float> refWave = GenerateWave(type, SAMPLES_PER_WAVE, 0.5, 8);

				// Normalize reference wave
				float refMaxAbs = 0.0f;
				for (float sample : refWave) {
					refMaxAbs = std::max(refMaxAbs, std::abs(sample));
				}
				if (refMaxAbs > 0.0f) {
					for (float& sample : refWave) {
						sample /= refMaxAbs;
					}
				}

				// Get magnitude spectrum of reference
				std::vector<DSP::FrequencyBin> refSpectrum;
				fftProcessor->Forward(refWave, refSpectrum);

				// Calculate spectral distance (Euclidean distance of magnitude spectra)
				// Focus on lower bins (more perceptually relevant)
				float distance = 0.0f;
				int numBinsToCompare = std::min((int)importedSpectrum.size(), 512); // Compare first 512 bins

				for (int i = 1; i < numBinsToCompare; ++i) { // Skip DC component
					float diff = importedSpectrum[i].magnitude - refSpectrum[i].magnitude;
					distance += diff * diff;

					// Weight lower frequencies more heavily (more perceptually important)
					float weight = 1.0f / (1.0f + (float)i * 0.01f);
					distance += diff * diff * weight;
				}

				distance = std::sqrt(distance);

				// Convert distance to similarity (closer = more similar)
				// Using exponential decay for better discrimination
				float similarity = std::exp(-distance * 0.05f);

				if (similarity > 0.05f) { // Only include reasonable matches
					spectralMatches.push_back({ type, similarity });
				}
			}

			// Sort by similarity (highest first)
			std::sort(spectralMatches.begin(), spectralMatches.end(),
				[](const auto& a, const auto& b) { return a.second > b.second; });

			// Take top 3-5 matches
			std::vector<std::pair<WaveType, float>> result;
			int maxMatches = std::min(5, (int)spectralMatches.size());

			// Normalize weights so they sum to 1.0
			float totalWeight = 0.0f;
			for (int i = 0; i < maxMatches; ++i) {
				totalWeight += spectralMatches[i].second;
			}

			if (totalWeight > 0.0f) {
				for (int i = 0; i < maxMatches; ++i) {
					float normalizedWeight = spectralMatches[i].second / totalWeight;
					result.push_back({ spectralMatches[i].first, normalizedWeight });
				}
			}

			// If no good matches found, default to sine wave
			if (result.empty()) {
				result.push_back({ WaveType::Sine, 1.0f });
			}

			return result;
		}
	}
}