#ifndef XORSHIFT128PLUS_H
#define XORSHIFT128PLUS_H

#include <cstdint>
#include <chrono>

namespace WavetableGen {
	namespace Utils {
		// XorShift128+ - Fast, high-quality pseudo-random number generator
		class XorShift128Plus {
		public:
			// Constructor with optional seed
			explicit XorShift128Plus(uint64_t seed = 0) {
				if (seed == 0) {
					// Use current time as seed
					seed = static_cast<uint64_t>(
						std::chrono::high_resolution_clock::now().time_since_epoch().count()
						);
				}

				// Initialize state with seed (using splitmix64 algorithm)
				m_state[0] = splitmix64(seed);
				m_state[1] = splitmix64(m_state[0]);
			}

			// Generate next random uint64_t
			uint64_t Next() {
				uint64_t s1 = m_state[0];
				const uint64_t s0 = m_state[1];
				m_state[0] = s0;
				s1 ^= s1 << 23;
				m_state[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
				return m_state[1] + s0;
			}

			// Generate random integer in range [min, max] (inclusive)
			int NextInt(int min, int max) {
				if (min >= max) return min;
				uint64_t range = static_cast<uint64_t>(max - min + 1);
				return min + static_cast<int>(Next() % range);
			}

			// Generate random float in range [0.0, 1.0]
			float NextFloat() {
				return static_cast<float>(Next() & 0xFFFFFF) / static_cast<float>(0x1000000);
			}

			// Generate random float in range [min, max]
			float NextFloat(float min, float max) {
				return min + NextFloat() * (max - min);
			}

			// Generate random bool with given probability (0.0 to 1.0)
			bool NextBool(float probability = 0.5f) {
				return NextFloat() < probability;
			}

		private:
			uint64_t m_state[2];

			// Splitmix64 for seed initialization
			static uint64_t splitmix64(uint64_t x) {
				x += 0x9e3779b97f4a7c15;
				x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
				x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
				return x ^ (x >> 31);
			}
		};
	}
}

#endif // XORSHIFT128PLUS_H
