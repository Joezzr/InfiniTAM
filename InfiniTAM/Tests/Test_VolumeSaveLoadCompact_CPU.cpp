//  ================================================================
//  Created by Gregory Kramida on 9/5/19.
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
#define BOOST_TEST_MODULE SceneConstruction
#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

//boost
#include <boost/test/unit_test.hpp>
//ITMLib
#include "../ITMLib/ITMLibDefines.h"
#include "../ITMLib/Objects/Scene/ITMVoxelVolume.h"
#include "../ITMLib/Utils/ITMLibSettings.h"
#include "../ITMLib/Engines/Manipulation/CPU/ITMSceneManipulationEngine_CPU.h"
#include "TestUtils.h"
#include "../ITMLib/Engines/SceneFileIO/ITMSceneFileIOEngine.h"
#include "../ITMLib/Utils/Analytics/SceneStatisticsCalculator/CPU/ITMSceneStatisticsCalculator_CPU.h"
#include "../ITMLib/Utils/Analytics/VoxelVolumeComparison/ITMVoxelVolumeComparison_CPU.h"

using namespace ITMLib;

typedef ITMSceneFileIOEngine<ITMVoxel, ITMPlainVoxelArray> SceneFileIOEngine_PVA;
typedef ITMSceneFileIOEngine<ITMVoxel, ITMVoxelBlockHash> SceneFileIOEngine_VBH;
typedef ITMSceneStatisticsCalculator_CPU<ITMVoxel, ITMPlainVoxelArray> SceneStatisticsCalculator_PVA;
typedef ITMSceneStatisticsCalculator_CPU<ITMVoxel, ITMVoxelBlockHash> SceneStatisticsCalculator_VBH;

BOOST_AUTO_TEST_CASE(testSaveSceneCompact_CPU) {

	ITMLibSettings* settings = &ITMLibSettings::Instance();
	settings->deviceType = ITMLibSettings::DEVICE_CPU;

	Vector3i volumeSize(40, 68, 20);
	Vector3i volumeOffset(-20, 0, 0);

	ITMVoxelVolume<ITMVoxel, ITMPlainVoxelArray> scene1(
			&settings->sceneParams, settings->swappingMode == ITMLibSettings::SWAPPINGMODE_ENABLED,
			settings->GetMemoryType(), volumeSize, volumeOffset);

	ITMVoxelVolume<ITMVoxel, ITMPlainVoxelArray> scene2(
			&settings->sceneParams, settings->swappingMode == ITMLibSettings::SWAPPINGMODE_ENABLED,
			settings->GetMemoryType(), volumeSize, volumeOffset);

	GenerateTestScene_CPU(&scene1);
	std::string path = "TestData/test_PVA_";
	SceneFileIOEngine_PVA::SaveToDirectoryCompact(&scene1, path);
	ManipulationEngine_CPU_PVA_Voxel::Inst().ResetScene(&scene2);
	SceneFileIOEngine_PVA::LoadFromDirectoryCompact(&scene2, path);

	float tolerance = 1e-8;
	BOOST_REQUIRE_EQUAL( SceneStatisticsCalculator_PVA::Instance().ComputeNonTruncatedVoxelCount(&scene2), 19456);
	BOOST_REQUIRE(contentAlmostEqual_CPU(&scene1, &scene2, tolerance));

	ITMVoxelVolume<ITMVoxel, ITMVoxelBlockHash> scene3(
			&settings->sceneParams, settings->swappingMode == ITMLibSettings::SWAPPINGMODE_ENABLED,
			settings->GetMemoryType());

	ITMVoxelVolume<ITMVoxel, ITMVoxelBlockHash> scene4(
			&settings->sceneParams, settings->swappingMode == ITMLibSettings::SWAPPINGMODE_ENABLED,
			settings->GetMemoryType());

	GenerateTestScene_CPU(&scene3);
	path = "TestData/test_VBH_";
	SceneFileIOEngine_VBH::SaveToDirectoryCompact(&scene3, path);
	ManipulationEngine_CPU_VBH_Voxel::Inst().ResetScene(&scene4);
	SceneFileIOEngine_VBH::LoadFromDirectoryCompact(&scene4, path);

	BOOST_REQUIRE_EQUAL( SceneStatisticsCalculator_VBH::Instance().ComputeNonTruncatedVoxelCount(&scene4), 19456);
	BOOST_REQUIRE(contentAlmostEqual_CPU(&scene3, &scene4, tolerance));
	BOOST_REQUIRE(contentAlmostEqual_CPU(&scene1, &scene4, tolerance));
}