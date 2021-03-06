//  ================================================================
//  Created by Gregory Kramida on 10/18/17.
//  Copyright (c) 2017-2000 Gregory Kramida
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at

//  http://www.apache.org/licenses/LICENSE-2.0

//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//  ================================================================
#pragma once

//stdlib
#include <array>

//local
#include "FusionAlgorithm.h"
#include "CameraTrackingController.h"
#include "../ImageProcessing/Interface/ImageProcessingEngineInterface.h"
#include "../Meshing/Interface/MeshingEngine.h"
#include "../ViewBuilder/Interface/ViewBuilder.h"
#include "../Rendering/Interface/RenderingEngineInterface.h"
#include "../Indexing/Interface/IndexingEngine.h"
#include "../DepthFusion/DepthFusionEngine.h"
#include "../VolumeFusion/VolumeFusionEngine.h"
#include "../Swapping/Interface/SwappingEngine.h"
#include "../Telemetry/TelemetryRecorder.h"
#include "../../Objects/Misc/IMUCalibrator.h"
#include "../../CameraTrackers/Interface/CameraTracker.h"
#include "../LevelSetAlignment/Interface/LevelSetAlignmentEngine.h"
#include "../../../FernRelocLib/Relocaliser.h"
#include "../../Utils/Configuration/AutomaticRunSettings.h"


namespace ITMLib {

template<typename TVoxel, typename TWarp, typename TIndex>
class DynamicSceneVoxelEngine : public FusionAlgorithm {
private: // instance variables
	static constexpr int live_volume_count = 2;

	bool camera_tracking_enabled, fusion_active, main_processing_active, tracking_initialised;
	int frames_processed, relocalization_count;

	configuration::Configuration config;
	const AutomaticRunSettings automatic_run_settings;

	// engines
	ImageProcessingEngineInterface* image_processing_engine;
	RenderingEngineBase<TVoxel, TIndex>* rendering_engine;
	MeshingEngine<TVoxel, TIndex>* meshing_engine;
	IndexingEngineInterface<TVoxel, TIndex>* indexing_engine;
	DepthFusionEngineInterface<TVoxel, TIndex>* depth_fusion_engine;
	VolumeFusionEngineInterface<TVoxel, TIndex>* volume_fusion_engine;
	SwappingEngine<TVoxel, TIndex>* swapping_engine;

	TelemetryRecorderInterface<TVoxel, TWarp, TIndex>& telemetry_recorder;
	ViewBuilder* view_builder;
	CameraTrackingController* camera_tracking_controller;

	CameraTracker* camera_tracker;
	IMUCalibrator* imu_calibrator;
	LevelSetAlignmentEngineInterface<TVoxel, TWarp, TIndex>* surface_tracker;

	VoxelVolume<TVoxel, TIndex>* canonical_volume;
	VoxelVolume<TVoxel, TIndex>** live_volumes;
	VoxelVolume<TVoxel, TIndex>* target_warped_live_volume = nullptr;
	VoxelVolume<TWarp, TIndex>* warp_field;
	RenderState* canonical_render_state;
	RenderState* live_render_state;
	RenderState* freeview_render_state;


	FernRelocLib::Relocaliser<float>* relocalizer;
	UChar4Image* keyframe_raycast;

	/// The current input frame data
	View* view;

	/// Current camera pose and additional tracking information
	CameraTrackingState* tracking_state;
	CameraTrackingState::TrackingResult last_tracking_result;
	ORUtils::SE3Pose previous_frame_pose;

public: // instance functions

	DynamicSceneVoxelEngine(const RGBD_CalibrationInformation& calibration_info, Vector2i rgb_image_size, Vector2i depth_image_size);
	~DynamicSceneVoxelEngine() override;

	View* GetView() override { return view; }

	CameraTrackingState* GetTrackingState() override { return tracking_state; }

	CameraTrackingState::TrackingResult ProcessFrame(UChar4Image* rgb_image, ShortImage* depth_image,
	                                                 IMUMeasurement* imu_measurement = nullptr) override;

	/// Extracts a mesh from the current volume and saves it to the disk at the specified path
	void SaveVolumeToMesh(const std::string& path) override;

	/// save and load the full volume and relocalizer (if any) to/from file
	void SaveToFile() override;
	void SaveToFile(const std::string& path) override;
	void LoadFromFile() override;
	void LoadFromFile(const std::string& path) override;

	Vector2i GetImageSize() const override;

	/// Get a render of the result
	void GetImage(UChar4Image* out, GetImageType type,
	              ORUtils::SE3Pose* pose = nullptr, Intrinsics* intrinsics = nullptr) override;

	void ResetAll() override;

	void TurnOnTracking() override;
	void TurnOffTracking() override;

	// Here "integration" == "fusion", i.e. integration of new data into existing model
	void TurnOnIntegration() override;
	void TurnOffIntegration() override;

	void TurnOnMainProcessing() override;
	void TurnOffMainProcessing() override;

	virtual bool GetMainProcessingOn() const override;

private: // instance functions
	void Reset();
	void InitializeScenes();
	//TODO: move to SwappingEngine itself.
	void ProcessSwapping(RenderState* render_state);
	void HandlePotentialCameraTrackingFailure();
	int FrameIndex();
	void AddFrameIndexToImage(UChar4Image& out);

};
} // namespace ITMLib
