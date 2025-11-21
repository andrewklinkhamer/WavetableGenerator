#include "WinApplication.h"
#include "../Resources/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <io.h>

// Command IDs for UI controls (in GUI order: bottom controls, then Settings tab)
#define CMD_SELECT_ALL           1
#define CMD_RANDOMIZE_WAVEFORMS  2
#define CMD_CLEAR_ALL            3
#define CMD_RESET_SLIDERS        4
#define CMD_GENERATE             5
#define CMD_EXIT                 6
#define CMD_BROWSE_FOLDER        7
#define CMD_AUDIO_PREVIEW        8
#define CMD_ENABLE_MORPHING      9
#define CMD_IMPORT_WAVETABLE    10
#define CMD_USE_AS_START        11
#define CMD_USE_AS_END          12
#define CMD_CLEAR_IMPORT        13

namespace WavetableGen {
	namespace UI {
		using namespace Core;
		using namespace Services;
		using namespace Utils;

		WinApplication::WinApplication(IWavetableGenerator& wavetableGenerator, HINSTANCE hInstance)
			: m_wavetableGenerator(wavetableGenerator), m_hInstance(hInstance), m_hwnd(nullptr),
			m_randomGenerator(wavetableGenerator, m_rng),
			m_hWorkerThread(nullptr), m_dwWorkerThreadId(0), m_bCancelGeneration(false)
		{
			// Create font
			m_hFont = CreateFont(
				16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI"
			);
		}

		WinApplication::~WinApplication() {
			// Thread should already be cleaned up by WM_CLOSE handler
			// But just in case, clean up any remaining resources
			if (m_hWorkerThread) {
				m_bCancelGeneration = true;
				WaitForSingleObject(m_hWorkerThread, INFINITE);
				CloseHandle(m_hWorkerThread);
			}
			if (m_hFont) DeleteObject(m_hFont);
		}

		int WinApplication::Run(int nCmdShow) {
			const wchar_t CLASS_NAME[] = L"WavetableGeneratorWindow";

			INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES | ICC_TAB_CLASSES };
			InitCommonControlsEx(&icex);

