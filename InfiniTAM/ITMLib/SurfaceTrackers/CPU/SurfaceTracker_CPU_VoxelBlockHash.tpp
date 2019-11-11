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
#include <cmath>
#include <iomanip>
#include <unordered_set>
#include <chrono>

//_DEBUG -- OpenCV
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <unordered_map>
#include <opencv/cv.hpp>

//local
#include "SurfaceTracker_CPU.h"

#include "../../Engines/Traversal/CPU/ITMSceneTraversal_CPU_VoxelBlockHash.h"
#include "../Shared/ITMSceneMotionTracker_Functors.h"

#include "../../Utils/Analytics/SceneStatisticsCalculator/CPU/ITMSceneStatisticsCalculator_CPU.h"
#include "../../Objects/Scene/ITMTrilinearDistribution.h"
#include "../../Engines/Manipulation/CPU/ITMSceneManipulationEngine_CPU.h"
#include "../../Utils/Configuration.h"
#include "../Shared/ITMCalculateWarpGradientFunctor.h"


using namespace ITMLib;

// region ===================================== HOUSEKEEPING ===========================================================


template<typename TVoxel, typename TWarp>
void SurfaceTracker<TVoxel, TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::ResetWarps(
		ITMVoxelVolume<TWarp, ITMVoxelBlockHash>* warpField) {
	ITMSceneTraversalEngine<TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::template
	StaticVoxelTraversal<WarpClearFunctor<TWarp, TWarp::hasCumulativeWarp>>(warpField);
};


template<typename TVoxel, typename TWarp>
void SurfaceTracker<TVoxel, TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::ClearOutFlowWarp(
		ITMVoxelVolume<TWarp, ITMVoxelBlockHash>* warpField) {
	ITMSceneTraversalEngine<TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::template
	StaticVoxelTraversal<ClearOutFlowWarpStaticFunctor<TWarp>>(warpField);
}


// endregion ===========================================================================================================

template<typename TVoxel, typename TIndex>
inline static void PrintSceneStatistics(
		ITMVoxelVolume<TVoxel, TIndex>* scene,
		std::string description) {
	ITMSceneStatisticsCalculator_CPU<TVoxel, TIndex>& calculator = ITMSceneStatisticsCalculator_CPU<TVoxel, TIndex>::Instance();
	std::cout << green << "=== Stats for scene '" << description << "' ===" << reset << std::endl;
	std::cout << "    Total voxel count: " << calculator.ComputeAllocatedVoxelCount(scene) << std::endl;
	std::cout << "    NonTruncated voxel count: " << calculator.ComputeNonTruncatedVoxelCount(scene) << std::endl;
	std::cout << "    +1.0 voxel count: " << calculator.CountVoxelsWithSpecificSdfValue(scene, 1.0f) << std::endl;
	std::vector<int> allocatedHashes = calculator.GetFilledHashBlockIds(scene);
	std::cout << "    Allocated hash count: " << allocatedHashes.size() << std::endl;
	std::cout << "    NonTruncated SDF sum: " << calculator.ComputeNonTruncatedVoxelAbsSdfSum(scene) << std::endl;
	std::cout << "    Truncated SDF sum: " << calculator.ComputeTruncatedVoxelAbsSdfSum(scene) << std::endl;
};

// region ===================================== CALCULATE GRADIENT SMOOTHING ===========================================

template<typename TVoxel, typename TWarp>
void
SurfaceTracker<TVoxel, TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::CalculateWarpGradient(
		ITMVoxelVolume<TVoxel, ITMVoxelBlockHash>* canonicalScene,
		ITMVoxelVolume<TVoxel, ITMVoxelBlockHash>* liveScene,
		ITMVoxelVolume<TWarp, ITMVoxelBlockHash>* warpField) {

	// manage hash
	ITMSceneTraversalEngine<TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::template
	StaticVoxelTraversal<ClearOutGradientStaticFunctor<TWarp>>(warpField);
	ITMIndexingEngine<TVoxel, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::Instance()
			.AllocateUsingOtherVolume(canonicalScene, liveScene);
	ITMIndexingEngine<TVoxel, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::Instance()
			.AllocateUsingOtherVolume(warpField, liveScene);

	ITMCalculateWarpGradientFunctor<TVoxel, TWarp, ITMVoxelBlockHash::IndexData, ITMVoxelBlockHash::IndexCache>
			calculateGradientFunctor(this->parameters, this->switches,
			                         liveScene->localVBA.GetVoxelBlocks(), liveScene->index.GetIndexData(),
			                         canonicalScene->localVBA.GetVoxelBlocks(), canonicalScene->index.GetIndexData(),
			                         warpField->localVBA.GetVoxelBlocks(), warpField->index.GetIndexData());

	ITMDualSceneWarpTraversalEngine<TVoxel, TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::
	DualVoxelPositionTraversal(liveScene, canonicalScene, warpField, calculateGradientFunctor);

	calculateGradientFunctor.PrintStatistics();
}

// endregion ===========================================================================================================
// region ========================================== SOBOLEV GRADIENT SMOOTHING ========================================


template<typename TVoxel, typename TWarp>
void SurfaceTracker<TVoxel, TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::SmoothWarpGradient(
		ITMVoxelVolume<TVoxel, ITMVoxelBlockHash>* canonicalScene,
		ITMVoxelVolume<TVoxel, ITMVoxelBlockHash>* liveScene,
		ITMVoxelVolume<TWarp, ITMVoxelBlockHash>* warpField) {

	if (this->switches.enableSobolevGradientSmoothing) {
		SmoothWarpGradient_common<TVoxel, TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>
				(liveScene, canonicalScene, warpField);
	}
}

// endregion ===========================================================================================================

// region ============================= UPDATE FRAMEWISE & GLOBAL (CUMULATIVE) WARPS ===================================
template<typename TVoxel, typename TWarp>
float SurfaceTracker<TVoxel, TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::UpdateWarps(
		ITMVoxelVolume<TVoxel, ITMVoxelBlockHash>* canonicalScene,
		ITMVoxelVolume<TVoxel, ITMVoxelBlockHash>* liveScene,
		ITMVoxelVolume<TWarp, ITMVoxelBlockHash>* warpField) {
	return UpdateWarps_common<TVoxel, TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>(
			canonicalScene, liveScene, warpField, this->parameters.gradientDescentLearningRate,
			this->switches.enableSobolevGradientSmoothing);
}

template<typename TVoxel, typename TWarp>
void SurfaceTracker<TVoxel, TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>::AddFlowWarpToWarp(
		ITMVoxelVolume<TWarp, ITMVoxelBlockHash>* warpField, bool clearFlowWarp) {
	AddFlowWarpToWarp_common<TWarp, ITMVoxelBlockHash, MEMORYDEVICE_CPU>(warpField, clearFlowWarp);
}

//endregion ============================================================================================================