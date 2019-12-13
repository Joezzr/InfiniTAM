//  ================================================================
//  Created by Gregory Kramida on 11/7/19.
//  Copyright (c) 2019 Gregory Kramida
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

#define BOOST_TEST_MODULE WarpGradient_VBH_to_PVA
#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

//stdlib
#include <unordered_map>

//boost
#include <boost/test/unit_test.hpp>

//local
#include "../ITMLib/ITMLibDefines.h"
#include "../ITMLib/Objects/Scene/ITMVoxelVolume.h"
#include "../ITMLib/Utils/Analytics/VoxelVolumeComparison/ITMVoxelVolumeComparison.h"

#include "../ITMLib/Utils/Analytics/VoxelVolumeComparison/ITMVoxelVolumeComparison_CPU.h"
#include "../ITMLib/Utils/Analytics/SceneStatisticsCalculator/CPU/ITMSceneStatisticsCalculator_CPU.h"
#include "../ITMLib/SurfaceTrackers/Interface/SurfaceTracker.h"

#include "../ITMLib/Engines/Manipulation/ITMSceneManipulationEngineFactory.h"
#include "../ITMLib/Engines/Reconstruction/CPU/ITMDynamicSceneReconstructionEngine_CPU.h"

#include "../ITMLib/Engines/Reconstruction/CUDA/ITMDynamicSceneReconstructionEngine_CUDA.h"
#include "../ITMLib/Utils/Analytics/VoxelVolumeComparison/ITMVoxelVolumeComparison_CUDA.h"
#include "../ITMLib/Utils/Analytics/SceneStatisticsCalculator/CUDA/ITMSceneStatisticsCalculator_CUDA.h"

#include "../ITMLib/Engines/Reconstruction/ITMDynamicSceneReconstructionEngineFactory.h"


#include "TestUtils.h"
#include "TestUtilsForSnoopyFrames16And17.h"
#include "../ITMLib/Objects/RenderStates/ITMRenderStateFactory.h"

using namespace ITMLib;

typedef ITMDynamicSceneReconstructionEngine_CUDA<ITMVoxel, ITMWarp, ITMPlainVoxelArray> RecoEngine_CUDA_PVA;
typedef ITMDynamicSceneReconstructionEngine_CUDA<ITMVoxel, ITMWarp, ITMVoxelBlockHash> RecoEngine_CUDA_VBH;


//TODO: move the construction checks into Test_ConstructVolumeFromImage test files as appropriate 

enum GenericWarpTestMode {
	SAVE_SUCCESSIVE_ITERATIONS,
	SAVE_FINAL_ITERATION_AND_FUSION,
	TEST_SUCCESSIVE_ITERATIONS,
	TEST_FINAL_ITERATION_AND_FUSION
};


std::string get_path_warps(std::string prefix, int iteration) {
	return "TestData/snoopy_result_fr16-17_warps/" + prefix + "_iter_" + std::to_string(iteration) + "_";
}

std::string get_path_warped_live(std::string prefix, int iteration) {
	return get_path_warps(prefix, iteration) + "warped_live_";
}

std::string get_path_fused(std::string prefix, int iteration) {
	return get_path_warps(prefix, iteration) + "fused_";
}


unsigned int switches_to_int_code(const SlavchevaSurfaceTracker::Switches& switches) {
	unsigned int code = 0;
	code |= static_cast<unsigned int>(switches.enableDataTerm) << 0u;
	code |= static_cast<unsigned int>(switches.enableLevelSetTerm) << 1u;
	code |= static_cast<unsigned int>(switches.enableSmoothingTerm) << 2u;
	code |= static_cast<unsigned int>(switches.enableKillingRigidityEnforcementTerm) << 3u;
	code |= static_cast<unsigned int>(switches.enableSobolevGradientSmoothing) << 4u;
	return code;
}

