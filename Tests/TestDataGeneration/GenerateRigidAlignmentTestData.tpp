//  ================================================================
//  Created by Gregory Kramida (https://github.com/Algomorph) on 12/24/20.
//  Copyright (c) 2020 Gregory Kramida
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
// === local ===
#include "GenerateRigidAlignmentTestData.h"
// === ORUtils ===
#include "../../ORUtils/MemoryDeviceType.h"
#include "../../ORUtils/MathTypePersistence/MathTypePersistence.h"
// === ITMLib ===
#include "../../ITMLib/Utils/Logging/Logging.h"
#include "../../ITMLib/Objects/Views/View.h"
#include "../../ITMLib/Objects/Tracking/CameraTrackingState.h"
#include "../../ITMLib/Engines/ImageProcessing/ImageProcessingEngineFactory.h"
#include "../../ITMLib/Engines/Indexing/IndexingEngineFactory.h"
#include "../../ITMLib/Engines/DepthFusion/DepthFusionEngineFactory.h"
#include "../../ITMLib/Engines/Rendering/RenderingEngineFactory.h"
#include "../../ITMLib/CameraTrackers/CameraTrackerFactory.h"
#include "../../ITMLib/Engines/Indexing/Interface/IndexingEngine.h"

// === Test Utilities ===
#include "../TestUtilities/TestDataUtilities.h"
#include "../TestUtilities/RigidTrackerPresets.h"



using namespace ITMLib;
using namespace test;

template<typename TIndex, MemoryDeviceType TMemoryDeviceType>
void GenerateRigidAlignmentTestData() {
	LOG4CPLUS_INFO(log4cplus::Logger::getRoot(),
	               "Generating rigid alignment test data (" << IndexString<TIndex>() << ", "
	                                                        << DeviceString<TMemoryDeviceType>() << ") ...");
	View* view = nullptr;

	// generate teddy_volume_115 for teddy scene frame 115
	UpdateView(&view,
	           teddy::frame_115_depth_path.ToString(),
	           teddy::frame_115_color_path.ToString(),
	           teddy::calibration_path.ToString(),
	           TMemoryDeviceType);

	VoxelVolume<TSDFVoxel_f_rgb, TIndex> teddy_volume_115(teddy::DefaultVolumeParameters(), false, TMemoryDeviceType,
	                                                      teddy::InitializationParameters<TIndex>());
	teddy_volume_115.Reset();
	IndexingEngineInterface<TSDFVoxel_f_rgb, TIndex>* indexing_engine = IndexingEngineFactory::Build<TSDFVoxel_f_rgb, TIndex>(TMemoryDeviceType);
	CameraTrackingState camera_tracking_state(teddy::frame_image_size, TMemoryDeviceType);
	indexing_engine->AllocateNearSurface(&teddy_volume_115, view, &camera_tracking_state);
	DepthFusionEngineInterface<TSDFVoxel_f_rgb, TIndex>* depth_fusion_engine = DepthFusionEngineFactory::Build<TSDFVoxel_f_rgb, TIndex>(
			TMemoryDeviceType);
	depth_fusion_engine->IntegrateDepthImageIntoTsdfVolume(&teddy_volume_115, view);


	ConstructGeneratedVolumeSubdirectoriesIfMissing();
	teddy_volume_115.SaveToDisk(teddy::Volume115Path<TIndex>());

	// generate rigid tracker outputs for next frame in the sequence

	UpdateView(&view,
	           teddy::frame_116_depth_path.ToString(),
	           teddy::frame_116_color_path.ToString(),
	           teddy::calibration_path.ToString(),
	           TMemoryDeviceType);

	IMUCalibrator* imu_calibrator = new ITMIMUCalibrator_iPad();
	ImageProcessingEngineInterface* image_processing_engine = ImageProcessingEngineFactory::Build(TMemoryDeviceType);

	ConstructGeneratedMatrixDirectoryIfMissing();

	// the rendering engine will generate the point cloud
	auto rendering_engine = ITMLib::RenderingEngineFactory::Build<TSDFVoxel_f_rgb, TIndex>(TMemoryDeviceType);
	// we need a dud rendering state also for the point cloud
	RenderState render_state(teddy::frame_image_size,
	                         teddy::DefaultVolumeParameters().near_clipping_distance, teddy::DefaultVolumeParameters().far_clipping_distance,
	                         TMemoryDeviceType);

	for (auto& pair : test::matrix_file_name_by_preset) {
		//__DEBUG
		// auto it = std::find(test::depth_tracker_presets.begin(), test::depth_tracker_presets.end(), pair.first);
		// if (it == test::depth_tracker_presets.end()) {
		// 	continue;
		// }
		if (std::string(pair.first) != test::depth_tracker_preset_default) {
			continue;
		}
		CameraTrackingState tracking_state(teddy::frame_image_size, TMemoryDeviceType);
		rendering_engine->CreatePointCloud(&teddy_volume_115, view, &tracking_state, &render_state);
		const std::string& preset = pair.first;
		const std::string& matrix_filename = pair.second;
		CameraTracker* tracker = CameraTrackerFactory::Instance().Make(
				TMemoryDeviceType, preset.c_str(), teddy::frame_image_size, teddy::frame_image_size,
				image_processing_engine, imu_calibrator,
				teddy::DefaultVolumeParameters());

		//__DEBUG
		// ORUtils::OStreamWrapper debug_writer(test::generated_arrays_directory.ToString() + "/debug.dat");
		// debug_writer << *tracking_state.point_cloud;
		// debug_writer << *view;


		tracker->TrackCamera(&tracking_state, view);
		std::string matrix_path = test::generated_matrix_directory.ToString() + "/" + matrix_filename;
		ORUtils::OStreamWrapper matrix_writer(matrix_path, false);
		matrix_writer << tracking_state.pose_d->GetM();

		//__DEBUG
		std::cout << preset << ": " << std::endl << tracking_state.pose_d->GetM() << std::endl;

		tracking_state.pose_d->SetM(Matrix4f::Identity());
		delete tracker;
	}

	delete imu_calibrator;
	delete image_processing_engine;
	delete indexing_engine;
	delete depth_fusion_engine;
	delete view;
}