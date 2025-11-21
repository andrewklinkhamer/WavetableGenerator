#ifndef WINAPPLICATION_H
#define WINAPPLICATION_H

#include "Include.h"
#include "../Core/IWavetableGenerator.h"
#include "../Utils/XorShift128Plus.h"
#include "../Core/RandomWavetableGenerator.h"
#include "../Core/WavetableImporter.h"

namespace WavetableGen {
	namespace UI {
		using namespace Core;
		using namespace Utils;
		using namespace Services;
		using namespace IO;

		class WinApplication {
		public:
			explicit WinApplication(IWavetableGenerator& wavetableGenerator, HINSTANCE hInstance);
			~WinApplication();

			// Run the application
			int Run(int nCmdShow);

		private:
			// Window procedure (static to work with Win32 API)
			static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

			static LRESULT CALLBACK PaneProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

			// Instance window procedure
			LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

			// Helper methods
			std::vector<std::pair<WaveType, float>> GetStartFrameWaves();
			std::vector<std::pair<WaveType, float>> GetEndFrameWaves();
			void SetFontForAllChildren(HWND hwndParent) const;
			void ErrorMessage(HWND hwnd, const wchar_t* msg);
			void InfoMessage(HWND hwnd, const wchar_t* msg);
			const wchar_t* GetErrorMessage(GenerationResult result);
			EffectsSettings GetEffectsSettings();
			MorphCurve GetMorphCurve();
			double GetPWMDutyCycle();

			// Helper to build available waveforms list from UI
			std::vector<RandomWavetableGenerator::AvailableWaveform> GetAvailableWaveforms();

			// Structure to track waveform controls
			struct WaveCheckbox {
				HWND hwnd;
				HWND hSliderStart;
				HWND hSliderEnd;
				WaveType type;
			};

			// Member variables
			HINSTANCE m_hInstance;
			HWND m_hwnd;
			HFONT m_hFont;
			HBRUSH m_hBkgBrush;
			IWavetableGenerator& m_wavetableGenerator;
			Utils::XorShift128Plus m_rng;
			RandomWavetableGenerator m_randomGenerator;

			// Control handles
			HWND m_hEditPath;
			HWND m_hBtnBrowse;
			HWND m_hEditCount;
			HWND m_hEditMinWaves;
			HWND m_hEditMaxWaves;
			HWND m_hBtnGenerate;
			HWND m_hChkAudioPreview;
			HWND m_hChkEnableMorphing;
			HWND m_hComboNumFrames;
			HWND m_hComboOutputFormat;
			HWND m_hStatus;
			HWND m_hTabControl;

			// Tab page HWNDs
			HWND m_hTabPage[15];  // 15 tabs (11 waveform tabs + Import + Effects + Spectral Effects + Settings)

			// Effects tab controls
			HWND m_hSliderPWMDuty;
			HWND m_hLabelPWMDuty;
			HWND m_hComboMorphCurve;
			HWND m_hComboDistortionType;
			HWND m_hSliderDistortionAmount;
			HWND m_hLabelDistortionAmount;
			HWND m_hChkLowPass;
			HWND m_hSliderLowPassCutoff;
			HWND m_hLabelLowPassCutoff;
			HWND m_hChkHighPass;
			HWND m_hSliderHighPassCutoff;
			HWND m_hLabelHighPassCutoff;
			HWND m_hChkBitCrush;
			HWND m_hSliderBitDepth;
			HWND m_hLabelBitDepth;
			HWND m_hChkMirrorH;
			HWND m_hChkMirrorV;
			HWND m_hChkInvert;
			HWND m_hChkReverse;
			HWND m_hChkWavefold;
			HWND m_hSliderWavefold;
			HWND m_hLabelWavefold;
			HWND m_hChkSampleRateReduction;
			HWND m_hSliderSampleRateReduction;
			HWND m_hLabelSampleRateReduction;
			HWND m_hChkSpectralDecay;
			HWND m_hSliderSpectralDecayAmount;
			HWND m_hLabelSpectralDecayAmount;
			HWND m_hSliderSpectralDecayCurve;
			HWND m_hLabelSpectralDecayCurve;
			HWND m_hChkSpectralTilt;
			HWND m_hSliderSpectralTilt;
			HWND m_hLabelSpectralTilt;
			HWND m_hChkSpectralGate;
			HWND m_hSliderSpectralGate;
			HWND m_hLabelSpectralGate;
			HWND m_hChkPhaseRandomize;
			HWND m_hSliderPhaseRandomize;
			HWND m_hLabelPhaseRandomize;
			HWND m_hChkSpectralShift;
			HWND m_hSliderSpectralShift;
			HWND m_hLabelSpectralShift;