std::string switches_to_prefix(const SlavchevaSurfaceTracker::Switches& switches) {
	static std::unordered_map<unsigned int, std::string> prefix_by_switches_map = {
			{switches_to_int_code(SlavchevaSurfaceTracker::Switches(true, false, false, false, false)),
					                                                         "data_only"},
			{switches_to_int_code(SlavchevaSurfaceTracker::Switches(false, false, true, false, false)),
					                                                         "tikhonov_only"},
			{switches_to_int_code(SlavchevaSurfaceTracker::Switches(true, false, true, false,
			                                                        false)), "data_tikhonov"},
			{switches_to_int_code(SlavchevaSurfaceTracker::Switches(true, false, true, false, true)),
					                                                         "data_tikhonov_sobolev"}
	};
	return prefix_by_switches_map[switches_to_int_code(switches)];
}

template<typename TIndex>
std::string getIndexString();

template<>
std::string getIndexString<ITMPlainVoxelArray>() { return "PVA"; }

template<>
std::string getIndexString<ITMVoxelBlockHash>() { return "VBH"; }

template<typename TIndex, MemoryDeviceType TMemoryDeviceType>
void
GenericWarpConsistencySubtest(const SlavchevaSurfaceTracker::Switches& switches,
                              int iteration_limit = 10,
                              GenericWarpTestMode mode = TEST_SUCCESSIVE_ITERATIONS,
                              float absolute_tolerance = 1e-7, bool allocateLiveFromBothImages = false,
                              bool expand_raw_live_allocation = true) {

	std::string prefix = switches_to_prefix(switches);
	if (iteration_limit < 2) {
		DIEWITHEXCEPTION_REPORTLOCATION("Iteration limit must be at least 2");
	}

	ITMVoxelVolume<ITMWarp, TIndex> warp_field(&Configuration::get().scene_parameters,
	                                           Configuration::get().swapping_mode ==
	                                           Configuration::SWAPPINGMODE_ENABLED,
	                                           TMemoryDeviceType,
	                                           Frame16And17Fixture::InitParams<TIndex>());

	ITMVoxelVolume<ITMVoxel, TIndex>* volume_16;
	ITMView* view = nullptr;
	buildSdfVolumeFromImage(&volume_16, &view,
	                        "TestData/snoopy_depth_000016.png",
	                        "TestData/snoopy_color_000016.png",
	                        "TestData/snoopy_omask_000016.png",
	                        "TestData/snoopy_calib.txt",
	                        TMemoryDeviceType,
	                        Frame16And17Fixture::InitParams<TIndex>());

	ITMVoxelVolume<ITMVoxel, TIndex>* warped_volumes[2] = {
			new ITMVoxelVolume<ITMVoxel, TIndex>(&Configuration::get().scene_parameters,
			                                     Configuration::get().swapping_mode ==
			                                     Configuration::SWAPPINGMODE_ENABLED,
			                                     TMemoryDeviceType,
			                                     Frame16And17Fixture::InitParams<TIndex>()),
			new ITMVoxelVolume<ITMVoxel, TIndex>(&Configuration::get().scene_parameters,
			                                     Configuration::get().swapping_mode ==
			                                     Configuration::SWAPPINGMODE_ENABLED,
			                                     TMemoryDeviceType,
			                                     Frame16And17Fixture::InitParams<TIndex>())
	};
	ITMSceneManipulationEngineFactory::Instance<ITMVoxel, TIndex, TMemoryDeviceType>().ResetScene(warped_volumes[0]);
	ITMSceneManipulationEngineFactory::Instance<ITMVoxel, TIndex, TMemoryDeviceType>().ResetScene(warped_volumes[1]);

	Vector2i imageSize(640, 480);
	ITMRenderState* renderState = ITMRenderStateFactory<TIndex>::CreateRenderState(imageSize,
	                                                                               &Configuration::get().scene_parameters,
	                                                                               TMemoryDeviceType,
	                                                                               warped_volumes[1]->index);
	ITMTrackingState trackingState(imageSize, TMemoryDeviceType);
	if (allocateLiveFromBothImages) {
		ITMIndexingEngine<ITMVoxel, TIndex, TMemoryDeviceType>::Instance().AllocateFromDepth(
				warped_volumes[1], view, &trackingState, renderState, false, false);
	}

	updateView("TestData/snoopy_depth_000017.png",
	           "TestData/snoopy_color_000017.png", "TestData/snoopy_omask_000017.png",
	           "TestData/snoopy_calib.txt", TMemoryDeviceType, &view);
	ITMIndexingEngine<ITMVoxel, TIndex, TMemoryDeviceType>::Instance().AllocateFromDepth(
			warped_volumes[1], view, &trackingState, renderState, false, false);

	int scene_to_integrate_raw_depth_index = 1;

	if (expand_raw_live_allocation) {
		scene_to_integrate_raw_depth_index = 0;
		ITMIndexingEngine<ITMVoxel, TIndex, TMemoryDeviceType>::Instance().AllocateUsingOtherVolumeExpanded(
				warped_volumes[0], warped_volumes[1]);
	}

	ITMDynamicSceneReconstructionEngine<ITMVoxel, ITMWarp, TIndex>* reconstructionEngine =
			ITMDynamicSceneReconstructionEngineFactory
			::MakeSceneReconstructionEngine<ITMVoxel, ITMWarp, TIndex>(TMemoryDeviceType);

	reconstructionEngine->IntegrateIntoScene(warped_volumes[scene_to_integrate_raw_depth_index], view, &trackingState,
	                                         renderState);

	ITMSceneManipulationEngineFactory::Instance<ITMWarp, TIndex, TMemoryDeviceType>().ResetScene(&warp_field);

	SurfaceTracker<ITMVoxel, ITMWarp, TIndex, TMemoryDeviceType, TRACKER_SLAVCHEVA_DIAGNOSTIC>
			motionTracker(switches);

	std::cout << "Subtest " << getIndexString<TIndex>() << " iteration 0" << std::endl;
	motionTracker.CalculateWarpGradient(volume_16, warped_volumes[scene_to_integrate_raw_depth_index], &warp_field);
	motionTracker.SmoothWarpGradient(volume_16, warped_volumes[scene_to_integrate_raw_depth_index], &warp_field);
	motionTracker.UpdateWarps(volume_16, warped_volumes[scene_to_integrate_raw_depth_index], &warp_field);

	ITMVoxelVolume<ITMWarp, TIndex> warp_field2(&Configuration::get().scene_parameters,
	                                            Configuration::get().swapping_mode ==
	                                            Configuration::SWAPPINGMODE_ENABLED,
	                                            TMemoryDeviceType,
	                                            Frame16And17Fixture::InitParams<TIndex>());
	ITMSceneManipulationEngineFactory::Instance<ITMWarp, TIndex, TMemoryDeviceType>().ResetScene(&warp_field2);
	switch (mode) {
		case SAVE_SUCCESSIVE_ITERATIONS:
			warp_field.SaveToDirectory(std::string("../../Tests/") + get_path_warps(prefix, 0));
			break;
		case TEST_SUCCESSIVE_ITERATIONS:
			warp_field2.LoadFromDirectory(get_path_warps(prefix, 0));
			BOOST_REQUIRE(contentAlmostEqual(&warp_field, &warp_field2, absolute_tolerance, TMemoryDeviceType));
			break;
		default:
			break;
	}

	ITMDynamicSceneReconstructionEngine<ITMVoxel, ITMWarp, TIndex>* recoEngine =
			ITMDynamicSceneReconstructionEngineFactory::MakeSceneReconstructionEngine<ITMVoxel, ITMWarp, TIndex>(
					TMemoryDeviceType);

	int source_warped_field_ix = scene_to_integrate_raw_depth_index;
	int target_warped_field_ix = (scene_to_integrate_raw_depth_index + 1) % 2;
	recoEngine->WarpScene_FlowWarps(&warp_field, warped_volumes[source_warped_field_ix],warped_volumes[target_warped_field_ix]);

	ITMVoxelVolume<ITMVoxel, TIndex>* ground_truth_warped_live = new ITMVoxelVolume<ITMVoxel, TIndex>(&Configuration::get().scene_parameters,
	                                                                                     Configuration::get().swapping_mode ==
	                                                                                     Configuration::SWAPPINGMODE_ENABLED,
	                                                                                                  TMemoryDeviceType,
	                                                                                                  Frame16And17Fixture::InitParams<TIndex>());

	switch (mode) {
		case SAVE_SUCCESSIVE_ITERATIONS:
			warped_volumes[1]->SaveToDirectory(std::string("../../Tests/") + get_path_warped_live(prefix, 0));
			break;
		case TEST_SUCCESSIVE_ITERATIONS:
			ITMSceneManipulationEngineFactory::Instance<ITMVoxel, TIndex, TMemoryDeviceType>().ResetScene(ground_truth_warped_live);
			ground_truth_warped_live->LoadFromDirectory(get_path_warped_live(prefix, 0));
			BOOST_REQUIRE(
					contentAlmostEqual_Verbose(ground_truth_warped_live, warped_volumes[target_warped_field_ix], absolute_tolerance, TMemoryDeviceType));
			break;
		default:
			break;
	}


	for (int iteration = 1; iteration < iteration_limit; iteration++) {
		std::swap(source_warped_field_ix, target_warped_field_ix);
		std::cout << "Subtest " << getIndexString<TIndex>() << " iteration " << std::to_string(iteration) << std::endl;
		motionTracker.CalculateWarpGradient(volume_16, warped_volumes[source_warped_field_ix], &warp_field);
		motionTracker.SmoothWarpGradient(volume_16, warped_volumes[source_warped_field_ix], &warp_field);
		motionTracker.UpdateWarps(volume_16, warped_volumes[source_warped_field_ix], &warp_field);
		recoEngine->WarpScene_FlowWarps(&warp_field, warped_volumes[source_warped_field_ix],
		                                warped_volumes[target_warped_field_ix]);
		std::string path = get_path_warps(prefix, iteration);
		std::string path_warped_live = get_path_warped_live(prefix, iteration);
		switch (mode) {
			case SAVE_SUCCESSIVE_ITERATIONS:
				warped_volumes[target_warped_field_ix]->SaveToDirectory(std::string("../../Tests/") + path_warped_live);
				warp_field.SaveToDirectory(std::string("../../Tests/") + path);
				break;
			case TEST_SUCCESSIVE_ITERATIONS:
				ITMSceneManipulationEngineFactory::Instance<ITMWarp, TIndex, TMemoryDeviceType>().ResetScene(
						&warp_field2);
				warp_field2.LoadFromDirectory(path);
				BOOST_REQUIRE(contentAlmostEqual(&warp_field, &warp_field2, absolute_tolerance, TMemoryDeviceType));
				ITMSceneManipulationEngineFactory::Instance<ITMVoxel, TIndex, TMemoryDeviceType>().ResetScene(
						ground_truth_warped_live);
				ground_truth_warped_live->LoadFromDirectory(path_warped_live);
				BOOST_REQUIRE(contentAlmostEqual(ground_truth_warped_live, warped_volumes[target_warped_field_ix],
				                                 absolute_tolerance, TMemoryDeviceType));
				break;
			default:
				break;
		}
	}
	switch (mode) {
		case SAVE_FINAL_ITERATION_AND_FUSION:
			warp_field.SaveToDirectory(std::string("../../Tests/") + get_path_warps(prefix, iteration_limit - 1));
			warped_volumes[target_warped_field_ix]->SaveToDirectory(
					std::string("../../Tests/") + get_path_warped_live(prefix, iteration_limit - 1));
			recoEngine->FuseLiveIntoCanonicalSdf(volume_16, warped_volumes[target_warped_field_ix]);
			volume_16->SaveToDirectory(std::string("../../Tests/") + get_path_fused(prefix, iteration_limit - 1));
			break;
		case TEST_FINAL_ITERATION_AND_FUSION:
			ITMSceneManipulationEngineFactory::Instance<ITMWarp, TIndex, TMemoryDeviceType>().ResetScene(&warp_field2);
			warp_field2.LoadFromDirectory(get_path_warps(prefix, iteration_limit - 1));
			BOOST_REQUIRE(contentAlmostEqual(&warp_field, &warp_field2, absolute_tolerance, TMemoryDeviceType));
			warped_volumes[source_warped_field_ix]->LoadFromDirectory(
					get_path_warped_live(prefix, iteration_limit - 1));
			BOOST_REQUIRE(contentAlmostEqual(warped_volumes[target_warped_field_ix],
			                                 warped_volumes[source_warped_field_ix], absolute_tolerance,
			                                 TMemoryDeviceType));
			recoEngine->FuseLiveIntoCanonicalSdf(volume_16, warped_volumes[target_warped_field_ix]);
			warped_volumes[source_warped_field_ix]->LoadFromDirectory(get_path_fused(prefix, iteration_limit - 1));
			BOOST_REQUIRE(
					contentAlmostEqual(volume_16, warped_volumes[source_warped_field_ix], absolute_tolerance,
					                   TMemoryDeviceType));
			break;
		default:
			break;
	}

	delete volume_16;
	delete ground_truth_warped_live;
	delete warped_volumes[0];
	delete warped_volumes[1];
	delete recoEngine;
}

