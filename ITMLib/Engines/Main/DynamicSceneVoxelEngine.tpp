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

//stdlib
#include <unordered_set>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

//log4cplus
#include <log4cplus/loggingmacros.h>

// local
#include "DynamicSceneVoxelEngine.h"
#include "../ImageProcessing/ImageProcessingEngineFactory.h"
#include "../Meshing/MeshingEngineFactory.h"
#include "../ViewBuilder/ViewBuilderFactory.h"
#include "../Rendering/RenderingEngineFactory.h"
#include "../VolumeFileIO/VolumeFileIOEngine.h"
#include "../VolumeFusion/VolumeFusionEngineFactory.h"
#include "../EditAndCopy/CPU/EditAndCopyEngine_CPU.h"
#include "../Analytics/AnalyticsEngineFactory.h"
#include "../DepthFusion/DepthFusionEngineFactory.h"
#include "../Indexing/IndexingEngineFactory.h"
#include "../Swapping/SwappingEngineFactory.h"
#include "../Analytics/AnalyticsLogging.h"
#include "../Telemetry/TelemetryRecorderFactory.h"
#include "../../CameraTrackers/CameraTrackerFactory.h"
#include "../LevelSetAlignment/LevelSetAlignmentEngineFactory.h"
#include "../../Utils/Analytics/BenchmarkUtilities.h"
#include "../../Utils/Logging/Logging.h"
#include "../../Utils/Quaternions/QuaternionFromMatrix.h"
#include "../../../ORUtils/NVTimer.h"
#include "../../../ORUtils/FileUtils.h"
#include "../../../ORUtils/FileUtils.h"
#include "../../Utils/Telemetry/TelemetryUtilities.h"
#include "../../../ORUtils/VectorAndMatrixPersistence.h"
#include "../../../ORUtils/DrawText.h"

using namespace ITMLib;

