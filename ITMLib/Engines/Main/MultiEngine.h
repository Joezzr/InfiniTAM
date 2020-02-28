// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

#include "MainEngine.h"
#include "CameraTrackingController.h"
#include "../LowLevel/Interface/LowLevelEngine.h"
#include "../ViewBuilding/Interface/ViewBuilder.h"
#include "../../Objects/Misc/IMUCalibrator.h"
#include "../../../FernRelocLib/Relocaliser.h"

#include "../MultiScene/ActiveMapManager.h"
#include "../MultiScene/GlobalAdjustmentEngine.h"
#include "../Visualization/Interface/MultiVisualizationEngine.h"
#include "../Meshing/MultiMeshingEngineFactory.h"
#include "../../CameraTrackers/Interface/CameraTracker.h"

#include <vector>

namespace ITMLib
{
	/** \brief
	*/
	template <typename TVoxel, typename TIndex>
	class MultiEngine : public MainEngine
	{
	private:

		LowLevelEngine *lowLevelEngine;
		VisualizationEngine<TVoxel, TIndex>* visualization_engine;
		MultiVisualizationEngine<TVoxel, TIndex> *multiVisualizationEngine;

		MultiMeshingEngine<TVoxel, TIndex> *meshingEngine;

		ViewBuilder *viewBuilder;
		CameraTrackingController *trackingController;
		CameraTracker *tracker;
		IMUCalibrator *imuCalibrator;
		DenseMapper<TVoxel, TIndex> *denseMapper;

		FernRelocLib::Relocaliser<float> *relocaliser;

		VoxelMapGraphManager<TVoxel, TIndex> *mapManager;
		ActiveMapManager *mActiveDataManager;
		GlobalAdjustmentEngine *mGlobalAdjustmentEngine;
		bool mScheduleGlobalAdjustment;

		Vector2i trackedImageSize;
		RenderState *renderState_freeview;
		RenderState *renderState_multiscene;
		int freeviewLocalMapIdx;

		/// Pointer for storing the current input frame
		View *view;
	public:
		View* GetView() { return view; }

		CameraTrackingState* GetTrackingState(void);

		/// Process a frame with rgb and depth images and (optionally) a corresponding imu measurement
		CameraTrackingState::TrackingResult ProcessFrame(ITMUChar4Image *rgbImage, ITMShortImage *rawDepthImage, IMUMeasurement *imuMeasurement = NULL);

		/// Get a result image as output
		Vector2i GetImageSize(void) const;

		void GetImage(ITMUChar4Image *out, GetImageType getImageType, ORUtils::SE3Pose *pose = NULL, Intrinsics *intrinsics = NULL);

		void changeFreeviewLocalMapIdx(ORUtils::SE3Pose *pose, int newIdx);
		void setFreeviewLocalMapIdx(int newIdx)
		{
			freeviewLocalMapIdx = newIdx;
		}
		int getFreeviewLocalMapIdx(void) const
		{
			return freeviewLocalMapIdx;
		}
		int findPrimaryLocalMapIdx(void) const
		{
			return mActiveDataManager->findPrimaryLocalMapIdx();
		}

		/// Extracts a mesh from the current scene and saves it to the model file specified by the file name
		void SaveSceneToMesh(const char *fileName) override;

		/// save and load the full scene and relocaliser (if any) to/from file
		void SaveToFile() override;
		void LoadFromFile() override;

		/// resets the scene and the tracker
		void resetAll() override;

		/// switch for turning tracking on/off
		void turnOnTracking() override;
		void turnOffTracking() override;

		/// switch for turning integration on/off
		void turnOnIntegration() override;
		void turnOffIntegration() override;

		/// switch for turning main processing on/off
		void turnOnMainProcessing() override;
		void turnOffMainProcessing() override;

		/** \brief Constructor
			Ommitting a separate image size for the depth images
			will assume same resolution as for the RGB images.
		*/
		MultiEngine(const RGBDCalib& calib, Vector2i imgSize_rgb, Vector2i imgSize_d);
		~MultiEngine(void);
	};
}