///CAUTION: SAVE modes require the build directory to be immediately inside the root source directory.
template<MemoryDeviceType TMemoryDeviceType>
void
GenericWarpTest(const SlavchevaSurfaceTracker::Switches& switches, int iteration_limit = 10,
                GenericWarpTestMode mode = TEST_SUCCESSIVE_ITERATIONS, float absoluteTolerance = 1e-7) {

	std::string prefix = switches_to_prefix(switches);
	GenericWarpConsistencySubtest<ITMPlainVoxelArray, TMemoryDeviceType>(switches, iteration_limit, mode,
	                                                                     absoluteTolerance);
	GenericWarpConsistencySubtest<ITMVoxelBlockHash, TMemoryDeviceType>(switches, iteration_limit, mode,
	                                                                    absoluteTolerance);

	ITMVoxelVolume<ITMVoxel, ITMPlainVoxelArray> volume_PVA(&Configuration::get().scene_parameters,
	                                                        Configuration::get().swapping_mode ==
	                                                        Configuration::SWAPPINGMODE_ENABLED,
	                                                        TMemoryDeviceType,
	                                                        Frame16And17Fixture::InitParams<ITMPlainVoxelArray>());
	ITMVoxelVolume<ITMVoxel, ITMVoxelBlockHash> volume_VBH(&Configuration::get().scene_parameters,
	                                                       Configuration::get().swapping_mode ==
	                                                       Configuration::SWAPPINGMODE_ENABLED,
	                                                       TMemoryDeviceType,
	                                                       Frame16And17Fixture::InitParams<ITMVoxelBlockHash>());
	switch (mode) {
		case TEST_SUCCESSIVE_ITERATIONS: {

			ITMVoxelVolume<ITMWarp, ITMPlainVoxelArray> warp_field_PVA(&Configuration::get().scene_parameters,
			                                                           Configuration::get().swapping_mode ==
			                                                           Configuration::SWAPPINGMODE_ENABLED,
			                                                           TMemoryDeviceType,
			                                                           Frame16And17Fixture::InitParams<ITMPlainVoxelArray>());
			ITMVoxelVolume<ITMWarp, ITMVoxelBlockHash> warp_field_VBH(&Configuration::get().scene_parameters,
			                                                          Configuration::get().swapping_mode ==
			                                                          Configuration::SWAPPINGMODE_ENABLED,
			                                                          TMemoryDeviceType,
			                                                          Frame16And17Fixture::InitParams<ITMVoxelBlockHash>());

			for (int iteration = 0; iteration < iteration_limit; iteration++) {
				std::cout << "Testing iteration " << iteration << std::endl;
				warp_field_PVA.LoadFromDirectory(get_path_warps(prefix, iteration));
				ITMSceneManipulationEngineFactory::Instance<ITMWarp, ITMVoxelBlockHash, TMemoryDeviceType>().ResetScene(
						&warp_field_VBH);
				warp_field_VBH.LoadFromDirectory(get_path_warps(prefix, iteration));
				BOOST_REQUIRE(allocatedContentAlmostEqual_Verbose(&warp_field_PVA, &warp_field_VBH,
				                                                  absoluteTolerance, TMemoryDeviceType));
				ITMSceneManipulationEngineFactory::Instance<ITMVoxel, ITMVoxelBlockHash, TMemoryDeviceType>().ResetScene(
						&volume_VBH);
				volume_PVA.LoadFromDirectory(get_path_warped_live(prefix, iteration));
				volume_VBH.LoadFromDirectory(get_path_warped_live(prefix, iteration));
				BOOST_REQUIRE(contentForFlagsAlmostEqual_Verbose(&volume_PVA, &volume_VBH, VOXEL_NONTRUNCATED,
				                                                 absoluteTolerance, TMemoryDeviceType));
			}
		}
			break;
		case TEST_FINAL_ITERATION_AND_FUSION: {
			volume_PVA.LoadFromDirectory(get_path_warped_live(prefix, iteration_limit - 1));
			ITMSceneManipulationEngineFactory::Instance<ITMVoxel, ITMVoxelBlockHash, TMemoryDeviceType>().ResetScene(
					&volume_VBH);
			volume_VBH.LoadFromDirectory(get_path_warped_live(prefix, iteration_limit - 1));
			BOOST_REQUIRE(
					contentForFlagsAlmostEqual(&volume_PVA, &volume_VBH, VOXEL_NONTRUNCATED, absoluteTolerance,
					                           TMemoryDeviceType));
			volume_PVA.LoadFromDirectory(get_path_fused(prefix, iteration_limit - 1));
			ITMSceneManipulationEngineFactory::Instance<ITMVoxel, ITMVoxelBlockHash, TMemoryDeviceType>().ResetScene(
					&volume_VBH);
			volume_VBH.LoadFromDirectory(get_path_fused(prefix, iteration_limit - 1));
			BOOST_REQUIRE(
					contentForFlagsAlmostEqual(&volume_PVA, &volume_VBH, VOXEL_NONTRUNCATED, absoluteTolerance,
					                           TMemoryDeviceType));
		}
			break;
		default:
			break;
	}
}


