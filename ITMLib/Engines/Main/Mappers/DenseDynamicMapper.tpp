//  ================================================================
//  Created by Gregory Kramida on 10/18/17.
//  Copyright (c) 2017-2025 Gregory Kramida
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
//#include <limits>
#include <chrono>

//boost
#include <boost/filesystem.hpp>

//local
#include "DenseDynamicMapper.h"
#include "../../DepthFusion/DepthFusionEngineFactory.h"
#include "../../Warping/WarpingEngineFactory.h"
#include "../../Swapping/SwappingEngineFactory.h"
#include "../../../SurfaceTrackers/SurfaceTrackerFactory.h"
#include "../../../Utils/Analytics/BenchmarkUtils.h"
#include "../../EditAndCopy/Interface/EditAndCopyEngineInterface.h"
#include "../../../Utils/Analytics/SceneStatisticsCalculator/Interface/SceneStatisticsCalculatorInterface.h"

//** CPU **
#include "../../EditAndCopy/CPU/EditAndCopyEngine_CPU.h"
#include "../../../Utils/Analytics/SceneStatisticsCalculator/CPU/SceneStatisticsCalculator_CPU.h"
//** CUDA **
#ifndef COMPILE_WITHOUT_CUDA
#include "../../EditAndCopy/CUDA/EditAndCopyEngine_CUDA.h"
#include "../../../Utils/Analytics/SceneStatisticsCalculator/CUDA/SceneStatisticsCalculator_CUDA.h"
#endif

using namespace ITMLib;

namespace bench = ITMLib::Bench;

// region ========================================= DEBUG PRINTING =====================================================

template<typename TVoxel, typename TWarp, typename TIndex>
void DenseDynamicMapper<TVoxel, TWarp, TIndex>::LogVolumeStatistics(VoxelVolume<TVoxel, TIndex>* volume,
                                                                    std::string volume_description) {
	if (this->log_volume_statistics) {
		SceneStatisticsCalculatorInterface<TVoxel, TIndex>* calculator = nullptr;
		switch (volume->index.memoryType) {
			case MEMORYDEVICE_CPU:
				calculator = &ITMSceneStatisticsCalculator<TVoxel, TIndex, MEMORYDEVICE_CPU>::Instance();
				break;
			case MEMORYDEVICE_CUDA:
#ifndef COMPILE_WITHOUT_CUDA
				calculator = &ITMSceneStatisticsCalculator<TVoxel, TIndex, MEMORYDEVICE_CUDA>::Instance();
#else
				DIEWITHEXCEPTION_REPORTLOCATION("Built without CUDA support, aborting.");
#endif
				break;
			case MEMORYDEVICE_METAL:
				DIEWITHEXCEPTION_REPORTLOCATION("Metal framework not supported for this.");
		}

		std::cout << green << "=== Stats for volume '" << volume_description << "' ===" << reset << std::endl;
		std::cout << "    Total voxel count: " << calculator->ComputeAllocatedVoxelCount(volume) << std::endl;
		std::cout << "    NonTruncated voxel count: " << calculator->ComputeNonTruncatedVoxelCount(volume) << std::endl;
		std::cout << "    +1.0 voxel count: " << calculator->CountVoxelsWithSpecificSdfValue(volume, 1.0f) << std::endl;
		//std::vector<int> allocatedHashes = calculator->GetAllocatedHashCodes(volume);
		std::cout << "    Allocated hash count: " << calculator->ComputeAllocatedHashBlockCount(volume) << std::endl;
		std::cout << "    NonTruncated SDF sum: " << calculator->ComputeNonTruncatedVoxelAbsSdfSum(volume) << std::endl;
		std::cout << "    Truncated SDF sum: " << calculator->ComputeTruncatedVoxelAbsSdfSum(volume) << std::endl;
	}
};

inline static void PrintOperationStatus(const char* status) {
	std::cout << bright_cyan << status << reset << std::endl;
}
// endregion ===========================================================================================================

// region ===================================== CONSTRUCTORS / DESTRUCTORS =============================================

