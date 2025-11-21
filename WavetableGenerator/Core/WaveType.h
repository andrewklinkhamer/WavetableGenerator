#ifndef WAVETYPE_H
#define WAVETYPE_H

namespace WavetableGen {
	namespace Core {
		enum class WaveType {
			// TAB 0: Basic Waves
			Sine = 0,
			Square,
			Triangle,
			Saw,
			ReverseSaw,
			Pulse,

			// TAB 1: Chaos Theory
			Lorenz,
			Rossler,
			Henon,
			Duffing,
			Chua,
			LogisticChaos,

			// TAB 2: Fractals
			Weierstrass,
			Cantor,
			Koch,
			Mandelbrot,

			// TAB 3: Harmonic Waves
			OddHarmonics,
			EvenHarmonics,
			HarmonicSeries,
			SubHarmonics,
			Formant,
			Additive,

			// TAB 4: Inharmonic Series
			StretchedHarm,
			CompressedHarm,
			Metallic,
			Clangorous,
			KarplusStrong,
			StiffString,

			// TAB 5: Modern/Digital + Mathematical
			Supersaw,
			PWMSaw,
			Parabolic,
			DoubleSine,
			HalfSine,
			Trapezoid,
			Power,
			Exponential,
			Logistic,
			Stepped,
			Noise,
			Procedural,
			Sinc,

			// TAB 6: Modulation Synthesis
			RingMod,
			AmplitudeMod,
			FrequencyMod,
			CrossMod,
			PhaseMod,

			// TAB 7: Physical Models
			String,
			Brass,
			Reed,
			Vocal,
			Bell,

			// TAB 8: Synthesis Waves
			SimpleFM,
			ComplexFM,
			PhaseDistortion,
			Wavefold,
			HardSync,
			Chebyshev,

			// TAB 9: Vintage Synth Emulation
			ARPOdyssey,
			CS80,
			Juno,
			MiniMoog,
			MS20,
			Oberheim,
			PPG,
			Prophet5,
			TB303,

			// TAB 10: Vowel Formants
			VowelA,
			VowelE,
			VowelI,
			VowelO,
			VowelU,
			Diphthong
		};
	}
}

#endif // WAVETYPE_H