BOOST_AUTO_TEST_CASE(Test_Warp_PVA_VBH_DataTermOnly_CPU) {
	SlavchevaSurfaceTracker::Switches switches(true, false, false, false, false);
	GenericWarpTest<MEMORYDEVICE_CPU>(switches, 10, SAVE_SUCCESSIVE_ITERATIONS);
}


BOOST_AUTO_TEST_CASE(Test_Warp_PVA_VBH_DataTermOnly_CUDA) {
	SlavchevaSurfaceTracker::Switches switches(true, false, false, false, false);
	GenericWarpTest<MEMORYDEVICE_CUDA>(switches, 10, TEST_SUCCESSIVE_ITERATIONS, 1e-5);
}

BOOST_AUTO_TEST_CASE(Test_Warp_PVA_VBH_DataAndTikhonov_CPU) {
	Vector3i test_pos(-57, -9, 196);
	Configuration::get().telemetry_settings.focus_coordinates_specified = true;
	Configuration::get().telemetry_settings.focus_coordinates = test_pos;
	SlavchevaSurfaceTracker::Switches switches(true, false, true, false, false);
	GenericWarpTest<MEMORYDEVICE_CPU>(switches, 5, TEST_SUCCESSIVE_ITERATIONS, 1e-7);
}

BOOST_AUTO_TEST_CASE(Test_Warp_PVA_VBH_DataAndTikhonov_CUDA) {
	SlavchevaSurfaceTracker::Switches switches(true, false, true, false, false);
	GenericWarpTest<MEMORYDEVICE_CUDA>(switches, 5, TEST_SUCCESSIVE_ITERATIONS, 1e-7);
}