template<typename TVoxel, typename TWarp, typename TIndex>
DenseDynamicMapper<TVoxel, TWarp, TIndex>::DenseDynamicMapper(const TIndex& index) :
		depth_fusion_engine(
				DepthFusionEngineFactory::Build<TVoxel, TWarp, TIndex>
						(configuration::get().device_type)),
		warping_engine(WarpingEngineFactory::MakeWarpingEngine<TVoxel, TWarp, TIndex>()),
		surface_tracker(
				SurfaceTrackerFactory::MakeSceneMotionTracker<TVoxel, TWarp, TIndex>()),
		swapping_engine(configuration::get().swapping_mode != configuration::SWAPPINGMODE_DISABLED
		               ? SwappingEngineFactory::Build<TVoxel, TIndex>(
						configuration::get().device_type, index)
		               : nullptr),
		swapping_mode(configuration::get().swapping_mode),
		parameters(configuration::get().non_rigid_tracking_parameters),
		use_expanded_allocation_during_TSDF_construction(
				configuration::get().general_voxel_volume_parameters.add_extra_block_ring_during_allocation),
		max_vector_update_threshold_in_voxels(parameters.max_update_length_threshold /
		                                      configuration::get().general_voxel_volume_parameters.voxel_size),
		has_focus_coordinates(configuration::get().verbosity_level >= configuration::VERBOSITY_FOCUS_SPOTS),
		focus_coordinates(configuration::get().telemetry_settings.focus_coordinates),
		verbosity_level(configuration::get().verbosity_level) {
	LogSettings();
}

template<typename TVoxel, typename TWarp, typename TIndex>
DenseDynamicMapper<TVoxel, TWarp, TIndex>::~DenseDynamicMapper() {
	delete depth_fusion_engine;
	delete warping_engine;
	delete swapping_engine;
	delete surface_tracker;
}
//endregion ================================= C/D ======================================================================

template<typename TVoxel, typename TWarp, typename TIndex>
void DenseDynamicMapper<TVoxel, TWarp, TIndex>::ProcessInitialFrame(
		const ITMView* view, const CameraTrackingState* trackingState,
		VoxelVolume<TVoxel, TIndex>* canonical_volume, VoxelVolume<TVoxel, TIndex>* live_volume,
		RenderState* canonical_render_state) {
	PrintOperationStatus("Generating raw live frame from view...");
	bench::StartTimer("GenerateRawLiveAndCanonicalVolumes");
	depth_fusion_engine->GenerateTsdfVolumeFromView(live_volume, view, trackingState);
	bench::StopTimer("GenerateRawLiveAndCanonicalVolumes");
	//** prepare canonical for new frame
	PrintOperationStatus("Fusing data from live frame into canonical frame...");
	//** fuse the live into canonical directly
	bench::StartTimer("FuseOneTsdfVolumeIntoAnother");
	volume_fusion_engine->FuseOneTsdfVolumeIntoAnother(canonical_volume, live_volume);
	bench::StopTimer("FuseOneTsdfVolumeIntoAnother");
	ProcessSwapping(canonical_volume, canonical_render_state);
}

