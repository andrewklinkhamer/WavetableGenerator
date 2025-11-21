#ifndef INCLUDE_H
#define INCLUDE_H

#include <windows.h>
#include <commdlg.h>
#include <vector>
#include <string>
#include <commctrl.h>
#include <uxtheme.h>
#include "../Core/WaveGenerator.h"

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

// To ensure we load the x64 versions in 64 bit builds we need to check our manifest dependency for Microsoft.Windows.Common-Controls.
// The 64-bit version of it must have processorArchitecture='amd64'.  We'll load the wrong one if it is still 'x86'.
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#endif // INCLUDE_H
