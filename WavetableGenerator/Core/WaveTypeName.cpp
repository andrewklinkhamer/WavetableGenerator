#include "WaveTypeName.h"

namespace WavetableGen {
	namespace Core {
		const char* WaveTypeName::Get(WaveType type) {
			switch (type) {
				// TAB 0: Basic Waves
			case WaveType::Sine: return "Sine";
			case WaveType::Square: return "Square";
			case WaveType::Triangle: return "Triangle";
			case WaveType::Saw: return "Saw";
			case WaveType::ReverseSaw: return "ReverseSaw";
			case WaveType::Pulse: return "Pulse";

				// TAB 1: Chaos Theory
			case WaveType::Lorenz: return "Lorenz";
			case WaveType::Rossler: return "Rossler";
			case WaveType::Henon: return "Henon";
			case WaveType::Duffing: return "Duffing";
			case WaveType::Chua: return "Chua";
			case WaveType::LogisticChaos: return "LogisticChaos";

				// TAB 2: Fractals
			case WaveType::Weierstrass: return "Weierstrass";
			case WaveType::Cantor: return "Cantor";
			case WaveType::Koch: return "Koch";
			case WaveType::Mandelbrot: return "Mandelbrot";

				// TAB 3: Harmonic Waves
			case WaveType::OddHarmonics: return "OddHarmonics";
			case WaveType::EvenHarmonics: return "EvenHarmonics";
			case WaveType::HarmonicSeries: return "HarmonicSeries";
			case WaveType::SubHarmonics: return "SubHarmonics";
			case WaveType::Formant: return "Formant";
			case WaveType::Additive: return "Additive";

				// TAB 4: Inharmonic Series
			case WaveType::StretchedHarm: return "StretchedHarm";
			case WaveType::CompressedHarm: return "CompressedHarm";
			case WaveType::Metallic: return "Metallic";
			case WaveType::Clangorous: return "Clangorous";
			case WaveType::KarplusStrong: return "KarplusStrong";
			case WaveType::StiffString: return "StiffString";

				// TAB 5: Modern/Digital + Mathematical
			case WaveType::Supersaw: return "Supersaw";
			case WaveType::PWMSaw: return "PWMSaw";
			case WaveType::Parabolic: return "Parabolic";
			case WaveType::DoubleSine: return "DoubleSine";
			case WaveType::HalfSine: return "HalfSine";
			case WaveType::PhaseMod: return "PhaseMod";

				// TAB 7: Physical Models
			case WaveType::String: return "String";
			case WaveType::Brass: return "Brass";
			case WaveType::Reed: return "Reed";
			case WaveType::Vocal: return "Vocal";
			case WaveType::Bell: return "Bell";

				// TAB 8: Synthesis Waves
			case WaveType::SimpleFM: return "SimpleFM";
			case WaveType::ComplexFM: return "ComplexFM";
			case WaveType::PhaseDistortion: return "PhaseDist";
			case WaveType::Wavefold: return "Wavefold";
			case WaveType::HardSync: return "HardSync";
			case WaveType::Chebyshev: return "Chebyshev";

				// TAB 9: Vintage Synth Emulation (Alphabetical)
			case WaveType::ARPOdyssey: return "ARPOdyssey";
			case WaveType::CS80: return "CS80";
			case WaveType::Juno: return "Juno";
			case WaveType::MiniMoog: return "MiniMoog";
			case WaveType::MS20: return "MS20";
			case WaveType::Oberheim: return "Oberheim";
			case WaveType::PPG: return "PPG";
			case WaveType::Prophet5: return "Prophet5";
			case WaveType::TB303: return "TB303";

				// TAB 10: Vowel Formants
			case WaveType::VowelA: return "VowelA";
			case WaveType::VowelE: return "VowelE";
			case WaveType::VowelI: return "VowelI";
			case WaveType::VowelO: return "VowelO";
			case WaveType::VowelU: return "VowelU";
			case WaveType::Diphthong: return "Diphthong";

			default: return "Unknown";
			}
		}
	} // namespace Core
} // namespace WavetableGen