template<typename TVoxel, typename TWarp, typename TIndex>
void
DenseDynamicMapper<TVoxel, TWarp, TIndex>::ProcessFrame(const ITMView* view, const CameraTrackingState* trackingState,
                                                        VoxelVolume<TVoxel, TIndex>* canonical_volume,
                                                        VoxelVolume<TVoxel, TIndex>** live_volume_pair,
                                                        VoxelVolume<TWarp, TIndex>* warp_field,
                                                        RenderState* canonical_render_state) {


	PrintOperationStatus("Generating raw live TSDF from view...");
	bench::StartTimer("GenerateRawLiveAndCanonicalVolumes");
	if (this->use_expanded_allocation_during_TSDF_construction) {
		depth_fusion_engine->GenerateTsdfVolumeFromViewExpanded(live_volume_pair[0], live_volume_pair[1], view,
		                                                        trackingState->pose_d->GetM());
	} else {
		depth_fusion_engine->GenerateTsdfVolumeFromView(live_volume_pair[0], view, trackingState->pose_d->GetM());
	}
	LogVolumeStatistics(live_volume_pair[0], "[[live TSDF before tracking]]");
	bench::StopTimer("GenerateRawLiveAndCanonicalVolumes");
	surface_tracker->ClearOutFramewiseWarp(warp_field);
	DynamicFusionLogger<TVoxel, TWarp, TIndex>::Instance().InitializeFrameRecording();



	bench::StartTimer("TrackMotion");
	VoxelVolume<TVoxel, TIndex>* target_warped_live_volume = TrackFrameMotion(canonical_volume, live_volume_pair, warp_field);
	bench::StopTimer("TrackMotion");
	LogVolumeStatistics(target_warped_live_volume, "[[live TSDF after tracking]]");


	//fuse warped live into canonical
	bench::StartTimer("FuseOneTsdfVolumeIntoAnother");
	volume_fusion_engine->FuseOneTsdfVolumeIntoAnother(canonical_volume, target_warped_live_volume);
	bench::StopTimer("FuseOneTsdfVolumeIntoAnother");
	LogVolumeStatistics(canonical_volume, "[[canonical TSDF after fusion]]");

	DynamicFusionLogger<TVoxel, TWarp, TIndex>::Instance().FinalizeFrameRecording();

	//TODO: revise how swapping works for dynamic scenes
	ProcessSwapping(canonical_volume, canonical_render_state);
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DenseDynamicMapper<TVoxel, TWarp, TIndex>::UpdateVisibleList(
		const ITMView* view,
		const CameraTrackingState* trackingState,
		VoxelVolume<TVoxel, TIndex>* scene, RenderState* renderState,
		bool resetVisibleList) {
	depth_fusion_engine->UpdateVisibleList(scene, view, trackingState, renderState, resetVisibleList);
}

// region =========================================== MOTION TRACKING ==================================================
/**
 * \brief Tracks motion of voxels from canonical frame to live frame.
 * \details The warp field representing motion of voxels in the canonical frame is updated such that the live frame maps
 * as closely as possible back to the canonical using the warp.
 * \param canonical_volume the canonical voxel grid
 * \param liveScene the live voxel grid (typcially obtained by integrating a single depth image into an empty TSDF grid)
 */
template<typename TVoxel, typename TWarp, typename TIndex>
VoxelVolume<TVoxel, TIndex>* DenseDynamicMapper<TVoxel, TWarp, TIndex>::TrackFrameMotion(
		VoxelVolume<TVoxel, TIndex>* canonical_volume,
		VoxelVolume<TVoxel, TIndex>** live_volume_pair,
		VoxelVolume<TWarp, TIndex>* warp_field) {

	float max_vector_update_length_in_voxels = std::numeric_limits<float>::infinity();
	PrintOperationStatus("*** Optimizing warp based on difference between canonical and live SDF. ***");
	bench::StartTimer("TrackMotion_3_Optimization");
	int source_live_volume_index = 0;
	int target_live_volume_index = 0;
	for (int iteration = 0; max_vector_update_length_in_voxels > this->max_vector_update_threshold_in_voxels
	                        && iteration < parameters.max_iteration_threshold; iteration++) {
		source_live_volume_index = iteration % 2;
		target_live_volume_index = (iteration + 1) % 2;
		PerformSingleOptimizationStep(canonical_volume, live_volume_pair[source_live_volume_index],
		                              live_volume_pair[target_live_volume_index], warp_field,
		                              max_vector_update_length_in_voxels, iteration);
	}
	bench::StopTimer("TrackMotion_3_Optimization");
	PrintOperationStatus("*** Warping optimization finished for current frame. ***");
	return live_volume_pair[target_live_volume_index];
}

template<typename TVoxel, typename TWarp, typename TIndex>
void DenseDynamicMapper<TVoxel, TWarp, TIndex>::PerformSingleOptimizationStep(
		VoxelVolume<TVoxel, TIndex>* canonical_volume,
		VoxelVolume<TVoxel, TIndex>* source_live_volume,
		VoxelVolume<TVoxel, TIndex>* target_live_volume,
		VoxelVolume<TWarp, TIndex>* warp_field,
		float& max_update_vector_length,
		int iteration) {

	DynamicFusionLogger<TVoxel, TWarp, TIndex>::Instance().SaveWarpSlices(iteration);

	if (configuration::get().verbosity_level >= configuration::VERBOSITY_PER_ITERATION) {
		std::cout << red << "Iteration: " << iteration << reset << std::endl;
		//** warp update gradient computation
		PrintOperationStatus("Calculating warp energy gradient...");
	}

	bench::StartTimer("TrackMotion_31_CalculateWarpUpdate");
	surface_tracker->CalculateWarpGradient(warp_field, canonical_volume, source_live_volume);
	bench::StopTimer("TrackMotion_31_CalculateWarpUpdate");

	if (configuration::get().verbosity_level >= configuration::VERBOSITY_PER_ITERATION) {
		PrintOperationStatus("Applying Sobolev smoothing to energy gradient...");
	}

	bench::StartTimer("TrackMotion_32_ApplySmoothingToGradient");
	surface_tracker->SmoothWarpGradient(warp_field, canonical_volume, source_live_volume);
	bench::StopTimer("TrackMotion_32_ApplySmoothingToGradient");

	if (verbosity_level >= configuration::VERBOSITY_PER_ITERATION) {
		PrintOperationStatus("Applying warp update (based on energy gradient) to the cumulative warp...");
	}
	bench::StartTimer("TrackMotion_33_UpdateWarps");
	max_update_vector_length = surface_tracker->UpdateWarps(warp_field, canonical_volume, source_live_volume);
	bench::StopTimer("TrackMotion_33_UpdateWarps");

	if (verbosity_level >= configuration::VERBOSITY_PER_ITERATION) {
		PrintOperationStatus(
				"Updating live frame SDF by mapping from raw live SDF to new warped SDF based on latest warp...");
	}
	bench::StartTimer("TrackMotion_35_WarpLiveScene");
	warping_engine->WarpVolume_WarpUpdates(warp_field, source_live_volume, target_live_volume);
	bench::StopTimer("TrackMotion_35_WarpLiveScene");
	DynamicFusionLogger<TVoxel, TWarp, TIndex>::Instance().SaveWarps();
}


//endregion ============================================================================================================

template<typename TVoxel, typename TWarp, typename TIndex>
void DenseDynamicMapper<TVoxel, TWarp, TIndex>::ProcessSwapping(
		VoxelVolume<TVoxel, TIndex>* canonicalScene, RenderState* renderState) {
	if (swapping_engine != nullptr) {
		// swapping: CPU -> CUDA
		if (swapping_mode == configuration::SWAPPINGMODE_ENABLED)
			swapping_engine->IntegrateGlobalIntoLocal(canonicalScene, renderState);

		// swapping: CUDA -> CPU
		switch (swapping_mode) {
			case configuration::SWAPPINGMODE_ENABLED:
				swapping_engine->SaveToGlobalMemory(canonicalScene, renderState);
				break;
			case configuration::SWAPPINGMODE_DELETE:
				swapping_engine->CleanLocalMemory(canonicalScene, renderState);
				break;
			case configuration::SWAPPINGMODE_DISABLED:
				break;
		}
	}
}


template<typename TVoxel, typename TWarp, typename TIndex>
void DenseDynamicMapper<TVoxel, TWarp, TIndex>::LogSettings() {
	if (this->log_settings) {
		std::cout << bright_cyan << "*** DenseDynamicMapper Settings: ***" << reset << std::endl;
		std::cout << "Max iteration count: " << this->parameters.max_iteration_threshold << std::endl;
		std::cout << "Warping vector update threshold: " << this->parameters.max_update_length_threshold << " m, ";
		std::cout << this->max_vector_update_threshold_in_voxels << " voxels" << std::endl;
		std::cout << bright_cyan << "*** *********************************** ***" << reset << std::endl;
	}
}
