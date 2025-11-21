#include <windows.h>
#include "Core/WaveGenerator.h"
#include "UI/WinApplication.h"

using namespace WavetableGen;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
	// Create wave generator instance
	Core::WaveGenerator waveGenerator;

	// Create and run application
	UI::WinApplication app(waveGenerator, hInstance);
	return app.Run(nCmdShow);
}