			// Import controls
			HWND m_hBtnImport;
			HWND m_hBtnClearImport;
			HWND m_hLabelImportInfo;
			HWND m_hComboImportedFrame;
			HWND m_hBtnUseAsStart;
			HWND m_hBtnUseAsEnd;

			// Advanced settings controls
			HWND m_hSliderMaxHarmonics;
			HWND m_hLabelMaxHarmonics;

			// Progress and control buttons
			HWND m_hProgressBar;
			HWND m_hBtnExit;

			// Thread management
			HANDLE m_hWorkerThread;
			DWORD m_dwWorkerThreadId;
			volatile bool m_bCancelGeneration;

			std::vector<WaveCheckbox> m_waveCheckboxes;

			// Imported wavetable data
			ImportedWavetable m_importedWavetable;
			WavetableImporter m_importer;

			// Helper methods for applying imported frames to UI
			void ApplyWaveformsToStartFrame(const std::vector<std::pair<WaveType, float>>& waveforms);
			void ApplyWaveformsToEndFrame(const std::vector<std::pair<WaveType, float>>& waveforms);

			// Get max harmonics value from UI
			int GetMaxHarmonics();

			// UI state management
			void EnableGenerationControls(bool enable);

			// Helper methods for reading control values
			int GetIntFromEditControl(HWND hEdit);
			int GetCheckboxState(HWND hCheckbox);
			bool GetCheckboxStateBool(HWND hCheckbox);
			int GetSliderPosition(HWND hSlider);
			int GetComboBoxSelection(HWND hComboBox);

			// Helper methods for setting control values
			void SetCheckboxState(HWND hCheckbox, int value);
			void SetSliderPosition(HWND hSlider, int value);
			void SetComboBoxSelection(HWND hComboBox, int value);

			// Generation helper methods
			void HandleCancelRequest();
			bool GetFolderPath(std::string& outFolderPath);
			int GetGenerationCount();
			void GenerateSingleWavetable(const std::string& folderPath);
			void StartBatchGeneration(const std::string& folderPath, int count);

			// Settings save/load
			void SaveSettings();
			void LoadSettings();

			// UI helper methods - update labels
			void UpdateLabelWithFormat(HWND hLabel, int value, const wchar_t* format);
			void UpdateLabelPercent(HWND hLabel, int value);

			// UI helper methods - update slider labels
			void UpdateMaxHarmonicsLabel(int value);
			void UpdatePWMDutyLabel(int value);
			void UpdateDistortionAmountLabel(int value);
			void UpdateLowPassCutoffLabel(int value);
			void UpdateHighPassCutoffLabel(int value);
			void UpdateBitDepthLabel(int value);
			void UpdateWavefoldAmountLabel(int value);
			void UpdateSpectralDecayAmountLabel(int value);
			void UpdateSpectralDecayCurveLabel(int value);
			void UpdateSpectralTiltLabel(int value);
			void UpdateSpectralGateLabel(int value);
			void UpdatePhaseRandomizeLabel(int value);
			void UpdateSampleRateReductionLabel(int value);
			void UpdateSpectralShiftLabel(int value);

			// UI Creation Helper Methods
			void CreateTabControl(HWND hwnd);
			void CreateTabPages(HWND hwnd);
			void AddWaveform(int tabIdx, int yPos, const wchar_t* name, WaveType type);
			void AddColumnHeaders();
			void CreateBasicTab();
			void CreateChaosTab();
			void CreateFractalsTab();
			void CreateHarmonicTab();
			void CreateInharmonicTab();
			void CreateModernTab();
			void CreateModulationTab();
			void CreatePhysicalTab();
			void CreateSynthesisTab();
			void CreateVintageTab();
			void CreateVowelsTab();
			void CreateImportTab();
			void CreateEffectsTab();
			void CreateSpectralEffectsTab();
			void CreateSettingsTab();
			void CreateBottomControls(HWND hwnd);

			// Worker thread function
			static DWORD WINAPI WorkerThreadProc(LPVOID lpParam);

			// Structure to pass data to worker thread
			struct ThreadData {
				WinApplication* pApp;
				std::string outputFolder;
				int count;
				int minWaves;
				int maxWaves;
				std::vector<RandomWavetableGenerator::AvailableWaveform> availableWaveforms;
				std::string extension;
				OutputFormat format;
				bool isAudioPreview;
				EffectsSettings effects;
				MorphCurve morphCurve;
				double pulseDuty;
				int maxHarmonics;
			};
		};
	}
}

// Custom window messages for thread-to-UI communication
#define WM_GENERATION_PROGRESS (WM_USER + 100)
#define WM_GENERATION_COMPLETE (WM_USER + 101)
#define WM_GENERATION_ERROR (WM_USER + 102)

#endif // WINAPPLICATION_H
