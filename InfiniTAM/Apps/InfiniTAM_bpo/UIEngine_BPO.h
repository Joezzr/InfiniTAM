// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

#include "../../InputSource/ImageSourceEngine.h"
#include "../../InputSource/IMUSourceEngine.h"
#include "../../InputSource/FFMPEGWriter.h"
#include "../../ITMLib/Core/ITMMainEngine.h"
#include "../../ITMLib/Utils/ITMLibSettings.h"
#include "../../ORUtils/FileUtils.h"
#include "../../ORUtils/NVTimer.h"

#include <vector>

namespace InfiniTAM {
namespace Engine {
class UIEngine_BPO {
	static UIEngine_BPO* instance;

	enum MainLoopAction {
		PROCESS_PAUSED, PROCESS_FRAME, PROCESS_FRAME_RECORD, PROCESS_VIDEO, EXIT, SAVE_TO_DISK,
		PROCESS_N_FRAMES, PROCESS_SINGLE_STEP, PROCESS_STEPS_CONTINOUS
	} mainLoopAction;

	struct UIColourMode {
		const char* name;
		ITMLib::ITMMainEngine::GetImageType type;

		UIColourMode(const char* _name, ITMLib::ITMMainEngine::GetImageType _type)
				: name(_name), type(_type) {}
	};
	std::vector<UIColourMode> colourModes_main, colourModes_freeview;
	UIColourMode colourMode_stepByStep = UIColourMode("step_by_step",
	                                                  ITMLib::ITMMainEngine::InfiniTAM_IMAGE_STEP_BY_STEP);
	int currentColourMode;

	int autoIntervalFrameStart;
	int autoIntervalFrameCount;
	int startedProcessingFromFrameIx = 0;

	InputSource::ImageSourceEngine* imageSource;
	InputSource::IMUSourceEngine* imuSource;
	ITMLib::ITMLibSettings internalSettings;
	ITMLib::ITMMainEngine* mainEngine;

	StopWatchInterface* timer_instant;
	StopWatchInterface* timer_average;

private: // For UI layout
	static const int NUM_WIN = 3;
	Vector4f winReg[NUM_WIN]; // (x1, y1, x2, y2)
	Vector2i winSize;
	uint textureId[NUM_WIN];
	ITMUChar4Image* outImage[NUM_WIN];
	ITMLib::ITMMainEngine::GetImageType outImageType[NUM_WIN];

	ITMUChar4Image* inputRGBImage;
	ITMShortImage* inputRawDepthImage;
	ITMLib::ITMIMUMeasurement* inputIMUMeasurement;

	bool freeviewActive;
	bool integrationActive;
	ORUtils::SE3Pose freeviewPose;
	ITMLib::ITMIntrinsics freeviewIntrinsics;

	int mouseState;
	Vector2i mouseLastClick;
	bool mouseWarped; // To avoid the extra motion generated by glutWarpPointer

	int currentFrameNo;
	bool isRecordingImages;
	bool recordWarpUpdatesForNextFrame; // record warp updates during processing of the next frame
	InputSource::FFMPEGWriter* reconstructionVideoWriter;
	InputSource::FFMPEGWriter* rgbVideoWriter;
	InputSource::FFMPEGWriter* depthVideoWriter;
public:
	static UIEngine_BPO* Instance(void) {
		if (instance == NULL) instance = new UIEngine_BPO();
		return instance;
	}

	static void GlutDisplayFunction();
	static void GlutIdleFunction();
	static void GlutKeyUpFunction(unsigned char key, int x, int y);
	static void GlutMouseButtonFunction(int button, int state, int x, int y);
	static void GlutMouseMoveFunction(int x, int y);
	static void GlutMouseWheelFunction(int button, int dir, int x, int y);

	const Vector2i& GetWindowSize(void) const { return winSize; }

	float processedTime;
	int processedFrameNo;
	int trackingResult;
	char* outFolder;
	bool needsRefresh;
	bool inStepByStepMode;
	bool continuousStepByStep;
	ITMUChar4Image* saveImage;

	void Initialise(int& argc, char** argv, InputSource::ImageSourceEngine* imageSource,
		                InputSource::IMUSourceEngine* imuSource, ITMLib::ITMMainEngine* mainEngine,
		                const char* outFolder, ITMLib::ITMLibSettings::DeviceType deviceType,
		                int frameIntervalLength, int skipFirstNFrames, bool recordReconstructionResult,
		                bool startInStepByStep);
	void Shutdown();

	void Run();
	void ProcessFrame();
	//For scene-tracking updates
	bool BeginStepByStepModeForFrame();
	bool ContinueStepByStepModeForFrame();

	void GetScreenshot(ITMUChar4Image* dest) const;
	void SaveScreenshot(const char* filename) const;
	void SkipFrames(int numberOfFramesToSkip);
	void RecordReconstructionToVideo();
	void RecordDepthAndRGBInputToVideo();
	void RecordDepthAndRGBInputToImages();
};
}
}