BOOST_AUTO_TEST_CASE(Test_Warp_PVA_VBH_DataAndTikhonovAndSobolevSmoothing_CUDA) {
	SlavchevaSurfaceTracker::Switches switches(true, false, true, false, true);
	GenericWarpTest<MEMORYDEVICE_CUDA>(switches, 5, TEST_SUCCESSIVE_ITERATIONS, 1e-7);
}


BOOST_AUTO_TEST_CASE(Test_Warp_PVA_VBH_DataAndTikhonovAndSobolevSmoothing_Fusion_CUDA) {
	GenericWarpTest<MEMORYDEVICE_CUDA>(SlavchevaSurfaceTracker::Switches(true, false, true, false, true), 100,
	                                   GenericWarpTestMode::SAVE_FINAL_ITERATION_AND_FUSION);
}

template<MemoryDeviceType TMemoryDeviceType>
void Warp_PVA_VBH_simple_subtest(int iteration, SlavchevaSurfaceTracker::Switches trackerSwitches) {

	if (iteration < 0) {
		DIEWITHEXCEPTION_REPORTLOCATION("Expecting iteration >= 0, got less than that, aborting.");
	}
	std::string path_frame_17_PVA = "TestData/snoopy_result_fr16-17_partial_PVA/snoopy_partial_frame_17_";
	std::string path_frame_16_PVA = "TestData/snoopy_result_fr16-17_partial_PVA/snoopy_partial_frame_16_";
	std::string path_frame_17_VBH = "TestData/snoopy_result_fr16-17_partial_VBH/snoopy_partial_frame_17_";
	std::string path_frame_16_VBH = "TestData/snoopy_result_fr16-17_partial_VBH/snoopy_partial_frame_16_";

	std::string prefix = switches_to_prefix(trackerSwitches);
	float absoluteTolerance = 1e-7;

	// *** initialize/load warps
	ITMVoxelVolume<ITMWarp, ITMPlainVoxelArray>* warps_PVA;
	ITMVoxelVolume<ITMWarp, ITMVoxelBlockHash>* warps_VBH;
	if (iteration > 0) {
		std::string path_warps = get_path_warps(prefix, iteration - 1);
		loadVolume(&warps_PVA, path_warps, TMemoryDeviceType,
		           Frame16And17Fixture::InitParams<ITMPlainVoxelArray>());
		loadVolume(&warps_VBH, path_warps, TMemoryDeviceType,
		           Frame16And17Fixture::InitParams<ITMVoxelBlockHash>());
		BOOST_REQUIRE(allocatedContentAlmostEqual(warps_PVA, warps_VBH, absoluteTolerance, TMemoryDeviceType));
	} else {
		initializeVolume(&warps_PVA, Frame16And17Fixture::InitParams<ITMPlainVoxelArray>(), TMemoryDeviceType);
		initializeVolume(&warps_VBH, Frame16And17Fixture::InitParams<ITMVoxelBlockHash>(), TMemoryDeviceType);
		BOOST_REQUIRE(allocatedContentAlmostEqual(warps_PVA, warps_VBH, absoluteTolerance, TMemoryDeviceType));
	}

	// *** load warped live scene



	ITMVoxelVolume<ITMVoxel, ITMPlainVoxelArray>* warped_live_PVA;
	ITMVoxelVolume<ITMVoxel, ITMVoxelBlockHash>* warped_live_VBH;

	if (iteration > 0) {
		std::string path_warped_live = get_path_warped_live(prefix, iteration - 1);
		loadVolume(&warped_live_PVA, path_warped_live, TMemoryDeviceType,
		           Frame16And17Fixture::InitParams<ITMPlainVoxelArray>());
		loadVolume(&warped_live_VBH, path_warped_live, TMemoryDeviceType,
		           Frame16And17Fixture::InitParams<ITMVoxelBlockHash>());
	} else {
		loadVolume(&warped_live_PVA, path_frame_17_PVA, TMemoryDeviceType,
		           Frame16And17Fixture::InitParams<ITMPlainVoxelArray>());
		loadVolume(&warped_live_VBH, path_frame_17_VBH, TMemoryDeviceType,
		           Frame16And17Fixture::InitParams<ITMVoxelBlockHash>());
	}

	//_DEBUG
	Vector3i test_pos(-57, -9, 195);
	Configuration::get().telemetry_settings.focus_coordinates_specified = true;
	Configuration::get().telemetry_settings.focus_coordinates = test_pos;
//	ITMVoxel voxelPVA = ITMSceneManipulationEngine<ITMVoxel, ITMPlainVoxelArray>::Inst()
//			.ReadVoxel(warped_live_PVA,test_pos);
//	voxelPVA.print_self();
//	ITMVoxel voxelVBH = ITMSceneManipulationEngine<ITMVoxel, ITMVoxelBlockHash>::Inst()
//			.ReadVoxel(warped_live_VBH, test_pos);
//	voxelVBH.print_self();

	// *** load canonical volume as the two different data structures
	ITMVoxelVolume<ITMVoxel, ITMPlainVoxelArray>* volume_16_PVA;
	loadVolume(&volume_16_PVA, path_frame_16_PVA, TMemoryDeviceType,
	           Frame16And17Fixture::InitParams<ITMPlainVoxelArray>());
	ITMVoxelVolume<ITMVoxel, ITMVoxelBlockHash>* volume_16_VBH;
	loadVolume(&volume_16_VBH, path_frame_16_VBH, TMemoryDeviceType,
	           Frame16And17Fixture::InitParams<ITMVoxelBlockHash>());

	// *** perform the warp gradient computation and warp updates


	SurfaceTracker<ITMVoxel, ITMWarp, ITMPlainVoxelArray, TMemoryDeviceType, TRACKER_SLAVCHEVA_DIAGNOSTIC>
			motionTracker_PVA(trackerSwitches);

	motionTracker_PVA.CalculateWarpGradient(volume_16_PVA, warped_live_PVA, warps_PVA);
	motionTracker_PVA.SmoothWarpGradient(volume_16_PVA, warped_live_PVA, warps_PVA);
	motionTracker_PVA.UpdateWarps(volume_16_PVA, warped_live_PVA, warps_PVA);

	SurfaceTracker<ITMVoxel, ITMWarp, ITMVoxelBlockHash, TMemoryDeviceType, TRACKER_SLAVCHEVA_DIAGNOSTIC>
			motionTracker_VBH(trackerSwitches);

	motionTracker_VBH.CalculateWarpGradient(volume_16_VBH, warped_live_VBH, warps_VBH);
	motionTracker_VBH.SmoothWarpGradient(volume_16_VBH, warped_live_VBH, warps_VBH);
	motionTracker_VBH.UpdateWarps(volume_16_VBH, warped_live_VBH, warps_VBH);

	// *** test content

	BOOST_REQUIRE(allocatedContentAlmostEqual_Verbose(warps_PVA, warps_VBH, absoluteTolerance, TMemoryDeviceType));


	//_DEBUG
//	ITMWarp warpPVA = ITMSceneManipulationEngine<ITMWarp, ITMPlainVoxelArray>::Inst()
//			.ReadVoxel(warps_PVA, test_pos);
//	warpPVA.print_self();
//	ITMWarp warpVBH = ITMSceneManipulationEngine<ITMWarp, ITMVoxelBlockHash>::Inst()
//			.ReadVoxel(warps_VBH, test_pos);
//	warpVBH.print_self();

	delete volume_16_PVA;
	delete volume_16_VBH;
	delete warped_live_PVA;
	delete warped_live_VBH;

	ITMVoxelVolume<ITMWarp, ITMPlainVoxelArray>* loaded_warps_PVA;
	ITMVoxelVolume<ITMWarp, ITMVoxelBlockHash>* loaded_warps_VBH;
	std::string path_loaded_warps = get_path_warps(prefix, iteration);
	loadVolume(&loaded_warps_PVA, path_loaded_warps, TMemoryDeviceType,
	           Frame16And17Fixture::InitParams<ITMPlainVoxelArray>());
	loadVolume(&loaded_warps_VBH, path_loaded_warps, TMemoryDeviceType,
	           Frame16And17Fixture::InitParams<ITMVoxelBlockHash>());

	BOOST_REQUIRE(contentAlmostEqual_Verbose(warps_PVA, loaded_warps_PVA, absoluteTolerance, TMemoryDeviceType));
	BOOST_REQUIRE(contentAlmostEqual_Verbose(warps_VBH, loaded_warps_VBH, absoluteTolerance, TMemoryDeviceType));

	delete warps_PVA;
	delete warps_VBH;
	delete loaded_warps_PVA;
	delete loaded_warps_VBH;
}

BOOST_AUTO_TEST_CASE(Test_Warp_PVA_VBH_simple_CPU_data_only) {
	SlavchevaSurfaceTracker::Switches switches(true, false, false, false, false);
	Warp_PVA_VBH_simple_subtest<MEMORYDEVICE_CPU>(0, switches);
}

BOOST_AUTO_TEST_CASE(Test_Warp_PVA_VBH_simple_CUDA_data_only) {
	SlavchevaSurfaceTracker::Switches switches(true, false, false, false, false);
	Warp_PVA_VBH_simple_subtest<MEMORYDEVICE_CUDA>(9, switches);
}