//  ================================================================
//  Created by Gregory Kramida on 10/21/19.
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
#pragma once

#include <unordered_map>
#include "../ORUtils/MemoryDeviceType.h"
#include "../ITMLib/ITMLibDefines.h"
#include "../ITMLib/Utils/Configuration.h"
#include "../ITMLib/Objects/Scene/ITMVoxelVolume.h"
#include "../ITMLib/Engines/Manipulation/Interface/ITMSceneManipulationEngine.h"
#include "../ITMLib/Engines/Manipulation/ITMSceneManipulationEngineFactory.h"
#include "../ITMLib/Engines/Manipulation/CPU/ITMSceneManipulationEngine_CPU.h"
#include "../ITMLib/Engines/Manipulation/CUDA/ITMSceneManipulationEngine_CUDA.h"
#include "TestUtilsForSnoopyFrames16And17.h"
#include "../ITMLib/SurfaceTrackers/Interface/SurfaceTracker.h"

using namespace ITMLib;

template<typename TIndex>
std::string getIndexSuffix();

template<>
std::string getIndexSuffix<ITMVoxelBlockHash>() {
	return "VBH";
}

template<>
std::string getIndexSuffix<ITMPlainVoxelArray>() {
	return "PVA";
}


template<MemoryDeviceType TMemoryType, typename TIndex>
struct WarpGradientDataFixture {
	WarpGradientDataFixture() :
			settings(nullptr),
			warp_field_data_term(nullptr), canonical_volume(nullptr), live_volume(nullptr),
			pathToData("TestData/snoopy_result_fr16-17_partial_" + getIndexSuffix<TIndex>() + "/"),
			indexParameters(Frame16And17Fixture::InitParams<TIndex>()) {
		Configuration::load_default();
		settings = &Configuration::get();

		BOOST_TEST_MESSAGE("setup fixture");
		auto loadSdfVolume = [&](ITMVoxelVolume<ITMVoxel, TIndex>** scene, const std::string& pathSuffix) {
			*scene = new ITMVoxelVolume<ITMVoxel, TIndex>(&Configuration::get().scene_parameters,
			                                              settings->swapping_mode ==
			                                              Configuration::SWAPPINGMODE_ENABLED,
			                                              TMemoryType,
			                                              indexParameters);
			PrepareVoxelVolumeForLoading(*scene, TMemoryType);
			(*scene)->LoadFromDirectory(pathToData + pathSuffix);
		};
		auto loadWarpVolume = [&](ITMVoxelVolume<ITMWarp, TIndex>** scene, const std::string& pathSuffix) {
			*scene = new ITMVoxelVolume<ITMWarp, TIndex>(&Configuration::get().scene_parameters,
			                                             settings->swapping_mode ==
			                                             Configuration::SWAPPINGMODE_ENABLED,
			                                             TMemoryType,
			                                             indexParameters);
			PrepareVoxelVolumeForLoading(*scene, TMemoryType);
			(*scene)->LoadFromDirectory(pathToData + pathSuffix);
		};
		loadSdfVolume(&live_volume, "snoopy_partial_frame_17_");
		loadSdfVolume(&canonical_volume, "snoopy_partial_frame_16_");
		loadWarpVolume(&warp_field_data_term, "warp_field_0_data_");
		loadWarpVolume(&warp_field_iter0, "warp_field_0_data_flow_warps_");
		loadWarpVolume(&warp_field_data_term_smoothed, "warp_field_0_smoothed_");
//		loadWarpVolume(&warp_field_tikhonov_term, "warp_field_1_tikhonov_");
		loadWarpVolume(&warp_field_data_and_tikhonov_term, "warp_field_1_data_and_tikhonov_");
		loadWarpVolume(&warp_field_data_and_killing_term, "warp_field_1_data_and_killing_");
		loadWarpVolume(&warp_field_data_and_level_set_term, "warp_field_1_data_and_level_set_");
	}

	~WarpGradientDataFixture() {
		BOOST_TEST_MESSAGE("teardown fixture");
		delete live_volume;
		delete canonical_volume;
		delete warp_field_data_term;
		delete warp_field_iter0;
		delete warp_field_data_term_smoothed;
//		delete warp_field_tikhonov_term;
		delete warp_field_data_and_tikhonov_term;
		delete warp_field_data_and_killing_term;
		delete warp_field_data_and_level_set_term;
	}

	Configuration* settings;
	ITMVoxelVolume<ITMWarp, TIndex>* warp_field_data_term;
	ITMVoxelVolume<ITMWarp, TIndex>* warp_field_data_term_smoothed;
	ITMVoxelVolume<ITMWarp, TIndex>* warp_field_iter0;
	ITMVoxelVolume<ITMWarp, TIndex>* warp_field_tikhonov_term;
	ITMVoxelVolume<ITMWarp, TIndex>* warp_field_data_and_tikhonov_term;
	ITMVoxelVolume<ITMWarp, TIndex>* warp_field_data_and_killing_term;
	ITMVoxelVolume<ITMWarp, TIndex>* warp_field_data_and_level_set_term;
	ITMVoxelVolume<ITMVoxel, TIndex>* canonical_volume;
	ITMVoxelVolume<ITMVoxel, TIndex>* live_volume;
	const std::string pathToData;
	const typename TIndex::InitializationParameters indexParameters;
};


