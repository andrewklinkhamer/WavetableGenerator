#ifndef WAVETYPENAME_H
#define WAVETYPENAME_H

#include "WaveType.h"

namespace WavetableGen {
	namespace Core {
		// Utility class for converting WaveType enum values to string names
		class WaveTypeName {
		public:
			// Get the string name for a WaveType enum value
			static const char* Get(WaveType type);
		};
	}
}

#endif // WAVETYPENAME_H