			WNDCLASS wc = {};
			wc.lpfnWndProc = WndProc;
			wc.hInstance = m_hInstance;
			wc.lpszClassName = CLASS_NAME;
			wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			wc.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_APPICON));
			wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

			RegisterClass(&wc);

			// Calculate centered position
			int windowWidth = 686;
			int windowHeight = 558; // Compact layout with tabs and bottom controls
			int screenWidth = GetSystemMetrics(SM_CXSCREEN);
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);
			int xPos = (screenWidth - windowWidth) / 2;
			int yPos = (screenHeight - windowHeight) / 2;

			m_hwnd = CreateWindowEx(
				0, CLASS_NAME, L"Wavetable Generator",
				WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX,
				xPos, yPos, windowWidth, windowHeight,
				nullptr, nullptr, m_hInstance, this  // Pass 'this' pointer
			);

			if (!m_hwnd) {
				return -1;
			}

			ShowWindow(m_hwnd, nCmdShow);
			UpdateWindow(m_hwnd);

			MSG msg = {};
			while (GetMessage(&msg, nullptr, 0, 0)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			return (int)msg.wParam;
		}

		LRESULT CALLBACK WinApplication::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
			WinApplication* pThis = nullptr;

			if (msg == WM_NCCREATE) {
				// Retrieve the 'this' pointer from CREATESTRUCT and store it
				CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
				pThis = reinterpret_cast<WinApplication*>(pCreate->lpCreateParams);
				SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
			}
			else {
				// Retrieve the stored 'this' pointer
				pThis = reinterpret_cast<WinApplication*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			}

			if (pThis) {
				return pThis->HandleMessage(hwnd, msg, wParam, lParam);
			}

			return DefWindowProc(hwnd, msg, wParam, lParam);
		}

		LRESULT WinApplication::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
			switch (msg) {
			case WM_CREATE: {
				// Create UI components
				CreateTabControl(hwnd);
				CreateTabPages(hwnd);
				AddColumnHeaders();

				// Create content for each tab
				CreateBasicTab();
				CreateChaosTab();
				CreateFractalsTab();
				CreateHarmonicTab();
				CreateInharmonicTab();
				CreateModernTab();
				CreateModulationTab();
				CreatePhysicalTab();
				CreateSynthesisTab();
				CreateVintageTab();
				CreateVowelsTab();
				CreateImportTab();
				CreateEffectsTab();
				CreateSpectralEffectsTab();
				CreateSettingsTab();

				// Create bottom controls
				CreateBottomControls(hwnd);

				// Set fonts and load settings
				SetFontForAllChildren(hwnd);
				LoadSettings();
			}
						  break;

			case WM_CTLCOLORSTATIC: {
				HDC hdc = (HDC)wParam;
				HWND hControl = (HWND)lParam;
				SetBkMode(hdc, TRANSPARENT);
				SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
				return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
			}

			case WM_CTLCOLORBTN: {
				HDC hdc = (HDC)wParam;
				SetBkMode(hdc, TRANSPARENT);
				return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
			}

			case WM_HSCROLL: {
				// Handle slider movement to update labels
				HWND hControl = (HWND)lParam;
				if (hControl) {
					int pos = (int)SendMessage(hControl, TBM_GETPOS, 0, 0);

					if (hControl == m_hSliderPWMDuty) {
						UpdatePWMDutyLabel(pos);
					}
					else if (hControl == m_hSliderDistortionAmount) {
						UpdateDistortionAmountLabel(pos);
					}
					else if (hControl == m_hSliderLowPassCutoff) {
						UpdateLowPassCutoffLabel(pos);
					}
					else if (hControl == m_hSliderHighPassCutoff) {
						UpdateHighPassCutoffLabel(pos);
					}
					else if (hControl == m_hSliderBitDepth) {
						UpdateBitDepthLabel(pos);
					}
					else if (hControl == m_hSliderWavefold) {
						UpdateWavefoldAmountLabel(pos);
					}
					else if (hControl == m_hSliderSpectralDecayAmount) {
						UpdateSpectralDecayAmountLabel(pos);
					}
					else if (hControl == m_hSliderSpectralDecayCurve) {
						UpdateSpectralDecayCurveLabel(pos);
					}
					else if (hControl == m_hSliderSpectralTilt) {
						UpdateSpectralTiltLabel(pos);
					}
					else if (hControl == m_hSliderSpectralGate) {
						UpdateSpectralGateLabel(pos);
					}
					else if (hControl == m_hSliderPhaseRandomize) {
						UpdatePhaseRandomizeLabel(pos);
					}
					else if (hControl == m_hSliderMaxHarmonics) {
						UpdateMaxHarmonicsLabel(pos);
					}
					else if (hControl == m_hSliderSampleRateReduction) {
						UpdateSampleRateReductionLabel(pos);
					}
					else if (hControl == m_hSliderSpectralShift) {
						UpdateSpectralShiftLabel(pos);
					}
				}
				break;
			}

			case WM_NOTIFY: {
				LPNMHDR pnmhdr = (LPNMHDR)lParam;
				if (pnmhdr->hwndFrom == m_hTabControl && pnmhdr->code == TCN_SELCHANGE) {
					// Hide all tab pages
					for (int i = 0; i < 15; ++i) {
						ShowWindow(m_hTabPage[i], SW_HIDE);
					}

					// Show the selected tab page
					int selectedTab = TabCtrl_GetCurSel(m_hTabControl);
					if (selectedTab >= 0 && selectedTab < 15) {
						ShowWindow(m_hTabPage[selectedTab], SW_SHOW);
					}
				}
				break;
			}

			case WM_COMMAND: {
				switch (LOWORD(wParam)) {
				case CMD_BROWSE_FOLDER: {
					wchar_t folderPath[MAX_PATH] = {};

					BROWSEINFO bi = {};
					bi.hwndOwner = hwnd;
					bi.lpszTitle = L"Select Output Folder";
					bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

					LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
					if (pidl) {
						if (SHGetPathFromIDList(pidl, folderPath)) {
							SetWindowText(m_hEditPath, folderPath);
						}
						CoTaskMemFree(pidl);
					}
					break;
				}

				case CMD_GENERATE: {
					// Check if worker thread is running - if so, this is a Cancel request
					if (m_hWorkerThread != nullptr) {
						HandleCancelRequest();
						return 0; // Message handled
					}

					// Get folder path
					std::string folderPath;
					if (!GetFolderPath(folderPath)) {
						ErrorMessage(hwnd, L"Invalid folder path!");
						break;
					}

					// Get generation count
					int count = GetGenerationCount();

					// Generate single wavetable or start batch generation
					if (count == 1) {
						GenerateSingleWavetable(folderPath);
					}
					else {
						StartBatchGeneration(folderPath, count);
					}
					break;
				}

				case CMD_SELECT_ALL: {
					for (auto& wc : m_waveCheckboxes) {
						SendMessage(wc.hwnd, BM_SETCHECK, BST_CHECKED, 0);
					}
					SetWindowText(m_hStatus, L"All waveforms selected");
					break;
				}

				case CMD_CLEAR_ALL: {
					for (auto& wc : m_waveCheckboxes) {
						SendMessage(wc.hwnd, BM_SETCHECK, BST_UNCHECKED, 0);
					}
					SetWindowText(m_hStatus, L"All waveforms cleared");
					break;
				}

				case CMD_RESET_SLIDERS: {
					for (auto& wc : m_waveCheckboxes) {
						// Reset both start and end sliders to 100%
						SendMessage(wc.hSliderStart, TBM_SETPOS, TRUE, 100);
						SendMessage(wc.hSliderEnd, TBM_SETPOS, TRUE, 100);
					}
					SetWindowText(m_hStatus, L"All sliders reset to 100%");
					break;
				}

				case CMD_RANDOMIZE_WAVEFORMS: {
					// First, reset everything: clear all checkboxes and reset all sliders to 100%
					for (auto& wc : m_waveCheckboxes) {
						SendMessage(wc.hwnd, BM_SETCHECK, BST_UNCHECKED, 0);
						SendMessage(wc.hSliderStart, TBM_SETPOS, TRUE, 100);
						SendMessage(wc.hSliderEnd, TBM_SETPOS, TRUE, 100);
					}

					// Determine how many waveforms to select (between 1 and 8)
					int totalWaveforms = (int)m_waveCheckboxes.size();
					int numToSelect = (m_rng.Next() % 8) + 1; // Random 1-8 waveforms
					if (numToSelect > totalWaveforms) numToSelect = totalWaveforms;

					// Create a list of indices and shuffle them
					std::vector<int> indices;
					for (int i = 0; i < totalWaveforms; ++i) {
						indices.push_back(i);
					}

					// Fisher-Yates shuffle
					for (int i = totalWaveforms - 1; i > 0; --i) {
						int j = m_rng.Next() % (i + 1);
						std::swap(indices[i], indices[j]);
					}

					// Select the first numToSelect waveforms and randomize their sliders
					for (int i = 0; i < numToSelect; ++i) {
						auto& wc = m_waveCheckboxes[indices[i]];

						// Check the checkbox
						SendMessage(wc.hwnd, BM_SETCHECK, BST_CHECKED, 0);

						// Set random slider values (0-100)
						int startVal = m_rng.Next() % 101;
						int endVal = m_rng.Next() % 101;
						SendMessage(wc.hSliderStart, TBM_SETPOS, TRUE, startVal);
						SendMessage(wc.hSliderEnd, TBM_SETPOS, TRUE, endVal);
					}

					wchar_t msg[64];
					swprintf_s(msg, L"Randomized %d waveforms", numToSelect);
					SetWindowText(m_hStatus, msg);
					break;
				}

				case CMD_AUDIO_PREVIEW: {
					// Enable/disable format dropdown based on Audio Preview checkbox
					bool isAudioPreview = (SendMessage(m_hChkAudioPreview, BM_GETCHECK, 0, 0) == BST_CHECKED);
					EnableWindow(m_hComboOutputFormat, !isAudioPreview);
					break;
				}

				case CMD_ENABLE_MORPHING: {
					// Enable/disable frames dropdown based on Enable Morphing checkbox
					bool enableMorphing = (SendMessage(m_hChkEnableMorphing, BM_GETCHECK, 0, 0) == BST_CHECKED);
					EnableWindow(m_hComboNumFrames, enableMorphing);
					break;
				}

				case CMD_IMPORT_WAVETABLE: {
					// Open file dialog to select wavetable
					OPENFILENAME ofn = {};
					wchar_t filename[MAX_PATH] = {};

					ofn.lStructSize = sizeof(OPENFILENAME);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFilter = L"Wavetable Files (*.wt;*.wav)\0*.wt;*.wav\0WT Files (*.wt)\0*.wt\0WAV Files (*.wav)\0*.wav\0All Files (*.*)\0*.*\0";
					ofn.lpstrFile = filename;
					ofn.nMaxFile = MAX_PATH;
					ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
					ofn.lpstrTitle = L"Import Wavetable";

					if (GetOpenFileName(&ofn)) {
						// Convert to narrow string
						char filenameA[MAX_PATH];
						WideCharToMultiByte(CP_UTF8, 0, filename, -1, filenameA, MAX_PATH, nullptr, nullptr);
						std::string filenameStr(filenameA);

						// Import the wavetable
						ImportResult result = m_importer.Import(filenameStr, m_importedWavetable);

						if (result == ImportResult::Success && m_importedWavetable.IsValid()) {
							// Display info
							wchar_t info[256];
							swprintf_s(info, L"%d frames, %d samples/frame, %d Hz",
								m_importedWavetable.numFrames,
								m_importedWavetable.samplesPerFrame,
								m_importedWavetable.sampleRate);
							SetWindowText(m_hLabelImportInfo, info);

							// Populate frame selector
							SendMessage(m_hComboImportedFrame, CB_RESETCONTENT, 0, 0);
							for (int i = 0; i < m_importedWavetable.numFrames; ++i) {
								wchar_t frameText[32];
								swprintf_s(frameText, L"Frame %d", i + 1);
								SendMessage(m_hComboImportedFrame, CB_ADDSTRING, 0, (LPARAM)frameText);
							}
							SendMessage(m_hComboImportedFrame, CB_SETCURSEL, 0, 0);

							// Enable controls
							EnableWindow(m_hComboImportedFrame, TRUE);
							EnableWindow(m_hBtnUseAsStart, TRUE);
							EnableWindow(m_hBtnUseAsEnd, TRUE);
							EnableWindow(m_hBtnClearImport, TRUE);

							SetWindowText(m_hStatus, L"Wavetable imported successfully!");
						}
						else {
							// Show error
							const char* errorMsg = WavetableImporter::GetErrorMessage(result);
							wchar_t errorMsgW[256];
							MultiByteToWideChar(CP_UTF8, 0, errorMsg, -1, errorMsgW, 256);
							ErrorMessage(hwnd, errorMsgW);
							SetWindowText(m_hStatus, L"Import failed");
						}
					}
					break;
				}

				case CMD_USE_AS_START: {
					// Get selected frame
					int frameIndex = (int)SendMessage(m_hComboImportedFrame, CB_GETCURSEL, 0, 0);
					if (frameIndex >= 0 && m_importedWavetable.IsValid()) {
						// Get the frame data
						std::vector<float> frameData = m_importedWavetable.GetFrame(frameIndex);

						if (!frameData.empty()) {
							// Analyze the frame using spectral matching (more accurate)
							auto matchedWaveforms = m_wavetableGenerator.AnalyzeFrameSpectral(frameData);

							// Apply to UI
							ApplyWaveformsToStartFrame(matchedWaveforms);

							SetWindowText(m_hStatus, L"Start frame updated (spectral analysis)");
						}
						else {
							SetWindowText(m_hStatus, L"Error: Could not read frame data");
						}
					}
					break;
				}

				case CMD_USE_AS_END: {
					// Get selected frame
					int frameIndex = (int)SendMessage(m_hComboImportedFrame, CB_GETCURSEL, 0, 0);
					if (frameIndex >= 0 && m_importedWavetable.IsValid()) {
						// Get the frame data
						std::vector<float> frameData = m_importedWavetable.GetFrame(frameIndex);

						if (!frameData.empty()) {
							// Analyze the frame using spectral matching (more accurate)
							auto matchedWaveforms = m_wavetableGenerator.AnalyzeFrameSpectral(frameData);

							// Apply to UI
							ApplyWaveformsToEndFrame(matchedWaveforms);

							SetWindowText(m_hStatus, L"End frame updated (spectral analysis)");
						}
						else {
							SetWindowText(m_hStatus, L"Error: Could not read frame data");
						}
					}
					break;
				}

				case CMD_CLEAR_IMPORT: {
					// Clear imported wavetable
					m_importedWavetable = ImportedWavetable(); // Reset to empty state

					// Reset UI
					SetWindowText(m_hLabelImportInfo, L"No wavetable loaded");
					SendMessage(m_hComboImportedFrame, CB_RESETCONTENT, 0, 0);

					// Disable controls
					EnableWindow(m_hComboImportedFrame, FALSE);
					EnableWindow(m_hBtnUseAsStart, FALSE);
					EnableWindow(m_hBtnUseAsEnd, FALSE);
					EnableWindow(m_hBtnClearImport, FALSE);

					SetWindowText(m_hStatus, L"Imported wavetable cleared");
					break;
				}

				case CMD_EXIT: {
					// Exit application
					PostMessage(hwnd, WM_CLOSE, 0, 0);
					break;
				}
				}
				break;
			}

						   // Custom window messages from worker thread
			case WM_GENERATION_PROGRESS: {
				int progress = (int)wParam;
				int generated = (int)lParam;

				// Update progress bar
				SendMessage(m_hProgressBar, PBM_SETPOS, progress, 0);
				return 0;
			}

			case WM_GENERATION_COMPLETE: {
				int count = (int)wParam;

				// Clean up thread handle
				if (m_hWorkerThread) {
					CloseHandle(m_hWorkerThread);
					m_hWorkerThread = nullptr;
				}

				// Re-enable controls
				EnableGenerationControls(true);

				// Show success message
				SetWindowText(m_hStatus, L"Done!");
				std::wstring msg = std::to_wstring(count) + L" random wavetables generated successfully!";
				InfoMessage(hwnd, msg.c_str());
				return 0;
			}

			case WM_GENERATION_ERROR: {
				// Clean up thread handle
				if (m_hWorkerThread) {
					CloseHandle(m_hWorkerThread);
					m_hWorkerThread = nullptr;
				}

				// Re-enable controls
				EnableGenerationControls(true);

				// Show cancellation message
				SetWindowText(m_hStatus, L"Cancelled");
				InfoMessage(hwnd, L"Generation cancelled by user.");
				return 0;
			}

			case WM_CLOSE: {
				// Check if generation is running
				if (m_hWorkerThread != nullptr) {
					// Ask user if they want to cancel and exit
					int result = MessageBox(hwnd,
						L"Wavetable generation is in progress.\nDo you want to cancel and exit?",
						L"Confirm Exit",
						MB_ICONQUESTION | MB_YESNO);

					if (result == IDYES) {
						// Cancel generation
						m_bCancelGeneration = true;

						// Wait for thread to finish (with timeout)
						SetWindowText(m_hStatus, L"Cancelling and exiting...");
						DWORD waitResult = WaitForSingleObject(m_hWorkerThread, 5000); // 5 second timeout

						if (waitResult == WAIT_TIMEOUT) {
							// Force terminate if it doesn't respond
							TerminateThread(m_hWorkerThread, 1);
						}

						CloseHandle(m_hWorkerThread);
						m_hWorkerThread = nullptr;

						// Auto-save settings before exit
						SaveSettings();

						// Now allow window to close
						DestroyWindow(hwnd);
					}
					// If user clicked No, don't close the window
					return 0;
				}
				else {
					// Auto-save settings before exit
					SaveSettings();

					// No generation running, close normally
					DestroyWindow(hwnd);
					return 0;
				}
			}

			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;
			}

			return DefWindowProc(hwnd, msg, wParam, lParam);
		}

		LRESULT CALLBACK WinApplication::PaneProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			switch (msg)
			{
			case WM_CTLCOLORSTATIC:
			{
				HDC hdc = (HDC)wParam;
				SetBkMode(hdc, TRANSPARENT);
				return (INT_PTR)GetSysColorBrush(COLOR_WINDOW); // matches tab page
			}
			case WM_HSCROLL:
			{
				// Forward WM_HSCROLL messages to the parent window
				HWND hParent = GetParent(hwnd);
				if (hParent) {
					return SendMessage(hParent, msg, wParam, lParam);
				}
				break;
			}
			case WM_COMMAND:
			{
				// Forward WM_COMMAND messages (button clicks, etc.) to the parent window
				HWND hParent = GetParent(hwnd);
				if (hParent) {
					return SendMessage(hParent, msg, wParam, lParam);
				}
				break;
			}
			}

			WNDPROC proc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			return CallWindowProc(proc, hwnd, msg, wParam, lParam);
		}

		std::vector<std::pair<WaveType, float>> WinApplication::GetStartFrameWaves() {
			std::vector<std::pair<WaveType, float>> selected;

			for (auto& wc : m_waveCheckboxes) {
				if (SendMessage(wc.hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED) {
					// Read start slider value (0-100) and convert to 0.0-1.0 range
					int sliderPos = (int)SendMessage(wc.hSliderStart, TBM_GETPOS, 0, 0);
					float weight = sliderPos / 100.0f;
					if (weight > 0.0f) {
						selected.push_back({ wc.type, weight });
					}
				}
			}

			return selected;
		}

		std::vector<std::pair<WaveType, float>> WinApplication::GetEndFrameWaves() {
			std::vector<std::pair<WaveType, float>> selected;

			for (auto& wc : m_waveCheckboxes) {
				if (SendMessage(wc.hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED) {
					// Read end slider value (0-100) and convert to 0.0-1.0 range
					int sliderPos = (int)SendMessage(wc.hSliderEnd, TBM_GETPOS, 0, 0);
					float weight = sliderPos / 100.0f;
					if (weight > 0.0f) {
						selected.push_back({ wc.type, weight });
					}
				}
			}

			return selected;
		}

		void WinApplication::SetFontForAllChildren(HWND hwndParent) const {
			EnumChildWindows(hwndParent, [](HWND hwnd, LPARAM lParam) -> BOOL {
				SendMessage(hwnd, WM_SETFONT, lParam, TRUE);
				return TRUE;
				}, (LPARAM)m_hFont);
		}

		void WinApplication::ErrorMessage(HWND hwnd, const wchar_t* msg) {
			MessageBox(hwnd, msg, L"Error", MB_ICONERROR);
		}

		void WinApplication::InfoMessage(HWND hwnd, const wchar_t* msg) {
			MessageBox(hwnd, msg, L"Info", MB_ICONINFORMATION);
		}

		const wchar_t* WinApplication::GetErrorMessage(GenerationResult result) {
			switch (result) {
			case GenerationResult::Success:
				return L"Success";
			case GenerationResult::ErrorEmptyWaveforms:
				return L"No waveforms selected. Please select at least one waveform.";
			case GenerationResult::ErrorFileOpenFailed:
				return L"Failed to open file for writing. Check permissions and disk space.";
			case GenerationResult::ErrorInvalidSampleCount:
				return L"Internal error: Invalid sample count generated.";
			case GenerationResult::ErrorAllSamplesZero:
				return L"Internal error: All generated samples are zero.";
			default:
				return L"Unknown error occurred.";
			}
		}

		// Build list of available waveforms from UI (only checked checkboxes)
		std::vector<RandomWavetableGenerator::AvailableWaveform> WinApplication::GetAvailableWaveforms() {
			std::vector<RandomWavetableGenerator::AvailableWaveform> availableWaveforms;
			for (const auto& wc : m_waveCheckboxes) {
				if (SendMessage(wc.hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED) {
					// Get min weight from start slider
					int startPos = (int)SendMessage(wc.hSliderStart, TBM_GETPOS, 0, 0);
					float minWeight = startPos / 100.0f;

					// Get max weight from end slider
					int endPos = (int)SendMessage(wc.hSliderEnd, TBM_GETPOS, 0, 0);
					float maxWeight = endPos / 100.0f;

					// Ensure min <= max
					if (minWeight > maxWeight) {
						std::swap(minWeight, maxWeight);
					}

					// Only include if at least one slider is above 0
					if (maxWeight > 0.0f) {
						availableWaveforms.push_back({ wc.type, minWeight, maxWeight });
					}
				}
			}

			return availableWaveforms;
		}

		// Get PWM duty cycle from UI
		double WinApplication::GetPWMDutyCycle() {
			int pos = (int)SendMessage(m_hSliderPWMDuty, TBM_GETPOS, 0, 0);
			return pos / 100.0; // Convert 1-99 to 0.01-0.99
		}

		// Get morph curve from UI
		MorphCurve WinApplication::GetMorphCurve() {
			int sel = (int)SendMessage(m_hComboMorphCurve, CB_GETCURSEL, 0, 0);
			switch (sel) {
			case 1: return MorphCurve::Exponential;
			case 2: return MorphCurve::Logarithmic;
			case 3: return MorphCurve::SCurve;
			default: return MorphCurve::Linear;
			}
		}

		// Get effects settings from UI
		EffectsSettings WinApplication::GetEffectsSettings() {
			EffectsSettings effects;

			// Distortion
			int distortionTypeIndex = GetComboBoxSelection(m_hComboDistortionType);
			switch (distortionTypeIndex) {
			case 1: effects.distortionType = DistortionType::Soft; break;
			case 2: effects.distortionType = DistortionType::Hard; break;
			case 3: effects.distortionType = DistortionType::Asymmetric; break;
			default: effects.distortionType = DistortionType::None; break;
			}
			effects.distortionAmount = GetSliderPosition(m_hSliderDistortionAmount) / 100.0f;

			// Filtering
			effects.enableLowPass = GetCheckboxStateBool(m_hChkLowPass);
			effects.lowPassCutoff = GetSliderPosition(m_hSliderLowPassCutoff) / 100.0f;

			effects.enableHighPass = GetCheckboxStateBool(m_hChkHighPass);
			effects.highPassCutoff = GetSliderPosition(m_hSliderHighPassCutoff) / 100.0f;

			// Bit crushing
			effects.enableBitCrush = GetCheckboxStateBool(m_hChkBitCrush);
			effects.bitDepth = GetSliderPosition(m_hSliderBitDepth);

			// Wavefold
			effects.enableWavefold = GetCheckboxStateBool(m_hChkWavefold);
			effects.wavefoldAmount = GetSliderPosition(m_hSliderWavefold) / 100.0f;

			// Sample Rate Reduction
			effects.enableSampleRateReduction = GetCheckboxStateBool(m_hChkSampleRateReduction);
			effects.sampleRateReductionFactor = GetSliderPosition(m_hSliderSampleRateReduction);

			// Spectral decay
			effects.enableSpectralDecay = GetCheckboxStateBool(m_hChkSpectralDecay);
			effects.spectralDecayAmount = GetSliderPosition(m_hSliderSpectralDecayAmount) / 100.0f;
			effects.spectralDecayCurve = GetSliderPosition(m_hSliderSpectralDecayCurve) / 10.0f;

			// Spectral tilt
			effects.enableSpectralTilt = GetCheckboxStateBool(m_hChkSpectralTilt);
			int tiltValue = GetSliderPosition(m_hSliderSpectralTilt); // 0-200, center is 100
			effects.spectralTiltAmount = (tiltValue - 100) / 100.0f; // -1.0 to 1.0

			// Spectral gate
			effects.enableSpectralGate = GetCheckboxStateBool(m_hChkSpectralGate);
			effects.spectralGateThreshold = GetSliderPosition(m_hSliderSpectralGate) / 100.0f;

			// Phase randomization
			effects.enablePhaseRandomize = GetCheckboxStateBool(m_hChkPhaseRandomize);
			effects.phaseRandomizeAmount = GetSliderPosition(m_hSliderPhaseRandomize) / 100.0f;

			// Spectral Shift
			effects.enableSpectralShift = GetCheckboxStateBool(m_hChkSpectralShift);
			int shiftVal = GetSliderPosition(m_hSliderSpectralShift); // 0-200, center 100
			effects.spectralShiftAmount = shiftVal - 100;

			// Symmetry operations
			effects.mirrorHorizontal = GetCheckboxStateBool(m_hChkMirrorH);
			effects.mirrorVertical = GetCheckboxStateBool(m_hChkMirrorV);
			effects.invert = GetCheckboxStateBool(m_hChkInvert);
			effects.reverse = GetCheckboxStateBool(m_hChkReverse);

			return effects;
		}

		void WinApplication::UpdateSampleRateReductionLabel(int value) {
			wchar_t text[16];
			swprintf_s(text, L"%dx", value);
			SetWindowText(m_hLabelSampleRateReduction, text);
		}

		void WinApplication::UpdateSpectralShiftLabel(int value) {
			int shift = value - 100;
			wchar_t text[16];
			if (shift > 0) swprintf_s(text, L"+%d", shift);
			else swprintf_s(text, L"%d", shift);
			SetWindowText(m_hLabelSpectralShift, text);
		}

		// Apply analyzed waveforms to start frame controls
		void WinApplication::ApplyWaveformsToStartFrame(const std::vector<std::pair<WaveType, float>>& waveforms) {
			// First, uncheck all waveforms and reset start sliders
			for (auto& wc : m_waveCheckboxes) {
				SetCheckboxState(wc.hwnd, 0);
				SetSliderPosition(wc.hSliderStart, 0);
			}

			// Now check and set weights for the analyzed waveforms
			for (size_t i = 0; i < waveforms.size(); ++i) {
				WaveType type = waveforms[i].first;
				float weight = waveforms[i].second;

				// Find the matching checkbox
				for (auto& wc : m_waveCheckboxes) {
					if (wc.type == type) {
						SetCheckboxState(wc.hwnd, 1);
						int sliderValue = (int)(weight * 100.0f + 0.5f); // Convert to 0-100
						SetSliderPosition(wc.hSliderStart, sliderValue);
						break;
					}
				}
			}
		}

		// Apply analyzed waveforms to end frame controls
		void WinApplication::ApplyWaveformsToEndFrame(const std::vector<std::pair<WaveType, float>>& waveforms) {
			// First, uncheck all waveforms and reset end sliders
			for (auto& wc : m_waveCheckboxes) {
				SetCheckboxState(wc.hwnd, 0);
				SetSliderPosition(wc.hSliderEnd, 0);
			}

			// Now check and set weights for the analyzed waveforms
			for (size_t i = 0; i < waveforms.size(); ++i) {
				WaveType type = waveforms[i].first;
				float weight = waveforms[i].second;

				// Find the matching checkbox
				for (auto& wc : m_waveCheckboxes) {
					if (wc.type == type) {
						SetCheckboxState(wc.hwnd, 1);
						int sliderValue = (int)(weight * 100.0f + 0.5f); // Convert to 0-100
						SetSliderPosition(wc.hSliderEnd, sliderValue);
						break;
					}
				}
			}
		}

		// Get max harmonics value from UI
		int WinApplication::GetMaxHarmonics() {
			return GetSliderPosition(m_hSliderMaxHarmonics);
		}

		// Enable/disable generation controls during batch generation
		void WinApplication::EnableGenerationControls(bool enable) {
			// Strategy: Enumerate ALL child windows and disable them individually
			// Do NOT disable the tab control or tab page containers themselves
			// This prevents the repaint issues while still blocking user interaction

			struct EnumData {
				BOOL enableFlag;
				HWND hTabControl;
				HWND hTabPages[14];
				HWND hBtnGenerate;
			};

			EnumData data;
			data.enableFlag = (enable ? TRUE : FALSE);
			data.hTabControl = m_hTabControl;
			for (int i = 0; i < 14; ++i) {
				data.hTabPages[i] = m_hTabPage[i];
			}
			data.hBtnGenerate = m_hBtnGenerate;

			// Enumerate all child windows of the main window
			EnumChildWindows(m_hwnd, [](HWND hwnd, LPARAM lParam) -> BOOL {
				EnumData* pData = (EnumData*)lParam;

				// Skip the tab control itself
				if (hwnd == pData->hTabControl) {
					return TRUE;
				}

				// Skip the tab page containers
				bool isTabPage = false;
				for (int i = 0; i < 14; ++i) {
					if (hwnd == pData->hTabPages[i]) {
						isTabPage = true;
						break;
					}
				}
				if (isTabPage) {
					return TRUE;
				}

				// Skip the Generate button (we want it to stay enabled as Cancel button)
				if (hwnd == pData->hBtnGenerate) {
					return TRUE;
				}

				// Disable/enable this control
				EnableWindow(hwnd, pData->enableFlag);

				return TRUE;
				}, (LPARAM)&data);

			// Toggle Generate/Cancel button text and keep it enabled
			SetWindowText(m_hBtnGenerate, enable ? L"Generate" : L"Cancel");

			// Show/hide progress bar and status
			ShowWindow(m_hProgressBar, enable ? SW_HIDE : SW_SHOW);
			ShowWindow(m_hStatus, enable ? SW_SHOW : SW_HIDE);
		}

		// ===== Generation Helper Methods =====

		void WinApplication::HandleCancelRequest() {
			m_bCancelGeneration = true;
			SetWindowText(m_hStatus, L"Cancelling...");
		}

		bool WinApplication::GetFolderPath(std::string& outFolderPath) {
			// Get folder path from edit box
			wchar_t folderPath[MAX_PATH];
			GetWindowText(m_hEditPath, folderPath, MAX_PATH);

			// Create directory if it doesn't exist
			CreateDirectory(folderPath, nullptr);

			// Convert to narrow string for file operations
			char folderPathA[MAX_PATH];
			WideCharToMultiByte(CP_UTF8, 0, folderPath, -1, folderPathA, MAX_PATH, nullptr, nullptr);
			outFolderPath = folderPathA;

			// Ensure folder path ends with backslash
			if (!outFolderPath.empty() && outFolderPath.back() != '\\' && outFolderPath.back() != '/') {
				outFolderPath += "\\";
			}

			return !outFolderPath.empty();
		}

		// Helper functions for reading control values
		int WinApplication::GetIntFromEditControl(HWND hEdit) {
			wchar_t text[32];
			GetWindowText(hEdit, text, 32);
			return _wtoi(text);
		}

		int WinApplication::GetCheckboxState(HWND hCheckbox) {
			return (SendMessage(hCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
		}

		bool WinApplication::GetCheckboxStateBool(HWND hCheckbox) {
			return (SendMessage(hCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
		}

		int WinApplication::GetSliderPosition(HWND hSlider) {
			return (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
		}

		int WinApplication::GetComboBoxSelection(HWND hComboBox) {
			return (int)SendMessage(hComboBox, CB_GETCURSEL, 0, 0);
		}

		// Helper functions for setting control values
		void WinApplication::SetCheckboxState(HWND hCheckbox, int value) {
			SendMessage(hCheckbox, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
		}

		void WinApplication::SetSliderPosition(HWND hSlider, int value) {
			SendMessage(hSlider, TBM_SETPOS, TRUE, value);
		}

		void WinApplication::SetComboBoxSelection(HWND hComboBox, int value) {
			SendMessage(hComboBox, CB_SETCURSEL, value, 0);
		}

		int WinApplication::GetGenerationCount() {
			int count = GetIntFromEditControl(m_hEditCount);
			return (count < 1) ? 1 : count;
		}

		void WinApplication::GenerateSingleWavetable(const std::string& folderPath) {
			SetWindowText(m_hStatus, L"Generating...");

			// Get generation settings
			bool isAudioPreview = (SendMessage(m_hChkAudioPreview, BM_GETCHECK, 0, 0) == BST_CHECKED);
			bool enableMorphing = (SendMessage(m_hChkEnableMorphing, BM_GETCHECK, 0, 0) == BST_CHECKED);

			// Get output format from combobox (0 = WT, 1 = WAV)
			int formatIndex = (int)SendMessage(m_hComboOutputFormat, CB_GETCURSEL, 0, 0);
			OutputFormat format = (formatIndex == 1) ? OutputFormat::WAV : OutputFormat::WT;
			const char* extension = (formatIndex == 1) ? ".wav" : ".wt";

			// Get number of frames from combobox
			int selIndex = (int)SendMessage(m_hComboNumFrames, CB_GETCURSEL, 0, 0);
			wchar_t numFramesText[10];
			SendMessage(m_hComboNumFrames, CB_GETLBTEXT, selIndex, (LPARAM)numFramesText);
			int numFrames = _wtoi(numFramesText);

			// Get waveform configurations
			auto startWaves = GetStartFrameWaves();
			auto endWaves = GetEndFrameWaves();

			// Get effects settings, morph curve, and PWM duty cycle
			EffectsSettings effects = GetEffectsSettings();
			MorphCurve morphCurve = GetMorphCurve();
			double pulseDuty = GetPWMDutyCycle();

			// Generate filename from settings
			std::string baseFilename = m_wavetableGenerator.GenerateFilenameFromSettings(
				startWaves, endWaves, enableMorphing, effects, morphCurve, pulseDuty);
			std::string fullPath = folderPath + baseFilename + extension;

			// Generate wavetable
			GenerationResult result = m_wavetableGenerator.GenerateWavetable(
				startWaves, endWaves, fullPath, format, isAudioPreview, enableMorphing,
				numFrames, effects, morphCurve, pulseDuty, GetMaxHarmonics());

			// Display result
			if (result == GenerationResult::Success) {
				SetWindowText(m_hStatus, L"Done!");
				InfoMessage(m_hwnd, L"Wavetable generated successfully!");
			}
			else {
				SetWindowText(m_hStatus, L"Error");
				ErrorMessage(m_hwnd, GetErrorMessage(result));
			}
		}

		void WinApplication::StartBatchGeneration(const std::string& folderPath, int count) {
			SetWindowText(m_hStatus, L"Generating...");

			// Get min/max waveforms from edit boxes.
			int minWaves = GetIntFromEditControl(m_hEditMinWaves);
			int maxWaves = GetIntFromEditControl(m_hEditMaxWaves);

			// Validate min/max
			if (minWaves < 1) minWaves = 1;
			if (maxWaves < minWaves) maxWaves = minWaves;
			if (maxWaves > 10) maxWaves = 10; // Reasonable upper limit

			// Get output format from combobox (0 = WT, 1 = WAV)
			int formatIndex = (int)SendMessage(m_hComboOutputFormat, CB_GETCURSEL, 0, 0);
			OutputFormat format = (formatIndex == 1) ? OutputFormat::WAV : OutputFormat::WT;
			const char* extension = (formatIndex == 1) ? ".wav" : ".wt";

			// Get audio preview setting
			bool isAudioPreview = (SendMessage(m_hChkAudioPreview, BM_GETCHECK, 0, 0) == BST_CHECKED);

			// Get effects settings, morph curve, and PWM duty cycle
			EffectsSettings effects = GetEffectsSettings();
			MorphCurve morphCurve = GetMorphCurve();
			double pulseDuty = GetPWMDutyCycle();

			// Get available waveforms from UI
			auto availableWaveforms = GetAvailableWaveforms();

			if (availableWaveforms.empty()) {
				SetWindowText(m_hStatus, L"Error: No waveforms selected!");
				ErrorMessage(m_hwnd, L"Please select at least one waveform.");
				return;
			}

			// Prepare thread data
			ThreadData* pData = new ThreadData{
				this,
				folderPath,
				count,
				minWaves,
				maxWaves,
				availableWaveforms,
				extension,
				format,
				isAudioPreview,
				effects,
				morphCurve,
				pulseDuty,
				GetMaxHarmonics()
			};

			// Reset cancellation flag
			m_bCancelGeneration = false;

			// Disable controls during generation
			EnableGenerationControls(false);

			// Reset and show progress bar
			SendMessage(m_hProgressBar, PBM_SETPOS, 0, 0);

			// Create worker thread
			m_hWorkerThread = CreateThread(
				nullptr,
				0,
				WorkerThreadProc,
				pData,
				0,
				&m_dwWorkerThreadId
			);

			if (m_hWorkerThread == nullptr) {
				delete pData;
				EnableGenerationControls(true);
				ErrorMessage(m_hwnd, L"Failed to create worker thread!");
			}
		}

		// Worker thread function for batch generation
		DWORD WINAPI WinApplication::WorkerThreadProc(LPVOID lpParam) {
			ThreadData* pData = reinterpret_cast<ThreadData*>(lpParam);
			WinApplication* pApp = pData->pApp;

			// Generate multiple random wavetables
			pApp->m_randomGenerator.GenerateBatch(
				pData->outputFolder, pData->count, pData->minWaves, pData->maxWaves,
				pData->availableWaveforms, pData->extension.c_str(), pData->format, pData->isAudioPreview,
				pData->effects, pData->morphCurve, pData->pulseDuty, pData->maxHarmonics,
				[pApp, pData](int generated, int total) -> bool {
					// Check for cancellation
					if (pApp->m_bCancelGeneration) {
						return false; // Stop generation
					}

					// Update progress bar
					int progress = (generated * 100) / total;
					PostMessage(pApp->m_hwnd, WM_GENERATION_PROGRESS, progress, generated);
					return true; // Continue generation
				}
			);

			// Send completion message
			if (pApp->m_bCancelGeneration) {
				PostMessage(pApp->m_hwnd, WM_GENERATION_ERROR, 0, 0);
			}
			else {
				PostMessage(pApp->m_hwnd, WM_GENERATION_COMPLETE, pData->count, 0);
			}

			// Clean up thread data
			delete pData;
			return 0;
		}

		// ===== Settings Save/Load =====

		void WinApplication::SaveSettings() {
			// Get executable directory
			wchar_t exePath[MAX_PATH];
			GetModuleFileName(NULL, exePath, MAX_PATH);
			std::wstring exeDir(exePath);
			size_t lastSlash = exeDir.find_last_of(L"\\/");
			if (lastSlash != std::wstring::npos) {
				exeDir = exeDir.substr(0, lastSlash + 1);
			}
			std::wstring settingsFile = exeDir + L"WavetableGenerator.ini";

			// Convert to narrow string for ofstream
			char settingsFileA[MAX_PATH];
			WideCharToMultiByte(CP_UTF8, 0, settingsFile.c_str(), -1, settingsFileA, MAX_PATH, nullptr, nullptr);

			std::ofstream file(settingsFileA);
			if (!file.is_open()) {
				ErrorMessage(m_hwnd, L"Failed to save settings file");
				return;
			}

			// Save output settings
			wchar_t buffer[MAX_PATH];
			GetWindowText(m_hEditPath, buffer, MAX_PATH);
			char bufferA[MAX_PATH];
			WideCharToMultiByte(CP_UTF8, 0, buffer, -1, bufferA, MAX_PATH, nullptr, nullptr);
			file << "[Output]\n";
			file << "Folder=" << bufferA << "\n";
			file << "Format=" << GetComboBoxSelection(m_hComboOutputFormat) << "\n";
			file << "Count=" << GetIntFromEditControl(m_hEditCount) << "\n\n";

			// Save generation options
			file << "[Generation]\n";
			file << "AudioPreview=" << GetCheckboxState(m_hChkAudioPreview) << "\n";
			file << "EnableMorphing=" << GetCheckboxState(m_hChkEnableMorphing) << "\n";
			file << "NumFrames=" << GetComboBoxSelection(m_hComboNumFrames) << "\n";
			file << "MinWaves=" << GetIntFromEditControl(m_hEditMinWaves) << "\n";
			file << "MaxWaves=" << GetIntFromEditControl(m_hEditMaxWaves) << "\n\n";

			// Save effects settings
			file << "[Effects]\n";
			file << "PWMDuty=" << GetSliderPosition(m_hSliderPWMDuty) << "\n";
			file << "MorphCurve=" << GetComboBoxSelection(m_hComboMorphCurve) << "\n";
			file << "DistortionType=" << GetComboBoxSelection(m_hComboDistortionType) << "\n";
			file << "DistortionAmount=" << GetSliderPosition(m_hSliderDistortionAmount) << "\n";
			file << "LowPassEnabled=" << GetCheckboxState(m_hChkLowPass) << "\n";
			file << "LowPassCutoff=" << GetSliderPosition(m_hSliderLowPassCutoff) << "\n";
			file << "HighPassEnabled=" << GetCheckboxState(m_hChkHighPass) << "\n";
			file << "HighPassCutoff=" << GetSliderPosition(m_hSliderHighPassCutoff) << "\n";
			file << "BitCrushEnabled=" << GetCheckboxState(m_hChkBitCrush) << "\n";
			file << "BitDepth=" << GetSliderPosition(m_hSliderBitDepth) << "\n";
			file << "WavefoldEnabled=" << GetCheckboxState(m_hChkWavefold) << "\n";
			file << "WavefoldAmount=" << GetSliderPosition(m_hSliderWavefold) << "\n";
			file << "SampleRateReductionEnabled=" << GetCheckboxState(m_hChkSampleRateReduction) << "\n";
			file << "SampleRateReductionFactor=" << GetSliderPosition(m_hSliderSampleRateReduction) << "\n";
			file << "MirrorH=" << GetCheckboxState(m_hChkMirrorH) << "\n";
			file << "MirrorV=" << GetCheckboxState(m_hChkMirrorV) << "\n";
			file << "Invert=" << GetCheckboxState(m_hChkInvert) << "\n";
			file << "Reverse=" << GetCheckboxState(m_hChkReverse) << "\n\n";

			// Save spectral effects
			file << "[SpectralEffects]\n";
			file << "SpectralDecayEnabled=" << GetCheckboxState(m_hChkSpectralDecay) << "\n";
			file << "SpectralDecayAmount=" << GetSliderPosition(m_hSliderSpectralDecayAmount) << "\n";
			file << "SpectralDecayCurve=" << GetSliderPosition(m_hSliderSpectralDecayCurve) << "\n";
			file << "SpectralTiltEnabled=" << GetCheckboxState(m_hChkSpectralTilt) << "\n";
			file << "SpectralTiltAmount=" << GetSliderPosition(m_hSliderSpectralTilt) << "\n";
			file << "SpectralGateEnabled=" << GetCheckboxState(m_hChkSpectralGate) << "\n";
			file << "SpectralGateAmount=" << GetSliderPosition(m_hSliderSpectralGate) << "\n";
			file << "SpectralShiftEnabled=" << GetCheckboxState(m_hChkSpectralShift) << "\n";
			file << "SpectralShiftAmount=" << GetSliderPosition(m_hSliderSpectralShift) << "\n";
			file << "PhaseRandomizeEnabled=" << GetCheckboxState(m_hChkPhaseRandomize) << "\n";
			file << "PhaseRandomizeAmount=" << GetSliderPosition(m_hSliderPhaseRandomize) << "\n\n";

			// Save advanced settings
			file << "[Advanced]\n";
			file << "MaxHarmonics=" << GetSliderPosition(m_hSliderMaxHarmonics) << "\n\n";

			// Save waveform selections
			file << "[Waveforms]\n";
			for (size_t i = 0; i < m_waveCheckboxes.size(); ++i) {
				const auto& wc = m_waveCheckboxes[i];
				int checked = GetCheckboxState(wc.hwnd);
				int startVal = GetSliderPosition(wc.hSliderStart);
				int endVal = GetSliderPosition(wc.hSliderEnd);
				file << "Wave" << i << "=" << checked << "," << startVal << "," << endVal << "\n";
			}

			file.close();
		}

		void WinApplication::LoadSettings() {
			// Get executable directory
			wchar_t exePath[MAX_PATH];
			GetModuleFileName(NULL, exePath, MAX_PATH);
			std::wstring exeDir(exePath);
			size_t lastSlash = exeDir.find_last_of(L"\\/");
			if (lastSlash != std::wstring::npos) {
				exeDir = exeDir.substr(0, lastSlash + 1);
			}
			std::wstring settingsFile = exeDir + L"WavetableGenerator.ini";

			// Convert to narrow string for ifstream
			char settingsFileA[MAX_PATH];
			WideCharToMultiByte(CP_UTF8, 0, settingsFile.c_str(), -1, settingsFileA, MAX_PATH, nullptr, nullptr);

			std::ifstream file(settingsFileA);
			if (!file.is_open()) {
				// Silently fail - settings file doesn't exist yet (first run or never saved)
				return;
			}

			std::string line, section;
			while (std::getline(file, line)) {
				// Trim whitespace
				line.erase(0, line.find_first_not_of(" \t\r\n"));
				line.erase(line.find_last_not_of(" \t\r\n") + 1);

				if (line.empty() || line[0] == ';') continue;

				// Section header
				if (line[0] == '[') {
					section = line.substr(1, line.find(']') - 1);
					continue;
				}

				// Key=Value pair
				size_t eqPos = line.find('=');
				if (eqPos == std::string::npos) continue;

				std::string key = line.substr(0, eqPos);
				std::string value = line.substr(eqPos + 1);

				// Parse based on section
				if (section == "Output") {
					if (key == "Folder") {
						wchar_t wvalue[MAX_PATH];
						MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wvalue, MAX_PATH);
						SetWindowText(m_hEditPath, wvalue);
					}
					else if (key == "Format") {
						SetComboBoxSelection(m_hComboOutputFormat, std::stoi(value));
					}
					else if (key == "Count") {
						SetWindowText(m_hEditCount, std::to_wstring(std::stoi(value)).c_str());
					}
				}
				else if (section == "Generation") {
					if (key == "AudioPreview") {
						SetCheckboxState(m_hChkAudioPreview, std::stoi(value));
					}
					else if (key == "EnableMorphing") {
						int enabled = std::stoi(value);
						SetCheckboxState(m_hChkEnableMorphing, enabled);
						EnableWindow(m_hComboNumFrames, enabled ? TRUE : FALSE);
					}
					else if (key == "NumFrames") {
						SetComboBoxSelection(m_hComboNumFrames, std::stoi(value));
					}
					else if (key == "MinWaves") {
						SetWindowText(m_hEditMinWaves, std::to_wstring(std::stoi(value)).c_str());
					}
					else if (key == "MaxWaves") {
						SetWindowText(m_hEditMaxWaves, std::to_wstring(std::stoi(value)).c_str());
					}
				}
				else if (section == "Effects") {
					if (key == "PWMDuty") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderPWMDuty, pos);
						UpdatePWMDutyLabel(pos);
					}
					else if (key == "MorphCurve") {
						SetComboBoxSelection(m_hComboMorphCurve, std::stoi(value));
					}
					else if (key == "DistortionType") {
						SetComboBoxSelection(m_hComboDistortionType, std::stoi(value));
					}
					else if (key == "DistortionAmount") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderDistortionAmount, pos);
						UpdateDistortionAmountLabel(pos);
					}
					else if (key == "LowPassEnabled") {
						SetCheckboxState(m_hChkLowPass, std::stoi(value));
					}
					else if (key == "LowPassCutoff") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderLowPassCutoff, pos);
						UpdateLowPassCutoffLabel(pos);
					}
					else if (key == "HighPassEnabled") {
						SetCheckboxState(m_hChkHighPass, std::stoi(value));
					}
					else if (key == "HighPassCutoff") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderHighPassCutoff, pos);
						UpdateHighPassCutoffLabel(pos);
					}
					else if (key == "BitCrushEnabled") {
						SetCheckboxState(m_hChkBitCrush, std::stoi(value));
					}
					else if (key == "BitDepth") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderBitDepth, pos);
						UpdateBitDepthLabel(pos);
					}
					else if (key == "WavefoldEnabled") {
						SetCheckboxState(m_hChkWavefold, std::stoi(value));
					}
					else if (key == "WavefoldAmount") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderWavefold, pos);
						UpdateWavefoldAmountLabel(pos);
					}
					else if (key == "SampleRateReductionEnabled") {
						SetCheckboxState(m_hChkSampleRateReduction, std::stoi(value));
					}
					else if (key == "SampleRateReductionFactor") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderSampleRateReduction, pos);
						UpdateSampleRateReductionLabel(pos);
					}
					else if (key == "MirrorH") {
						SetCheckboxState(m_hChkMirrorH, std::stoi(value));
					}
					else if (key == "MirrorV") {
						SetCheckboxState(m_hChkMirrorV, std::stoi(value));
					}
					else if (key == "Invert") {
						SetCheckboxState(m_hChkInvert, std::stoi(value));
					}
					else if (key == "Reverse") {
						SetCheckboxState(m_hChkReverse, std::stoi(value));
					}
				}
				else if (section == "SpectralEffects") {
					if (key == "SpectralDecayEnabled") {
						SetCheckboxState(m_hChkSpectralDecay, std::stoi(value));
					}
					else if (key == "SpectralDecayAmount") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderSpectralDecayAmount, pos);
						UpdateLabelPercent(m_hLabelSpectralDecayAmount, pos);
					}
					else if (key == "SpectralDecayCurve") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderSpectralDecayCurve, pos);
						UpdateLabelWithFormat(m_hLabelSpectralDecayCurve, pos, L"%.1f");
					}
					else if (key == "SpectralTiltEnabled") {
						SetCheckboxState(m_hChkSpectralTilt, std::stoi(value));
					}
					else if (key == "SpectralTiltAmount") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderSpectralTilt, pos);
						UpdateSpectralTiltLabel(pos);
					}
					else if (key == "SpectralGateEnabled") {
						SetCheckboxState(m_hChkSpectralGate, std::stoi(value));
					}
					else if (key == "SpectralGateAmount") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderSpectralGate, pos);
						UpdateSpectralGateLabel(pos);
					}
					else if (key == "SpectralShiftEnabled") {
						SetCheckboxState(m_hChkSpectralShift, std::stoi(value));
					}
					else if (key == "SpectralShiftAmount") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderSpectralShift, pos);
						UpdateSpectralShiftLabel(pos);
					}
					else if (key == "PhaseRandomizeEnabled") {
						SetCheckboxState(m_hChkPhaseRandomize, std::stoi(value));
					}
					else if (key == "PhaseRandomizeAmount") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderPhaseRandomize, pos);
						UpdatePhaseRandomizeLabel(pos);
					}
				}
				else if (section == "Advanced") {
					if (key == "MaxHarmonics") {
						int pos = std::stoi(value);
						SetSliderPosition(m_hSliderMaxHarmonics, pos);
						UpdateMaxHarmonicsLabel(pos);
					}
				}
				else if (section == "Waveforms") {
					// Parse Wave0=1,100,100 format
					if (key.find("Wave") == 0) {
						size_t waveIdx = std::stoi(key.substr(4));
						if (waveIdx < m_waveCheckboxes.size()) {
							std::istringstream ss(value);
							std::string token;
							int checked = 0, startVal = 100, endVal = 100;

							if (std::getline(ss, token, ',')) checked = std::stoi(token);
							if (std::getline(ss, token, ',')) startVal = std::stoi(token);
							if (std::getline(ss, token, ',')) endVal = std::stoi(token);

							auto& wc = m_waveCheckboxes[waveIdx];
							SetCheckboxState(wc.hwnd, checked);
							SetSliderPosition(wc.hSliderStart, startVal);
							SetSliderPosition(wc.hSliderEnd, endVal);
						}
					}
				}
			}

			file.close();
		}

		// ===== UI Helper Methods - Update Slider Labels =====

		// Generic helper to update a label with a formatted value
		void WinApplication::UpdateLabelWithFormat(HWND hLabel, int value, const wchar_t* format) {
			wchar_t text[32];
			swprintf_s(text, format, value);
			SetWindowText(hLabel, text);
		}

		// Helper to update a label with percentage
		void WinApplication::UpdateLabelPercent(HWND hLabel, int value) {
			UpdateLabelWithFormat(hLabel, value, L"%d%%");
		}

		void WinApplication::UpdateMaxHarmonicsLabel(int value) {
			wchar_t text[32];
			if (value <= 5) {
				swprintf_s(text, L"%d (Clean)", value);
			}
			else if (value <= 8) {
				swprintf_s(text, L"%d (Balanced)", value);
			}
			else if (value <= 12) {
				swprintf_s(text, L"%d (Rich)", value);
			}
			else {
				swprintf_s(text, L"%d (Very Rich)", value);
			}
			SetWindowText(m_hLabelMaxHarmonics, text);
		}

		void WinApplication::UpdatePWMDutyLabel(int value) {
			UpdateLabelPercent(m_hLabelPWMDuty, value);
		}

		void WinApplication::UpdateDistortionAmountLabel(int value) {
			UpdateLabelPercent(m_hLabelDistortionAmount, value);
		}

		void WinApplication::UpdateLowPassCutoffLabel(int value) {
			UpdateLabelPercent(m_hLabelLowPassCutoff, value);
		}

		void WinApplication::UpdateHighPassCutoffLabel(int value) {
			UpdateLabelPercent(m_hLabelHighPassCutoff, value);
		}

		void WinApplication::UpdateBitDepthLabel(int value) {
			UpdateLabelWithFormat(m_hLabelBitDepth, value, L"%d bits");
		}

		void WinApplication::UpdateWavefoldAmountLabel(int value) {
			UpdateLabelPercent(m_hLabelWavefold, value);
		}

		void WinApplication::UpdateSpectralDecayAmountLabel(int value) {
			UpdateLabelPercent(m_hLabelSpectralDecayAmount, value);
		}

		void WinApplication::UpdateSpectralDecayCurveLabel(int value) {
			float curveValue = value / 10.0f;
			wchar_t buffer[32];
			swprintf_s(buffer, 32, L"%.1f", curveValue);
			SetWindowText(m_hLabelSpectralDecayCurve, buffer);
		}

		void WinApplication::UpdateSpectralTiltLabel(int value) {
			// Value 0-200, center is 100 (0%)
			int tiltPercent = value - 100;
			wchar_t buffer[32];
			if (tiltPercent > 0) {
				swprintf_s(buffer, 32, L"+%d%%", tiltPercent);
			}
			else {
				swprintf_s(buffer, 32, L"%d%%", tiltPercent);
			}
			SetWindowText(m_hLabelSpectralTilt, buffer);
		}

		void WinApplication::UpdateSpectralGateLabel(int value) {
			UpdateLabelPercent(m_hLabelSpectralGate, value);
		}

		void WinApplication::UpdatePhaseRandomizeLabel(int value) {
			UpdateLabelPercent(m_hLabelPhaseRandomize, value);
		}

		// ===== UI Creation Helper Methods =====

		void WinApplication::CreateTabControl(HWND hwnd) {
			// Create Tab Control (starts at top)
			m_hTabControl = CreateWindow(WC_TABCONTROL, nullptr,
				WS_VISIBLE | WS_CHILD,
				10, 10, 650, 430, hwnd, nullptr, m_hInstance, nullptr);

			SetWindowTheme(m_hTabControl, L"TabControl", NULL);

			// Add tabs (alphabetical order, with Import, Effects, and Settings at the end)
			TCITEM tie = {};
			tie.mask = TCIF_TEXT;

			tie.pszText = (LPWSTR)L"Basic";
			TabCtrl_InsertItem(m_hTabControl, 0, &tie);

			tie.pszText = (LPWSTR)L"Chaos";
			TabCtrl_InsertItem(m_hTabControl, 1, &tie);

			tie.pszText = (LPWSTR)L"Fractals";
			TabCtrl_InsertItem(m_hTabControl, 2, &tie);

			tie.pszText = (LPWSTR)L"Harmonic";
			TabCtrl_InsertItem(m_hTabControl, 3, &tie);

			tie.pszText = (LPWSTR)L"Inharmonic";
			TabCtrl_InsertItem(m_hTabControl, 4, &tie);

			tie.pszText = (LPWSTR)L"Modern";
			TabCtrl_InsertItem(m_hTabControl, 5, &tie);

			tie.pszText = (LPWSTR)L"Modulation";
			TabCtrl_InsertItem(m_hTabControl, 6, &tie);

			tie.pszText = (LPWSTR)L"Physical";
			TabCtrl_InsertItem(m_hTabControl, 7, &tie);

			tie.pszText = (LPWSTR)L"Synthesis";
			TabCtrl_InsertItem(m_hTabControl, 8, &tie);

			tie.pszText = (LPWSTR)L"Vintage";
			TabCtrl_InsertItem(m_hTabControl, 9, &tie);

			tie.pszText = (LPWSTR)L"Vowels";
			TabCtrl_InsertItem(m_hTabControl, 10, &tie);

			tie.pszText = (LPWSTR)L"Import";
			TabCtrl_InsertItem(m_hTabControl, 11, &tie);

			tie.pszText = (LPWSTR)L"Effects";
			TabCtrl_InsertItem(m_hTabControl, 12, &tie);

			tie.pszText = (LPWSTR)L"Spectral Effects";
			TabCtrl_InsertItem(m_hTabControl, 13, &tie);

			tie.pszText = (LPWSTR)L"Settings";
			TabCtrl_InsertItem(m_hTabControl, 14, &tie);
		}

		void WinApplication::CreateTabPages(HWND hwnd) {
			// Create tab pages (static windows to hold controls for each tab)
			for (int i = 0; i < 15; ++i) {
				m_hTabPage[i] = CreateWindowEx(
					0, L"STATIC", nullptr,
					WS_CHILD | (i == 0 ? WS_VISIBLE : 0), // Only first tab visible initially
					25, 35, 620, 385, hwnd, nullptr, m_hInstance, nullptr);

				// Enable visual styles for tab pages
				HWND hTab = m_hTabPage[i];
				SetWindowTheme(hTab, L"Explorer", NULL);
				WNDPROC originalPaneProc = (WNDPROC)SetWindowLongPtr(hTab, GWLP_WNDPROC, (LONG_PTR)PaneProc);
				SetWindowLongPtr(hTab, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(originalPaneProc));
				InvalidateRect(hTab, NULL, TRUE);
			}
		}

		void WinApplication::AddWaveform(int tabIdx, int yPos, const wchar_t* name, WaveType type) {
			HWND page = m_hTabPage[tabIdx];

			// Checkbox
			HWND hChk = CreateWindow(L"BUTTON", name,
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
				10, yPos, 120, 20, page, nullptr, nullptr, nullptr);

			// Start slider
			HWND hSliderStart = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS,
				140, yPos, 200, 20, page, nullptr, nullptr, nullptr);

			SendMessage(hSliderStart, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
			SendMessage(hSliderStart, TBM_SETPOS, TRUE, 100);

			// End slider
			HWND hSliderEnd = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS,
				360, yPos, 200, 20, page, nullptr, nullptr, nullptr);
			SendMessage(hSliderEnd, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
			SendMessage(hSliderEnd, TBM_SETPOS, TRUE, 100);

			m_waveCheckboxes.push_back({ hChk, hSliderStart, hSliderEnd, type });
		}

		void WinApplication::AddColumnHeaders() {
			// Add column headers to each tab page (tabs 0-10: Basic through Vowels)
			for (int i = 0; i < 11; ++i) {
				CreateWindow(L"STATIC", L"Waveform",
					WS_VISIBLE | WS_CHILD,
					10, 11, 120, 18, m_hTabPage[i], nullptr, nullptr, nullptr);

				CreateWindow(L"STATIC", L"Start Mix Level",
					WS_VISIBLE | WS_CHILD | SS_CENTER,
					140, 11, 200, 18, m_hTabPage[i], nullptr, nullptr, nullptr);

				CreateWindow(L"STATIC", L"End Mix Level",
					WS_VISIBLE | WS_CHILD | SS_CENTER,
					360, 11, 200, 18, m_hTabPage[i], nullptr, nullptr, nullptr);
			}
		}

		void WinApplication::CreateBasicTab() {
			// TAB 0: Basic Waves
			int y = 34;
			AddWaveform(0, y, L"Sine", WaveType::Sine); y += 25;
			AddWaveform(0, y, L"Square", WaveType::Square); y += 25;
			AddWaveform(0, y, L"Triangle", WaveType::Triangle); y += 25;
			AddWaveform(0, y, L"Saw", WaveType::Saw); y += 25;
			AddWaveform(0, y, L"Reverse Saw", WaveType::ReverseSaw); y += 25;
			AddWaveform(0, y, L"Pulse", WaveType::Pulse); y += 25;
		}

		void WinApplication::CreateChaosTab() {
			// TAB 1: Chaos Theory
			int y = 34;
			AddWaveform(1, y, L"Lorenz", WaveType::Lorenz); y += 25;
			AddWaveform(1, y, L"Rossler", WaveType::Rossler); y += 25;
			AddWaveform(1, y, L"Henon", WaveType::Henon); y += 25;
			AddWaveform(1, y, L"Duffing", WaveType::Duffing); y += 25;
			AddWaveform(1, y, L"Chua", WaveType::Chua); y += 25;
			AddWaveform(1, y, L"Logistic Chaos", WaveType::LogisticChaos); y += 25;
		}

		void WinApplication::CreateFractalsTab() {
			// TAB 2: Fractals
			int y = 34;
			AddWaveform(2, y, L"Weierstrass", WaveType::Weierstrass); y += 25;
			AddWaveform(2, y, L"Cantor", WaveType::Cantor); y += 25;
			AddWaveform(2, y, L"Koch", WaveType::Koch); y += 25;
			AddWaveform(2, y, L"Mandelbrot", WaveType::Mandelbrot); y += 25;
		}

		void WinApplication::CreateHarmonicTab() {
			// TAB 3: Harmonic Waves
			int y = 34;
			AddWaveform(3, y, L"Odd Harmonics", WaveType::OddHarmonics); y += 25;
			AddWaveform(3, y, L"Even Harmonics", WaveType::EvenHarmonics); y += 25;
			AddWaveform(3, y, L"Harmonic Series", WaveType::HarmonicSeries); y += 25;
			AddWaveform(3, y, L"Sub Harmonics", WaveType::SubHarmonics); y += 25;
			AddWaveform(3, y, L"Formant", WaveType::Formant); y += 25;
			AddWaveform(3, y, L"Additive", WaveType::Additive); y += 25;
		}

		void WinApplication::CreateInharmonicTab() {
			// TAB 4: Inharmonic Series
			int y = 34;
			AddWaveform(4, y, L"Stretched Harm", WaveType::StretchedHarm); y += 25;
			AddWaveform(4, y, L"Compressed Harm", WaveType::CompressedHarm); y += 25;
			AddWaveform(4, y, L"Metallic", WaveType::Metallic); y += 25;
			AddWaveform(4, y, L"Clangorous", WaveType::Clangorous); y += 25;
			AddWaveform(4, y, L"Karplus-Strong", WaveType::KarplusStrong); y += 25;
			AddWaveform(4, y, L"Stiff String", WaveType::StiffString); y += 25;
		}

		void WinApplication::CreateModernTab() {
			// TAB 5: Modern/Digital + Mathematical
			int y = 34;
			AddWaveform(5, y, L"Supersaw", WaveType::Supersaw); y += 25;
			AddWaveform(5, y, L"PWM Saw", WaveType::PWMSaw); y += 25;
			AddWaveform(5, y, L"Parabolic", WaveType::Parabolic); y += 25;
			AddWaveform(5, y, L"Double Sine", WaveType::DoubleSine); y += 25;
			AddWaveform(5, y, L"Half Sine", WaveType::HalfSine); y += 25;
			AddWaveform(5, y, L"Trapezoid", WaveType::Trapezoid); y += 25;
			AddWaveform(5, y, L"Power", WaveType::Power); y += 25;
			AddWaveform(5, y, L"Exponential", WaveType::Exponential); y += 25;
			AddWaveform(5, y, L"Logistic", WaveType::Logistic); y += 25;
			AddWaveform(5, y, L"Stepped", WaveType::Stepped); y += 25;
			AddWaveform(5, y, L"Noise", WaveType::Noise); y += 25;
			AddWaveform(5, y, L"Procedural", WaveType::Procedural); y += 25;
			AddWaveform(5, y, L"Sinc", WaveType::Sinc);
		}

		void WinApplication::CreateModulationTab() {
			// TAB 6: Modulation Synthesis
			int y = 34;
			AddWaveform(6, y, L"Ring Mod", WaveType::RingMod); y += 25;
			AddWaveform(6, y, L"Amplitude Mod", WaveType::AmplitudeMod); y += 25;
			AddWaveform(6, y, L"Frequency Mod", WaveType::FrequencyMod); y += 25;
			AddWaveform(6, y, L"Cross Mod", WaveType::CrossMod); y += 25;
			AddWaveform(6, y, L"Phase Mod", WaveType::PhaseMod); y += 25;
		}

		void WinApplication::CreatePhysicalTab() {
			// TAB 7: Physical Models
			int y = 34;
			AddWaveform(7, y, L"String", WaveType::String); y += 25;
			AddWaveform(7, y, L"Brass", WaveType::Brass); y += 25;
			AddWaveform(7, y, L"Reed", WaveType::Reed); y += 25;
			AddWaveform(7, y, L"Vocal", WaveType::Vocal); y += 25;
			AddWaveform(7, y, L"Bell", WaveType::Bell); y += 25;
		}

		void WinApplication::CreateSynthesisTab() {
			// TAB 8: Synthesis Waves
			int y = 34;
			AddWaveform(8, y, L"Simple FM", WaveType::SimpleFM); y += 25;
			AddWaveform(8, y, L"Complex FM", WaveType::ComplexFM); y += 25;
			AddWaveform(8, y, L"Phase Distortion", WaveType::PhaseDistortion); y += 25;
			AddWaveform(8, y, L"Wavefold", WaveType::Wavefold); y += 25;
			AddWaveform(8, y, L"Hard Sync", WaveType::HardSync); y += 25;
			AddWaveform(8, y, L"Chebyshev", WaveType::Chebyshev); y += 25;
		}

		void WinApplication::CreateVintageTab() {
			// TAB 9: Vintage Synth Emulation (Alphabetical)
			int y = 34;
			AddWaveform(9, y, L"ARP Odyssey", WaveType::ARPOdyssey); y += 25;
			AddWaveform(9, y, L"Yamaha CS-80", WaveType::CS80); y += 25;
			AddWaveform(9, y, L"Juno", WaveType::Juno); y += 25;
			AddWaveform(9, y, L"Minimoog", WaveType::MiniMoog); y += 25;
			AddWaveform(9, y, L"Korg MS-20", WaveType::MS20); y += 25;
			AddWaveform(9, y, L"Oberheim", WaveType::Oberheim); y += 25;
			AddWaveform(9, y, L"PPG Wave", WaveType::PPG); y += 25;
			AddWaveform(9, y, L"Prophet-5", WaveType::Prophet5); y += 25;
			AddWaveform(9, y, L"TB-303", WaveType::TB303); y += 25;
		}

		void WinApplication::CreateVowelsTab() {
			// TAB 10: Vowel Formants
			int y = 34;
			AddWaveform(10, y, L"Vowel A", WaveType::VowelA); y += 25;
			AddWaveform(10, y, L"Vowel E", WaveType::VowelE); y += 25;
			AddWaveform(10, y, L"Vowel I", WaveType::VowelI); y += 25;
			AddWaveform(10, y, L"Vowel O", WaveType::VowelO); y += 25;
			AddWaveform(10, y, L"Vowel U", WaveType::VowelU); y += 25;
			AddWaveform(10, y, L"Diphthong", WaveType::Diphthong); y += 25;
		}

		void WinApplication::CreateImportTab() {
			// ===== IMPORT TAB (Tab 11) =====
			HWND importPage = m_hTabPage[11];
			int iy = 10;

			// === IMPORT WAVETABLE GROUPBOX ===
			CreateWindow(L"BUTTON", L"Import Wavetable", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, iy, 590, 120, importPage, nullptr, nullptr, nullptr);
			iy += 24;

			m_hBtnImport = CreateWindow(L"BUTTON", L"Load Wavetable...",
				WS_VISIBLE | WS_CHILD,
				20, iy, 130, 24, importPage, (HMENU)CMD_IMPORT_WAVETABLE, nullptr, nullptr);

			m_hBtnClearImport = CreateWindow(L"BUTTON", L"Clear",
				WS_VISIBLE | WS_CHILD,
				155, iy, 60, 24, importPage, (HMENU)CMD_CLEAR_IMPORT, nullptr, nullptr);

			EnableWindow(m_hBtnClearImport, FALSE);

			m_hLabelImportInfo = CreateWindow(L"STATIC", L"No wavetable loaded",
				WS_VISIBLE | WS_CHILD | SS_LEFT,
				220, iy + 4, 350, 20, importPage, nullptr, nullptr, nullptr);
			iy += 34;

			CreateWindow(L"STATIC", L"Select Frame:", WS_VISIBLE | WS_CHILD,
				20, iy + 3, 80, 20, importPage, nullptr, nullptr, nullptr);

			m_hComboImportedFrame = CreateWindow(L"COMBOBOX", nullptr,
				WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
				105, iy, 110, 150, importPage, nullptr, nullptr, nullptr);

			EnableWindow(m_hComboImportedFrame, FALSE);
			iy += 32;

			CreateWindow(L"STATIC", L"Apply frame to generation:", WS_VISIBLE | WS_CHILD,
				20, iy + 4, 200, 20, importPage, nullptr, nullptr, nullptr);

			m_hBtnUseAsStart = CreateWindow(L"BUTTON", L"Use as Start Frame",
				WS_VISIBLE | WS_CHILD,
				220, iy, 140, 24, importPage, (HMENU)CMD_USE_AS_START, nullptr, nullptr);

			EnableWindow(m_hBtnUseAsStart, FALSE);

			m_hBtnUseAsEnd = CreateWindow(L"BUTTON", L"Use as End Frame",
				WS_VISIBLE | WS_CHILD,
				365, iy, 140, 24, importPage, (HMENU)CMD_USE_AS_END, nullptr, nullptr);

			EnableWindow(m_hBtnUseAsEnd, FALSE);
		}

		void WinApplication::CreateEffectsTab() {
			// ===== EFFECTS TAB (Tab 12) =====
			HWND effectsPage = m_hTabPage[12];
			int ey = 10;
			int y = ey;

			// === PWM & MORPH GROUPBOX ===
			CreateWindow(L"BUTTON", L"PWM & Morph", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, ey, 590, 60, effectsPage, nullptr, nullptr, nullptr);
			ey += 20;

			CreateWindow(L"STATIC", L"PWM Duty:", WS_VISIBLE | WS_CHILD,
				20, ey + 2, 75, 20, effectsPage, nullptr, nullptr, nullptr);

			m_hSliderPWMDuty = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				100, ey, 180, 20, effectsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderPWMDuty, TBM_SETRANGE, TRUE, MAKELONG(1, 99));
			SendMessage(m_hSliderPWMDuty, TBM_SETPOS, TRUE, 50);
			m_hLabelPWMDuty = CreateWindow(L"STATIC", L"50%", WS_VISIBLE | WS_CHILD | SS_LEFT,
				285, ey + 2, 40, 20, effectsPage, nullptr, nullptr, nullptr);

			CreateWindow(L"STATIC", L"Morph:", WS_VISIBLE | WS_CHILD,
				340, ey + 2, 50, 20, effectsPage, nullptr, nullptr, nullptr);

			m_hComboMorphCurve = CreateWindow(L"COMBOBOX", nullptr,
				WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
				395, ey, 170, 150, effectsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hComboMorphCurve, CB_ADDSTRING, 0, (LPARAM)L"Linear");
			SendMessage(m_hComboMorphCurve, CB_ADDSTRING, 0, (LPARAM)L"Exponential");
			SendMessage(m_hComboMorphCurve, CB_ADDSTRING, 0, (LPARAM)L"Logarithmic");
			SendMessage(m_hComboMorphCurve, CB_ADDSTRING, 0, (LPARAM)L"S-Curve");
			SendMessage(m_hComboMorphCurve, CB_SETCURSEL, 0, 0);
			ey += 50; // 60 (height) - 10 (overlap for groupbox border/title) = 50
			y = ey;

			// === DISTORTION GROUPBOX ===
			CreateWindow(L"BUTTON", L"Distortion", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, ey, 290, 90, effectsPage, nullptr, nullptr, nullptr);
			ey += 20;

			CreateWindow(L"STATIC", L"Type:", WS_VISIBLE | WS_CHILD,
				20, ey + 2, 40, 20, effectsPage, nullptr, nullptr, nullptr);

			m_hComboDistortionType = CreateWindow(L"COMBOBOX", nullptr,
				WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
				65, ey, 110, 150, effectsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hComboDistortionType, CB_ADDSTRING, 0, (LPARAM)L"None");
			SendMessage(m_hComboDistortionType, CB_ADDSTRING, 0, (LPARAM)L"Soft (Tanh)");
			SendMessage(m_hComboDistortionType, CB_ADDSTRING, 0, (LPARAM)L"Hard Clip");
			SendMessage(m_hComboDistortionType, CB_ADDSTRING, 0, (LPARAM)L"Asymmetric");
			SendMessage(m_hComboDistortionType, CB_SETCURSEL, 0, 0);

			CreateWindow(L"STATIC", L"Amount:", WS_VISIBLE | WS_CHILD,
				185, ey + 2, 50, 20, effectsPage, nullptr, nullptr, nullptr);

			m_hSliderDistortionAmount = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				20, ey + 32, 255, 20, effectsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderDistortionAmount, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
			SendMessage(m_hSliderDistortionAmount, TBM_SETPOS, TRUE, 0);
			m_hLabelDistortionAmount = CreateWindow(L"STATIC", L"0%", WS_VISIBLE | WS_CHILD,
				240, ey + 2, 40, 20, effectsPage, nullptr, nullptr, nullptr);

			ey += 80; // 90 (height) - 10 (overlap for groupbox border/title) = 80

			// === FILTERS GROUPBOX ===
			CreateWindow(L"BUTTON", L"Filters", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				310, y, 290, 90, effectsPage, nullptr, nullptr, nullptr);

			y += 20;

			m_hChkLowPass = CreateWindow(L"BUTTON", L"Low-Pass", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				320, y, 80, 20, effectsPage, nullptr, nullptr, nullptr);

			m_hSliderLowPassCutoff = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				405, y, 145, 20, effectsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderLowPassCutoff, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
			SendMessage(m_hSliderLowPassCutoff, TBM_SETPOS, TRUE, 100);

			m_hLabelLowPassCutoff = CreateWindow(L"STATIC", L"100%", WS_VISIBLE | WS_CHILD,
				555, y + 2, 40, 20, effectsPage, nullptr, nullptr, nullptr);
			y += 32;

			m_hChkHighPass = CreateWindow(L"BUTTON", L"High-Pass", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				320, y, 80, 20, effectsPage, nullptr, nullptr, nullptr);

			m_hSliderHighPassCutoff = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				405, y, 145, 20, effectsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderHighPassCutoff, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
			SendMessage(m_hSliderHighPassCutoff, TBM_SETPOS, TRUE, 0);
			m_hLabelHighPassCutoff = CreateWindow(L"STATIC", L"0%", WS_VISIBLE | WS_CHILD,
				555, y + 2, 40, 20, effectsPage, nullptr, nullptr, nullptr);

			y = ey;

			// === BIT CRUSHING GROUPBOX ===
			CreateWindow(L"BUTTON", L"Bit Crushing", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, ey, 290, 60, effectsPage, nullptr, nullptr, nullptr);
			ey += 20;

			m_hChkBitCrush = CreateWindow(L"BUTTON", L"Enable", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				20, ey, 60, 20, effectsPage, nullptr, nullptr, nullptr);

			m_hSliderBitDepth = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				85, ey, 145, 20, effectsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderBitDepth, TBM_SETRANGE, TRUE, MAKELONG(1, 16));
			SendMessage(m_hSliderBitDepth, TBM_SETPOS, TRUE, 16);

			m_hLabelBitDepth = CreateWindow(L"STATIC", L"16 bits", WS_VISIBLE | WS_CHILD,
				235, ey + 2, 50, 20, effectsPage, nullptr, nullptr, nullptr);

			ey += 50;

			// === SAMPLE RATE REDUCTION GROUPBOX ===
			CreateWindow(L"BUTTON", L"Sample Rate Reduction", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				310, y, 290, 60, effectsPage, nullptr, nullptr, nullptr);

			y += 20;

			m_hChkSampleRateReduction = CreateWindow(L"BUTTON", L"Enable", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				320, y, 60, 20, effectsPage, nullptr, nullptr, nullptr);

			m_hSliderSampleRateReduction = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				405, y, 145, 20, effectsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderSampleRateReduction, TBM_SETRANGE, TRUE, MAKELONG(1, 32));
			SendMessage(m_hSliderSampleRateReduction, TBM_SETPOS, TRUE, 1);

			m_hLabelSampleRateReduction = CreateWindow(L"STATIC", L"1x", WS_VISIBLE | WS_CHILD,
				555, y + 2, 40, 20, effectsPage, nullptr, nullptr, nullptr);

			y += 50;

			// === SYMMETRY GROUPBOX ===
			CreateWindow(L"BUTTON", L"Symmetry", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, ey, 290, 74, effectsPage, nullptr, nullptr, nullptr);

			ey += 20;
			m_hChkMirrorH = CreateWindow(L"BUTTON", L"Mirror H", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				20, ey, 80, 20, effectsPage, nullptr, nullptr, nullptr);
			m_hChkMirrorV = CreateWindow(L"BUTTON", L"Mirror V", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				140, ey, 80, 20, effectsPage, nullptr, nullptr, nullptr);
			ey += 25;

			m_hChkInvert = CreateWindow(L"BUTTON", L"Invert", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				20, ey, 70, 20, effectsPage, nullptr, nullptr, nullptr);
			m_hChkReverse = CreateWindow(L"BUTTON", L"Reverse", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				140, ey, 80, 20, effectsPage, nullptr, nullptr, nullptr);

			// === WAVEFOLD GROUPBOX ===
			CreateWindow(L"BUTTON", L"Wavefold", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				310, y, 290, 74, effectsPage, nullptr, nullptr, nullptr);

			y += 20;

			m_hChkWavefold = CreateWindow(L"BUTTON", L"Enable", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				320, y, 60, 20, effectsPage, nullptr, nullptr, nullptr);

			m_hSliderWavefold = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				405, y, 145, 20, effectsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderWavefold, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
			SendMessage(m_hSliderWavefold, TBM_SETPOS, TRUE, 0);

			m_hLabelWavefold = CreateWindow(L"STATIC", L"0%", WS_VISIBLE | WS_CHILD,
				555, y + 2, 40, 20, effectsPage, nullptr, nullptr, nullptr);

			y += 30;

			// Apply visual styles to effects tab controls
			SetWindowTheme(m_hSliderPWMDuty, L"Explorer", NULL);
			SetWindowTheme(m_hComboMorphCurve, L"Explorer", NULL);
			SetWindowTheme(m_hComboDistortionType, L"Explorer", NULL);
			SetWindowTheme(m_hSliderDistortionAmount, L"Explorer", NULL);
			SetWindowTheme(m_hChkLowPass, L"Explorer", NULL);
			SetWindowTheme(m_hSliderLowPassCutoff, L"Explorer", NULL);
			SetWindowTheme(m_hChkHighPass, L"Explorer", NULL);
			SetWindowTheme(m_hSliderHighPassCutoff, L"Explorer", NULL);
			SetWindowTheme(m_hChkBitCrush, L"Explorer", NULL);
			SetWindowTheme(m_hSliderBitDepth, L"Explorer", NULL);
			SetWindowTheme(m_hChkWavefold, L"Explorer", NULL);
			SetWindowTheme(m_hSliderWavefold, L"Explorer", NULL);
			SetWindowTheme(m_hChkSampleRateReduction, L"Explorer", NULL);
			SetWindowTheme(m_hSliderSampleRateReduction, L"Explorer", NULL);
			SetWindowTheme(m_hChkMirrorH, L"Explorer", NULL);
			SetWindowTheme(m_hChkMirrorV, L"Explorer", NULL);
			SetWindowTheme(m_hChkInvert, L"Explorer", NULL);
			SetWindowTheme(m_hChkReverse, L"Explorer", NULL);
		}

		void WinApplication::CreateSpectralEffectsTab() {
			// ===== SPECTRAL EFFECTS TAB (Tab 13) =====
			HWND spectralPage = m_hTabPage[13];
			int y = 10;
			int controlY = 0;

			// === SPECTRAL DECAY GROUPBOX ===
			CreateWindow(L"BUTTON", L"Spectral Decay", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, y, 590, 60, spectralPage, nullptr, nullptr, nullptr);

			controlY = y + 20;

			m_hChkSpectralDecay = CreateWindow(L"BUTTON", L"Enable", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				20, controlY, 60, 20, spectralPage, nullptr, nullptr, nullptr);

			CreateWindow(L"STATIC", L"Amount:", WS_VISIBLE | WS_CHILD,
				90, controlY + 2, 50, 20, spectralPage, nullptr, nullptr, nullptr);

			m_hSliderSpectralDecayAmount = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				145, controlY, 150, 20, spectralPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderSpectralDecayAmount, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
			SendMessage(m_hSliderSpectralDecayAmount, TBM_SETPOS, TRUE, 0);

			m_hLabelSpectralDecayAmount = CreateWindow(L"STATIC", L"0%", WS_VISIBLE | WS_CHILD,
				300, controlY + 2, 40, 20, spectralPage, nullptr, nullptr, nullptr);

			CreateWindow(L"STATIC", L"Curve:", WS_VISIBLE | WS_CHILD,
				370, controlY + 2, 40, 20, spectralPage, nullptr, nullptr, nullptr);

			m_hSliderSpectralDecayCurve = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				420, controlY, 130, 20, spectralPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderSpectralDecayCurve, TBM_SETRANGE, TRUE, MAKELONG(10, 50));
			SendMessage(m_hSliderSpectralDecayCurve, TBM_SETPOS, TRUE, 10); // Default 1.0

			m_hLabelSpectralDecayCurve = CreateWindow(L"STATIC", L"1.0", WS_VISIBLE | WS_CHILD,
				555, controlY + 2, 40, 20, spectralPage, nullptr, nullptr, nullptr);

			y += 70; // 60 (height) + 10 (gap)

			// === SPECTRAL TILT GROUPBOX ===
			CreateWindow(L"BUTTON", L"Spectral Tilt", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, y, 590, 60, spectralPage, nullptr, nullptr, nullptr);

			controlY = y + 20;

			m_hChkSpectralTilt = CreateWindow(L"BUTTON", L"Enable", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				20, controlY, 60, 20, spectralPage, nullptr, nullptr, nullptr);

			CreateWindow(L"STATIC", L"-Bass / +Treble:", WS_VISIBLE | WS_CHILD,
				90, controlY + 2, 85, 20, spectralPage, nullptr, nullptr, nullptr);

			m_hSliderSpectralTilt = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				180, controlY, 370, 20, spectralPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderSpectralTilt, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
			SendMessage(m_hSliderSpectralTilt, TBM_SETPOS, TRUE, 100); // Default 0 (center)

			m_hLabelSpectralTilt = CreateWindow(L"STATIC", L"0%", WS_VISIBLE | WS_CHILD,
				555, controlY + 2, 40, 20, spectralPage, nullptr, nullptr, nullptr);

			y += 70; // 60 (height) + 10 (gap)

			// === SPECTRAL GATE GROUPBOX ===
			CreateWindow(L"BUTTON", L"Spectral Gate", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, y, 590, 60, spectralPage, nullptr, nullptr, nullptr);

			controlY = y + 20;

			m_hChkSpectralGate = CreateWindow(L"BUTTON", L"Enable", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				20, controlY, 60, 20, spectralPage, nullptr, nullptr, nullptr);

			CreateWindow(L"STATIC", L"Threshold:", WS_VISIBLE | WS_CHILD,
				90, controlY + 2, 60, 20, spectralPage, nullptr, nullptr, nullptr);

			m_hSliderSpectralGate = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				155, controlY, 395, 20, spectralPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderSpectralGate, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
			SendMessage(m_hSliderSpectralGate, TBM_SETPOS, TRUE, 0);

			m_hLabelSpectralGate = CreateWindow(L"STATIC", L"0%", WS_VISIBLE | WS_CHILD,
				555, controlY + 2, 40, 20, spectralPage, nullptr, nullptr, nullptr);

			y += 70; // 60 (height) + 10 (gap)

			// === SPECTRAL SHIFT GROUPBOX ===
			CreateWindow(L"BUTTON", L"Spectral Shift", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, y, 590, 60, spectralPage, nullptr, nullptr, nullptr);

			controlY = y + 20;

			m_hChkSpectralShift = CreateWindow(L"BUTTON", L"Enable", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				20, controlY, 60, 20, spectralPage, nullptr, nullptr, nullptr);

			CreateWindow(L"STATIC", L"Shift Bins:", WS_VISIBLE | WS_CHILD,
				90, controlY + 2, 60, 20, spectralPage, nullptr, nullptr, nullptr);

			m_hSliderSpectralShift = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				155, controlY, 395, 20, spectralPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderSpectralShift, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
			SendMessage(m_hSliderSpectralShift, TBM_SETPOS, TRUE, 100); // Center 0

			m_hLabelSpectralShift = CreateWindow(L"STATIC", L"0", WS_VISIBLE | WS_CHILD,
				555, controlY + 2, 40, 20, spectralPage, nullptr, nullptr, nullptr);

			y += 70; // 60 (height) + 10 (gap)			

			// === PHASE RANDOMIZATION GROUPBOX ===
			CreateWindow(L"BUTTON", L"Phase Randomization", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, y, 590, 60, spectralPage, nullptr, nullptr, nullptr);

			controlY = y + 20;

			m_hChkPhaseRandomize = CreateWindow(L"BUTTON", L"Enable", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				20, controlY, 60, 20, spectralPage, nullptr, nullptr, nullptr);

			CreateWindow(L"STATIC", L"Amount:", WS_VISIBLE | WS_CHILD,
				90, controlY + 2, 50, 20, spectralPage, nullptr, nullptr, nullptr);

			m_hSliderPhaseRandomize = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				145, controlY, 405, 20, spectralPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderPhaseRandomize, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
			SendMessage(m_hSliderPhaseRandomize, TBM_SETPOS, TRUE, 0);

			m_hLabelPhaseRandomize = CreateWindow(L"STATIC", L"0%", WS_VISIBLE | WS_CHILD,
				555, controlY + 2, 40, 20, spectralPage, nullptr, nullptr, nullptr);

			// Apply visual styles to spectral effects tab controls
			SetWindowTheme(m_hChkSpectralDecay, L"Explorer", NULL);
			SetWindowTheme(m_hSliderSpectralDecayAmount, L"Explorer", NULL);
			SetWindowTheme(m_hSliderSpectralDecayCurve, L"Explorer", NULL);
			SetWindowTheme(m_hChkSpectralTilt, L"Explorer", NULL);
			SetWindowTheme(m_hSliderSpectralTilt, L"Explorer", NULL);
			SetWindowTheme(m_hChkSpectralGate, L"Explorer", NULL);
			SetWindowTheme(m_hSliderSpectralGate, L"Explorer", NULL);
			SetWindowTheme(m_hChkPhaseRandomize, L"Explorer", NULL);
			SetWindowTheme(m_hSliderPhaseRandomize, L"Explorer", NULL);
			SetWindowTheme(m_hChkSpectralShift, L"Explorer", NULL);
			SetWindowTheme(m_hSliderSpectralShift, L"Explorer", NULL);
		}

		void WinApplication::CreateSettingsTab() {
			// ===== SETTINGS TAB (Tab 14) =====
			HWND settingsPage = m_hTabPage[14];
			int sy = 10;

			// === OUTPUT SETTINGS GROUPBOX ===
			CreateWindow(L"BUTTON", L"Output Settings", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, sy, 590, 90, settingsPage, nullptr, nullptr, nullptr);
			sy += 24;

			CreateWindow(L"STATIC", L"Folder:", WS_VISIBLE | WS_CHILD,
				20, sy + 2, 50, 20, settingsPage, nullptr, nullptr, nullptr);
			m_hEditPath = CreateWindow(L"EDIT", L"C:\\Wavetables",
				WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
				75, sy, 400, 20, settingsPage, nullptr, nullptr, nullptr);
			m_hBtnBrowse = CreateWindow(L"BUTTON", L"Browse...",
				WS_VISIBLE | WS_CHILD,
				480, sy - 2, 90, 24, settingsPage, (HMENU)CMD_BROWSE_FOLDER, nullptr, nullptr);
			sy += 28;

			CreateWindow(L"STATIC", L"Format:", WS_VISIBLE | WS_CHILD,
				20, sy + 3, 50, 20, settingsPage, nullptr, nullptr, nullptr);

			m_hComboOutputFormat = CreateWindow(L"COMBOBOX", nullptr,
				WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
				75, sy, 130, 150, settingsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hComboOutputFormat, CB_ADDSTRING, 0, (LPARAM)L"WT (Wavetable)");
			SendMessage(m_hComboOutputFormat, CB_ADDSTRING, 0, (LPARAM)L"WAV (Audio)");
			SendMessage(m_hComboOutputFormat, CB_SETCURSEL, 0, 0);

			CreateWindow(L"STATIC", L"Count:", WS_VISIBLE | WS_CHILD,
				230, sy + 3, 45, 20, settingsPage, nullptr, nullptr, nullptr);

			m_hEditCount = CreateWindow(L"EDIT", L"1",
				WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
				280, sy + 1, 50, 20, settingsPage, (HMENU)8, nullptr, nullptr);

			sy += 48;

			// === GENERATION OPTIONS GROUPBOX ===
			CreateWindow(L"BUTTON", L"Generation Options", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, sy, 590, 60, settingsPage, nullptr, nullptr, nullptr);

			sy += 24;

			m_hChkAudioPreview = CreateWindow(L"BUTTON", L"Audio Preview",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
				20, sy, 120, 20, settingsPage, (HMENU)CMD_AUDIO_PREVIEW, nullptr, nullptr);

			m_hChkEnableMorphing = CreateWindow(L"BUTTON", L"Enable Morphing",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
				160, sy, 130, 20, settingsPage, (HMENU)11, nullptr, nullptr);

			SendMessage(m_hChkEnableMorphing, BM_SETCHECK, BST_CHECKED, 0);

			CreateWindow(L"STATIC", L"Frames:", WS_VISIBLE | WS_CHILD,
				310, sy + 2, 50, 18, settingsPage, nullptr, nullptr, nullptr);

			m_hComboNumFrames = CreateWindow(L"COMBOBOX", nullptr,
				WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
				365, sy - 2, 70, 150, settingsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hComboNumFrames, CB_ADDSTRING, 0, (LPARAM)L"1");
			SendMessage(m_hComboNumFrames, CB_ADDSTRING, 0, (LPARAM)L"64");
			SendMessage(m_hComboNumFrames, CB_ADDSTRING, 0, (LPARAM)L"128");
			SendMessage(m_hComboNumFrames, CB_ADDSTRING, 0, (LPARAM)L"256");
			SendMessage(m_hComboNumFrames, CB_ADDSTRING, 0, (LPARAM)L"512");
			SendMessage(m_hComboNumFrames, CB_ADDSTRING, 0, (LPARAM)L"1024");
			SendMessage(m_hComboNumFrames, CB_SETCURSEL, 3, 0);

			sy += 46;

			// === ADVANCED SETTINGS GROUPBOX ===
			CreateWindow(L"BUTTON", L"Advanced Settings", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
				10, sy, 590, 90, settingsPage, nullptr, nullptr, nullptr);
			sy += 24;

			CreateWindow(L"STATIC", L"Max Harmonics (1-16):", WS_VISIBLE | WS_CHILD,
				20, sy + 2, 140, 20, settingsPage, nullptr, nullptr, nullptr);

			m_hSliderMaxHarmonics = CreateWindow(TRACKBAR_CLASS, nullptr,
				WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
				165, sy, 250, 20, settingsPage, nullptr, nullptr, nullptr);

			SendMessage(m_hSliderMaxHarmonics, TBM_SETRANGE, TRUE, MAKELONG(1, 16));
			SendMessage(m_hSliderMaxHarmonics, TBM_SETPOS, TRUE, 8);

			m_hLabelMaxHarmonics = CreateWindow(L"STATIC", L"8 (Balanced)",
				WS_VISIBLE | WS_CHILD,
				420, sy + 2, 150, 20, settingsPage, nullptr, nullptr, nullptr);
			sy += 28;

			// Warning label
			CreateWindow(L"STATIC", L"Higher values (> 8) may cause aliasing. Use with caution!",
				WS_VISIBLE | WS_CHILD | SS_LEFT,
				20, sy, 560, 20, settingsPage, nullptr, nullptr, nullptr);
		}

		void WinApplication::CreateBottomControls(HWND hwnd) {
			// Bottom controls section - waveform buttons only
			int bottomY = 455;
			int buttonPos = bottomY - 2;
			int statusY = bottomY + 30;

			// Waveform utility buttons
			CreateWindow(L"STATIC", L"Waveforms:", WS_VISIBLE | WS_CHILD,
				15, bottomY + 2, 75, 20, hwnd, nullptr, nullptr, nullptr);

			CreateWindow(L"BUTTON", L"Select All",
				WS_VISIBLE | WS_CHILD,
				86, buttonPos, 75, 24, hwnd, (HMENU)CMD_SELECT_ALL, nullptr, nullptr);

			CreateWindow(L"BUTTON", L"Randomize",
				WS_VISIBLE | WS_CHILD,
				164, buttonPos, 75, 24, hwnd, (HMENU)CMD_RANDOMIZE_WAVEFORMS, nullptr, nullptr);

			CreateWindow(L"BUTTON", L"Clear All",
				WS_VISIBLE | WS_CHILD,
				242, buttonPos, 75, 24, hwnd, (HMENU)CMD_CLEAR_ALL, nullptr, nullptr);

			CreateWindow(L"BUTTON", L"Reset Sliders",
				WS_VISIBLE | WS_CHILD,
				320, buttonPos, 90, 24, hwnd, (HMENU)CMD_RESET_SLIDERS, nullptr, nullptr);

			// Random Waves controls
			CreateWindow(L"STATIC", L"Number:", WS_VISIBLE | WS_CHILD,
				450, bottomY + 2, 55, 20, hwnd, nullptr, nullptr, nullptr);

			m_hEditMinWaves = CreateWindow(L"EDIT", L"1",
				WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
				508, bottomY, 35, 20, hwnd, nullptr, nullptr, nullptr);

			CreateWindow(L"STATIC", L"-", WS_VISIBLE | WS_CHILD | SS_CENTER,
				546, bottomY, 10, 20, hwnd, nullptr, nullptr, nullptr);

			m_hEditMaxWaves = CreateWindow(L"EDIT", L"4",
				WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
				558, bottomY, 35, 20, hwnd, nullptr, nullptr, nullptr);

			// Progress bar (initially hidden) and status bar at bottom row
			m_hProgressBar = CreateWindowEx(
				0, PROGRESS_CLASS, nullptr,
				WS_CHILD | PBS_SMOOTH,
				15, statusY, 482, 20, hwnd, nullptr, m_hInstance, nullptr);

			SendMessage(m_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
			SendMessage(m_hProgressBar, PBM_SETPOS, 0, 0);

			// Status bar at bottom
			m_hStatus = CreateWindow(L"STATIC", L"Ready", WS_VISIBLE | WS_CHILD,
				15, statusY, 450, 20, hwnd, nullptr, nullptr, nullptr);

			// Generate/Cancel button
			m_hBtnGenerate = CreateWindow(L"BUTTON", L"Generate",
				WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
				506, statusY - 2, 75, 24, hwnd, (HMENU)CMD_GENERATE, nullptr, nullptr);

			// Exit button
			m_hBtnExit = CreateWindow(L"BUTTON", L"Exit",
				WS_VISIBLE | WS_CHILD,
				585, statusY - 2, 75, 24, hwnd, (HMENU)CMD_EXIT, nullptr, nullptr);
		}
	}
}