template<typename TIndex>
std::string GetIndexFolderSuffix();

template<>
std::string GetIndexFolderSuffix<ITMVoxelBlockHash>() { return "VBH"; }

template<>
std::string GetIndexFolderSuffix<ITMPlainVoxelArray>() { return "PVA"; }

/// Warning: has to be used when executable is within and two directories deep from the root source folder!! e.g. <root>/build/Tests
template<typename TIndex, MemoryDeviceType TMemoryDeviceType>
void GenerateTestData() {
	//TODO: use CMake-configured header instead to get source folder
	std::string sourceFolder = "../../";
	std::string output_directory =
			sourceFolder + "Tests/TestData/snoopy_result_fr16-17_partial_" + GetIndexFolderSuffix<TIndex>() + "/";

	ITMVoxelVolume<ITMVoxel, TIndex>* canonical_volume;
	ITMVoxelVolume<ITMVoxel, TIndex>* live_volume;
	loadSdfVolume(&live_volume, "snoopy_partial_frame_17_");
	loadSdfVolume(&canonical_volume, "snoopy_partial_frame_16_");

	SlavchevaSurfaceTracker::Switches data_only_switches(true, false, false, false, false);
	std::string data_only_filename = "warp_field_0_data_";
	SlavchevaSurfaceTracker::Switches data_smoothed_switches(false, false, false, false, true);
	std::string data_smoothed_filename = "warp_field_0_smoothed_";
	std::string flow_warps_filename = "warp_field_0_data_flow_warps_";

	std::vector<std::tuple<std::string, SlavchevaSurfaceTracker::Switches>> configurationPairs = {
			std::make_tuple(std::string("warp_field_1_tikhonov_"),
			                SlavchevaSurfaceTracker::Switches(false, false, true, false, false)),
			std::make_tuple(std::string("warp_field_1_data_and_tikhonov_"),
			                SlavchevaSurfaceTracker::Switches(true, false, true, false, false)),
			std::make_tuple(std::string("warp_field_1_data_and_killing_"),
			                SlavchevaSurfaceTracker::Switches(true, false, true, true, false)),
			std::make_tuple(std::string("warp_field_1_data_and_level_set_"),
			                SlavchevaSurfaceTracker::Switches(true, true, false, false, false))
	};



	ITMVoxelVolume<ITMWarp, TIndex> warp_field(&Configuration::get().scene_parameters,
	                                           Configuration::get().swapping_mode ==
	                                           Configuration::SWAPPINGMODE_ENABLED,
	                                           MEMORYDEVICE_CUDA, Frame16And17Fixture::InitParams<TIndex>());
	ITMSceneManipulationEngineFactory::Instance<ITMWarp, TIndex, TMemoryDeviceType>().ResetScene(&warp_field);

	SurfaceTracker<ITMVoxel, ITMWarp, TIndex, TMemoryDeviceType, TRACKER_SLAVCHEVA_DIAGNOSTIC> dataOnlyMotionTracker(
			data_only_switches);
	dataOnlyMotionTracker->CalculateWarpGradient(canonical_volume, live_volume, &warp_field);
	warp_field.SaveToDirectory(output_directory + data_only_filename);

	SurfaceTracker<ITMVoxel, ITMWarp, TIndex, TMemoryDeviceType, TRACKER_SLAVCHEVA_DIAGNOSTIC> dataSmoothedMotionTracker(
			data_smoothed_switches);
	dataSmoothedMotionTracker.SmoothWarpGradient(canonical_volume,live_volume, &warp_field);
	warp_field.SaveToDirectory(output_directory + data_smoothed_filename);

	ITMSceneManipulationEngineFactory::Instance<ITMWarp, TIndex, TMemoryDeviceType>().ResetScene(&warp_field);
	warp_field.LoadFromDirectory(output_directory + data_only_filename);

	dataOnlyMotionTracker.UpdateWarps(canonical_volume, live_volume, &warp_field);
	warp_field.SaveToDirectory(output_directory + flow_warps_filename);


	for (auto& pair : configurationPairs) {
		ITMSceneManipulationEngineFactory::Instance<ITMWarp, TIndex, TMemoryDeviceType>().ResetScene(&warp_field);
		warp_field.LoadFromDirectory(output_directory + flow_warps_filename);
		std::string filename = std::get<0>(pair);
		SurfaceTracker<ITMVoxel, ITMWarp, TIndex, TMemoryDeviceType, TRACKER_SLAVCHEVA_DIAGNOSTIC> tracker(
				std::get<1>(pair));
		tracker.CalculateWarpGradient(canonical_volume, live_volume, &warp_field);
		warp_field.SaveToDirectory(output_directory + filename);
	}

	delete canonical_volume;
	delete live_volume;
}