template<typename TVoxel, typename TWarp, typename TIndex>
DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::DynamicSceneVoxelEngine(
		const RGBD_CalibrationInformation& calibration_info,
		Vector2i rgb_image_size,
		Vector2i depth_image_size)
		: config(configuration::Get()),
		  camera_tracking_enabled(this->parameters.enable_rigid_alignment),
		  automatic_run_settings(ExtractSerializableStructFromPtreeIfPresent<AutomaticRunSettings>(configuration::Get().source_tree,
		                                                                                           AutomaticRunSettings::default_parse_path,
		                                                                                           configuration::Get().origin)),
		  indexing_engine(IndexingEngineFactory::Build<TVoxel, TIndex>(configuration::Get().device_type)),
		  depth_fusion_engine(
				  DepthFusionEngineFactory::Build<TVoxel, TIndex>
						  (configuration::Get().device_type)),
		  volume_fusion_engine(VolumeFusionEngineFactory::Build<TVoxel, TIndex>(configuration::Get().device_type)),
		  surface_tracker(LevelSetAlignmentEngineFactory::Build<TVoxel, TWarp, TIndex>()),
		  swapping_engine(configuration::Get().swapping_mode != configuration::SWAPPINGMODE_DISABLED ?
		                  SwappingEngineFactory::Build<TVoxel, TIndex>(
				                  configuration::Get().device_type,
				                  configuration::ForVolumeRole<TIndex>(configuration::VOLUME_CANONICAL)
		                  ) : nullptr),
		  image_processing_engine(ImageProcessingEngineFactory::Build(configuration::Get().device_type)),
		  telemetry_recorder(TelemetryRecorderFactory::GetDefault<TVoxel, TWarp, TIndex>(configuration::Get().device_type)),
		  view_builder(ViewBuilderFactory::Build(calibration_info, configuration::Get().device_type)),
		  rendering_engine(RenderingEngineFactory::Build<TVoxel, TIndex>(
				  configuration::Get().device_type)),
		  meshing_engine(config.create_meshing_engine ? MeshingEngineFactory::Build<TVoxel, TIndex>(
				  configuration::Get().device_type) : nullptr) {
	logging::InitializeLogging();


	this->InitializeScenes();

	const MemoryDeviceType device_type = config.device_type;
	MemoryDeviceType memory_device_type = config.device_type;
	if ((depth_image_size.x == -1) || (depth_image_size.y == -1)) depth_image_size = rgb_image_size;

	imu_calibrator = new ITMIMUCalibrator_iPad();
	camera_tracker = CameraTrackerFactory::Instance().Make(rgb_image_size, depth_image_size, image_processing_engine,
	                                                       imu_calibrator,
	                                                       canonical_volume->GetParameters());
	camera_tracking_controller = new CameraTrackingController(camera_tracker);
	//TODO: is "tracked" image size ever different from actual depth image size? If so, document GetTrackedImageSize function. Otherwise, revise.
	Vector2i tracked_image_size = camera_tracking_controller->GetTrackedImageSize(rgb_image_size, depth_image_size);
	tracking_state = new CameraTrackingState(tracked_image_size, memory_device_type);


	canonical_render_state = new RenderState(tracked_image_size,
	                                         canonical_volume->GetParameters().near_clipping_distance,
	                                         canonical_volume->GetParameters().far_clipping_distance,
	                                         config.device_type);
	live_render_state = new RenderState(tracked_image_size, canonical_volume->GetParameters().near_clipping_distance,
	                                    canonical_volume->GetParameters().far_clipping_distance,
	                                    config.device_type);
	freeview_render_state = nullptr; //will be created if needed



	Reset();

	camera_tracker->UpdateInitialPose(tracking_state);

	view = nullptr; // will be allocated by the view builder

	if (config.behavior_on_failure == configuration::FAILUREMODE_RELOCALIZE)
		relocalizer = new FernRelocLib::Relocaliser<float>(depth_image_size,
		                                                   Vector2f(
				                                                   config.general_voxel_volume_parameters.near_clipping_distance,
				                                                   config.general_voxel_volume_parameters.far_clipping_distance),
		                                                   0.2f, 500, 4);
	else relocalizer = nullptr;

	keyframe_raycast = new UChar4Image(depth_image_size, memory_device_type);

	fusion_active = true;
	main_processing_active = true;
	tracking_initialised = false;
	relocalization_count = 0;
	frames_processed = 0;
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::InitializeScenes() {
	configuration::Configuration& settings = configuration::Get();
	MemoryDeviceType memoryType = settings.device_type;
	this->canonical_volume = new VoxelVolume<TVoxel, TIndex>(
			settings.general_voxel_volume_parameters, settings.swapping_mode == configuration::SWAPPINGMODE_ENABLED,
			memoryType, configuration::ForVolumeRole<TIndex>(configuration::VOLUME_CANONICAL));
	this->live_volumes = new VoxelVolume<TVoxel, TIndex>* [2];
	for (int iLiveScene = 0;
	     iLiveScene < DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::live_volume_count; iLiveScene++) {
		this->live_volumes[iLiveScene] = new VoxelVolume<TVoxel, TIndex>(
				settings.general_voxel_volume_parameters,
				settings.swapping_mode == configuration::SWAPPINGMODE_ENABLED,
				memoryType, configuration::ForVolumeRole<TIndex>(configuration::VOLUME_LIVE));
	}
	this->warp_field = new VoxelVolume<TWarp, TIndex>(
			settings.general_voxel_volume_parameters,
			settings.swapping_mode == configuration::SWAPPINGMODE_ENABLED,
			memoryType, configuration::ForVolumeRole<TIndex>(configuration::VOLUME_WARP));
}

template<typename TVoxel, typename TWarp, typename TIndex>
DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::~DynamicSceneVoxelEngine() {

	delete rendering_engine;
	delete meshing_engine;
	delete depth_fusion_engine;
	delete swapping_engine;
	delete image_processing_engine;
	delete view_builder;

	delete camera_tracking_controller;

	delete surface_tracker;
	delete camera_tracker;
	delete imu_calibrator;

	for (int i_volume = 0; i_volume < DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::live_volume_count; i_volume++) {
		delete live_volumes[i_volume];
	}
	delete live_volumes;
	delete canonical_volume;

	delete canonical_render_state;
	delete live_render_state;
	if (freeview_render_state != nullptr) delete freeview_render_state;

	delete keyframe_raycast;
	delete relocalizer;

	delete view;
	delete tracking_state;
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::SaveVolumeToMesh(const std::string& path) {
	if (meshing_engine == nullptr) return;
	{
		Mesh mesh = meshing_engine->MeshVolume(canonical_volume);
		mesh.WritePLY(path);
	}
	if (target_warped_live_volume != nullptr) {
		Mesh mesh = meshing_engine->MeshVolume(target_warped_live_volume);
		fs::path p(path);
		mesh.WritePLY((p.parent_path() / "live_mesh.ply").string());
	}
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::SaveToFile(const std::string& path) {
	std::string relocalizer_output_path = path + "Relocaliser/";
	// throws error if any of the saves fail
	if (relocalizer) relocalizer->SaveToDirectory(relocalizer_output_path);
	std::cout << "Saving volumes & camera matrices in a compact way to '" << path << "'." << std::endl;
	canonical_volume->SaveToDisk(path + "/canonical_volume.dat");
	live_volumes[0]->SaveToDisk(path + "/live_volume.dat");
	ORUtils::OStreamWrapper camera_matrix_file(path + "/camera_matrix.dat");
	ORUtils::SaveMatrix(camera_matrix_file, tracking_state->pose_d->GetM());
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::SaveToFile() {
	fs::path path(std::string(config.paths.output_path) + "/ProcessedFrame_" + std::to_string(frames_processed));
	if (!fs::exists(path)) {
		fs::create_directories(path);
	}
	SaveToFile(path.string() + "/canonical_volume.dat");
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::LoadFromFile(const std::string& path) {
	std::string relocalizer_input_path = path + "Relocalizer/";
	if (view != nullptr) {
		try // load relocalizer
		{
			auto& settings = configuration::Get();
			FernRelocLib::Relocaliser<float>* relocalizer_temp =
					new FernRelocLib::Relocaliser<float>(view->depth.dimensions,
					                                     Vector2f(
							                                     settings.general_voxel_volume_parameters.near_clipping_distance,
							                                     settings.general_voxel_volume_parameters.far_clipping_distance),
					                                     0.2f, 500, 4);

			relocalizer_temp->LoadFromDirectory(relocalizer_input_path);

			delete relocalizer;
			relocalizer = relocalizer_temp;
		}
		catch (std::runtime_error& e) {
			throw std::runtime_error("Could not load relocalizer: " + std::string(e.what()));
		}
	}

	ORUtils::IStreamWrapper camera_matrix_file(path + "/camera_matrix.dat");
	tracking_state->pose_d->SetM(ORUtils::LoadMatrix<Matrix4f>(camera_matrix_file));

	try // load scene
	{
		std::cout << "Loading canonical volume from '" << path << "'." << std::endl;
		canonical_volume->LoadFromDisk(path + "/canonical_volume.dat");

		if (frames_processed == 0) {
			frames_processed = 1; //to skip initialization
		}
	}
	catch (std::runtime_error& e) {
		canonical_volume->Reset();
		throw std::runtime_error("Could not load volume:" + std::string(e.what()));
	}
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::LoadFromFile() {
	fs::path path(std::string(config.paths.output_path) + "/ProcessedFrame_" + std::to_string(frames_processed));
	LoadFromFile(path.string());
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::ResetAll() {
	Reset();
}

template<typename TVoxel, typename TWarp, typename TIndex>
int DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::FrameIndex() {
	return automatic_run_settings.index_of_frame_to_start_at + frames_processed;
}

template<typename TVoxel, typename TWarp, typename TIndex>
CameraTrackingState::TrackingResult
DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::ProcessFrame(UChar4Image* rgb_image,
                                                             ShortImage* depth_image,
                                                             IMUMeasurement* imu_measurement) {
	// prepare images & IMU measurements and turn them into a "view"
	if (imu_measurement == nullptr) {
		view_builder->UpdateView(&view, rgb_image, depth_image, config.use_threshold_filter,
		                         config.use_bilateral_filter, false, true);
	} else {
		view_builder->UpdateView(&view, rgb_image, depth_image, config.use_threshold_filter,
		                         config.use_bilateral_filter, imu_measurement, false, true);
	}

	if (!main_processing_active) {
		return CameraTrackingState::TRACKING_FAILED;
	}
	// camera tracking
	previous_frame_pose = (*(tracking_state->pose_d));
	if (camera_tracking_enabled) camera_tracking_controller->Track(tracking_state, view);

	HandlePotentialCameraTrackingFailure();

	bool fusion_succeeded = false;
	telemetry::SetGlobalFrameIndex(FrameIndex());
	if ((last_tracking_result == CameraTrackingState::TRACKING_GOOD || !tracking_initialised) &&
	    (fusion_active) && (relocalization_count == 0)) {

		camera_tracking_controller->Prepare(tracking_state, canonical_volume, view, rendering_engine, canonical_render_state);
		LOG4CPLUS_PER_FRAME(logging::GetLogger(), bright_cyan << "*** Generating raw live TSDF from view... ***" << reset);
		benchmarking::start_timer("GenerateRawLiveVolume");
		live_volumes[0]->Reset();
		live_volumes[1]->Reset();
		warp_field->Reset();

		indexing_engine->AllocateNearAndBetweenTwoSurfaces(live_volumes[0], view, tracking_state);
		AllocateUsingOtherVolume(live_volumes[1], live_volumes[0], this->config.device_type);
		AllocateUsingOtherVolume(canonical_volume, live_volumes[0], this->config.device_type);
		AllocateUsingOtherVolume(warp_field, live_volumes[0], this->config.device_type);
		depth_fusion_engine->IntegrateDepthImageIntoTsdfVolume(live_volumes[0], view, tracking_state);
		benchmarking::stop_timer("GenerateRawLiveVolume");

		//pre-tracking recording
		LogTSDFVolumeStatistics(live_volumes[0], "[[live TSDF before tracking]]", this->parameters.indexing_method);
		telemetry_recorder.RecordPreSurfaceTrackingData(*live_volumes[0], tracking_state->pose_d->GetM(), FrameIndex());

		benchmarking::start_timer("TrackMotion");
		LOG4CPLUS_PER_FRAME(logging::GetLogger(), bright_cyan << "*** Optimizing warp based on difference between canonical and live SDF. ***" << reset);
		bool optimizationConverged;
		target_warped_live_volume = surface_tracker->Align(warp_field, live_volumes, canonical_volume, optimizationConverged);
		if(this->parameters.halt_on_non_rigid_alignment_convergence_failure && !optimizationConverged){
			main_processing_active = false;
			LOG4CPLUS_TOP_LEVEL(logging::GetLogger(), bright_cyan << "Non-rigid TSDF alignment optimization did not converge. Switching off main processing." << reset);
		}

		LOG4CPLUS_PER_FRAME(logging::GetLogger(), bright_cyan << "*** Warping optimization finished for current frame. ***" << reset);
		benchmarking::stop_timer("TrackMotion");

		//post-tracking recording
		LogTSDFVolumeStatistics(target_warped_live_volume, "[[live TSDF after tracking]]", this->parameters.indexing_method);
		telemetry_recorder.RecordPostSurfaceTrackingData(*target_warped_live_volume, FrameIndex());

		//fuse warped live into canonical
		benchmarking::start_timer("FuseOneTsdfVolumeIntoAnother");
		volume_fusion_engine->FuseOneTsdfVolumeIntoAnother(canonical_volume, target_warped_live_volume, frames_processed);
		benchmarking::stop_timer("FuseOneTsdfVolumeIntoAnother");
		LogTSDFVolumeStatistics(canonical_volume, "[[canonical TSDF after fusion]]", this->parameters.indexing_method);

		//post-fusion recording
		telemetry_recorder.RecordPostFusionData(*canonical_volume, FrameIndex());
		LogVoxelHashBlockUsage(canonical_volume, target_warped_live_volume, warp_field, this->parameters.indexing_method);

		fusion_succeeded = true;
		if (frames_processed > 50) tracking_initialised = true;
		frames_processed++;
	}

	//preparation for next-frame tracking
	if (last_tracking_result == CameraTrackingState::TRACKING_GOOD ||
	    last_tracking_result == CameraTrackingState::TRACKING_POOR) {
		//TODO: FIXME (See issue #214 at https://github.com/Algomorph/InfiniTAM/issues/214)
		//if (!fusion_succeeded) indexing_engine->UpdateVisibleList(view, tracking_state, canonical_volume, canonical_render_state);

		// raycast to renderState_canonical for tracking and "freeview" rendering
		camera_tracking_controller->Prepare(tracking_state, canonical_volume, view, rendering_engine,
		                                    canonical_render_state);
	} else *tracking_state->pose_d = previous_frame_pose;


	LogCameraTrajectoryQuaternion(tracking_state->pose_d);


	return last_tracking_result;
}

template<typename TVoxel, typename TWarp, typename TIndex>
Vector2i DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::GetImageSize() const {
	return canonical_render_state->raycastImage->dimensions;
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::GetImage(UChar4Image* out, GetImageType type,
                                                              ORUtils::SE3Pose* pose,
                                                              Intrinsics* intrinsics) {


	auto& settings = configuration::Get();
	if (view == nullptr) return;

	out->Clear();

	bool might_need_frame_label_index = true;

	MemoryCopyDirection memory_copy_direction;
	if (settings.device_type == MEMORYDEVICE_CUDA) {
		memory_copy_direction = MemoryCopyDirection::CUDA_TO_CPU;
	} else {
		memory_copy_direction = MemoryCopyDirection::CPU_TO_CPU;
	}

	switch (type) {
		case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_ORIGINAL_RGB:
			out->SetFrom(view->rgb, memory_copy_direction);
			might_need_frame_label_index = false;
			break;
		case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_ORIGINAL_DEPTH:
			out->ChangeDims(view->depth.dimensions);
			if (settings.device_type == MEMORYDEVICE_CUDA) view->depth.UpdateHostFromDevice();
			RenderingEngineBase<TVoxel, TIndex>::DepthToUchar4(out, view->depth);
			might_need_frame_label_index = false;
			break;
		case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_SCENERAYCAST:
		case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_COLOUR_FROM_VOLUME:
		case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_COLOUR_FROM_NORMAL:
		case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_COLOUR_FROM_CONFIDENCE: {
			// use current raycast or forward projection?
			IRenderingEngine::RenderRaycastSelection raycastType;
			if (tracking_state->point_cloud_age <= 0) raycastType = IRenderingEngine::RENDER_FROM_OLD_RAYCAST;
			else raycastType = IRenderingEngine::RENDER_FROM_OLD_FORWARDPROJ;

			// what sort of image is it?
			IRenderingEngine::RenderImageType render_type;
			switch (type) {
				case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_COLOUR_FROM_CONFIDENCE:
					render_type = IRenderingEngine::RENDER_COLOUR_FROM_CONFIDENCE;
					break;
				case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_COLOUR_FROM_NORMAL:
					render_type = IRenderingEngine::RENDER_COLOUR_FROM_NORMAL;
					break;
				case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_COLOUR_FROM_VOLUME:
					render_type = IRenderingEngine::RENDER_COLOUR_FROM_VOLUME;
					break;
				default:
					render_type = IRenderingEngine::RENDER_SHADED_GREYSCALE_IMAGENORMALS;
			}

			rendering_engine->RenderImage(live_volumes[0], tracking_state->pose_d, &view->calibration_information.intrinsics_d,
			                              live_render_state, live_render_state->raycastImage, render_type,
			                              raycastType);


			ORUtils::Image<Vector4u>* srcImage = nullptr;
			if (relocalization_count != 0) srcImage = keyframe_raycast;
			else srcImage = canonical_render_state->raycastImage;
			out->SetFrom(*srcImage, memory_copy_direction);
			break;
		}
		case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_FREECAMERA_SHADED:
		case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_FREECAMERA_COLOUR_FROM_VOLUME:
		case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_FREECAMERA_COLOUR_FROM_NORMAL:
		case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_FREECAMERA_COLOUR_FROM_CONFIDENCE: {
			IRenderingEngine::RenderImageType render_type = IRenderingEngine::RENDER_SHADED_GREYSCALE;
			switch (type) {
				case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_FREECAMERA_COLOUR_FROM_VOLUME:
					render_type = IRenderingEngine::RENDER_COLOUR_FROM_VOLUME;
					break;
				case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_FREECAMERA_COLOUR_FROM_NORMAL:
					render_type = IRenderingEngine::RENDER_COLOUR_FROM_NORMAL;
					break;
				case DynamicSceneVoxelEngine::InfiniTAM_IMAGE_FREECAMERA_COLOUR_FROM_CONFIDENCE:
					render_type = IRenderingEngine::RENDER_COLOUR_FROM_CONFIDENCE;
					break;
				default:
					assert(false);
			}

			if (freeview_render_state == nullptr) {
				freeview_render_state = new RenderState(out->dimensions,
				                                        live_volumes[0]->GetParameters().near_clipping_distance,
				                                        live_volumes[0]->GetParameters().far_clipping_distance,
				                                        settings.device_type);
			}

			rendering_engine->FindVisibleBlocks(live_volumes[0], pose, intrinsics, freeview_render_state);
			rendering_engine->CreateExpectedDepths(live_volumes[0], pose, intrinsics, freeview_render_state);
			rendering_engine->RenderImage(live_volumes[0], pose, intrinsics, freeview_render_state,
			                              freeview_render_state->raycastImage, render_type);
			out->SetFrom(*freeview_render_state->raycastImage, memory_copy_direction);
			break;
		}
		case FusionAlgorithm::InfiniTAM_IMAGE_FREECAMERA_CANONICAL: {
			IRenderingEngine::RenderImageType render_image_type = IRenderingEngine::RENDER_SHADED_GREYSCALE;

			if (freeview_render_state == nullptr) {
				freeview_render_state = new RenderState(out->dimensions,
				                                        canonical_volume->GetParameters().near_clipping_distance,
				                                        canonical_volume->GetParameters().far_clipping_distance,
				                                        settings.device_type);
			}

			rendering_engine->FindVisibleBlocks(canonical_volume, pose, intrinsics, freeview_render_state);
			rendering_engine->CreateExpectedDepths(canonical_volume, pose, intrinsics, freeview_render_state);
			rendering_engine->RenderImage(canonical_volume, pose, intrinsics, freeview_render_state,
			                              freeview_render_state->raycastImage, render_image_type);

			out->SetFrom(*freeview_render_state->raycastImage, memory_copy_direction);
			break;
		}

		case FusionAlgorithm::InfiniTAM_IMAGE_UNKNOWN:
			break;
	};

	if (parameters.draw_frame_index_labels && might_need_frame_label_index) {
		AddFrameIndexToImage(*out);
	}
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::TurnOnTracking() { camera_tracking_enabled = true; }

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::TurnOffTracking() { camera_tracking_enabled = false; }

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::TurnOnIntegration() { fusion_active = true; }

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::TurnOffIntegration() { fusion_active = false; }

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::TurnOnMainProcessing() { main_processing_active = true; }

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::TurnOffMainProcessing() { main_processing_active = false; }

// region ==================================== STEP-BY-STEP MODE =======================================================


template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::HandlePotentialCameraTrackingFailure() {

	auto& settings = configuration::Get();
	last_tracking_result = CameraTrackingState::TRACKING_GOOD;
	switch (settings.behavior_on_failure) {
		case configuration::FAILUREMODE_RELOCALIZE:
			//relocalisation
			last_tracking_result = tracking_state->trackerResult;
			if (last_tracking_result == CameraTrackingState::TRACKING_GOOD && relocalization_count > 0)
				relocalization_count--;

			view->depth.UpdateHostFromDevice();

			{
				int NN;
				float distances;
				//find and add keyframe, if necessary
				bool hasAddedKeyframe = relocalizer->ProcessFrame(view->depth, tracking_state->pose_d, 0, 1, &NN,
				                                                  &distances,
				                                                  last_tracking_result ==
				                                                  CameraTrackingState::TRACKING_GOOD &&
				                                                  relocalization_count == 0);

				//frame not added and tracking failed -> we need to relocalise
				if (!hasAddedKeyframe && last_tracking_result == CameraTrackingState::TRACKING_FAILED) {
					relocalization_count = 10;

					// Reset previous rgb frame since the rgb image is likely different than the one acquired when setting the keyframe
					view->rgb_prev->Clear();

					const FernRelocLib::PoseDatabase::PoseInScene& keyframe = relocalizer->RetrievePose(NN);
					tracking_state->pose_d->SetFrom(&keyframe.pose);

					//TODO: FIXME (See issue #214 at https://github.com/Algomorph/InfiniTAM/issues/214)
					//indexing_engine->UpdateVisibleList(view, tracking_state, live_volumes[0], canonical_render_state, true);

					camera_tracking_controller->Prepare(tracking_state, live_volumes[0], view, rendering_engine,
					                                    canonical_render_state);
					camera_tracking_controller->Track(tracking_state, view);

					last_tracking_result = tracking_state->trackerResult;
				}
			}
			break;
		case configuration::FAILUREMODE_STOP_INTEGRATION:
			if (tracking_state->trackerResult != CameraTrackingState::TRACKING_FAILED)
				last_tracking_result = tracking_state->trackerResult;
			else last_tracking_result = CameraTrackingState::TRACKING_POOR;
			break;
		default:
			break;
	}


}

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::Reset() {
	canonical_volume->Reset();
	for (int iScene = 0; iScene < DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::live_volume_count; iScene++) {
		live_volumes[iScene]->Reset();
	}
	warp_field->Reset();
	tracking_state->Reset();
}


template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::ProcessSwapping(RenderState* render_state) {

	// swapping: CUDA -> CPU
	switch (config.swapping_mode) {
		case configuration::SWAPPINGMODE_ENABLED:
			swapping_engine->IntegrateGlobalIntoLocal(canonical_volume, render_state);
			swapping_engine->SaveToGlobalMemory(canonical_volume, render_state);
			break;
		case configuration::SWAPPINGMODE_DELETE:
			swapping_engine->CleanLocalMemory(canonical_volume, render_state);
			break;
		case configuration::SWAPPINGMODE_DISABLED:
			break;
	}
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::AddFrameIndexToImage(UChar4Image& out) {
	int frame_index = FrameIndex();
	std::stringstream ss;
	ss << std::setw(6) << std::setfill(' ') << frame_index;
	ORUtils::DrawTextOnImage(out, ss.str(), 15, 20, 20, true);
}

template<typename TVoxel, typename TWarp, typename TIndex>
bool DynamicSceneVoxelEngine<TVoxel, TWarp, TIndex>::GetMainProcessingOn() const {
	return this->main_processing_active;
}

// endregion ===========================================================================================================