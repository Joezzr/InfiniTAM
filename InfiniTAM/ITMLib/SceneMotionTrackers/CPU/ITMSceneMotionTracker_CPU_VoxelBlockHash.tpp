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
#include "ITMSceneMotionTracker_CPU.h"

#include "../Shared/ITMSceneMotionTracker_Shared_Old.h"
#include "../../Utils/Analytics/ITMSceneStatisticsCalculator.h"
#include "../../Objects/Scene/ITMTrilinearDistribution.h"
#include "../../Engines/Manipulation/ITMSceneManipulation.h"
#include "../../Utils/ITMLibSettings.h"
#include "../../Objects/Scene/ITMSceneTraversal_VoxelBlockHash.h"


using namespace ITMLib;

// region ================================ CONSTRUCTORS AND DESTRUCTORS ================================================


template<typename TVoxelCanonical, typename TVoxelLive>
ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::ITMSceneMotionTracker_CPU(const ITMLibSettings* settings)
		:ITMSceneMotionTracker<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>(settings),
		 calculateGradientFunctor(this->parameters, this->switches) {};
// endregion ============================== END CONSTRUCTORS AND DESTRUCTORS============================================

// region ===================================== HOUSEKEEPING ===========================================================

template<typename TVoxel>
struct WarpClearFunctor {
	static void run(TVoxel& voxel) {
		voxel.warp = Vector3f(0.0f);
	}
};

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::ResetWarps(
		ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene) {
	StaticVoxelTraversal_CPU<WarpClearFunctor<TVoxelCanonical>>(canonicalScene);
};

template<typename TVoxelCanonical>
struct ClearOutFramewiseWarpStaticFunctor {
	static void run(TVoxelCanonical& voxel) {
		voxel.framewise_warp = Vector3f(0.0f);
	}
};

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::ClearOutFramewiseWarp(
		ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene) {

	StaticVoxelTraversal_CPU<ClearOutFramewiseWarpStaticFunctor<TVoxelCanonical>>(canonicalScene);
}


// endregion ===========================================================================================================

//_DEBUG
template<typename TVoxel, typename TIndex>
inline static void PrintSceneStatistics(
		ITMScene<TVoxel, TIndex>* scene,
		std::string description) {
	ITMSceneStatisticsCalculator<TVoxel, TIndex>& calculator = ITMSceneStatisticsCalculator<TVoxel, TIndex>::Instance();
	std::cout << green << "=== Stats for scene '" << description << "' ===" << reset << std::endl;
	std::cout << "    Total voxel count: " << calculator.ComputeAllocatedVoxelCount(scene) << std::endl;
	std::cout << "    NonTruncated voxel count: " << calculator.ComputeNonTruncatedVoxelCount(scene) << std::endl;
	std::cout << "    +1.0 voxel count: " << calculator.ComputeVoxelWithValueCount(scene, 1.0f) << std::endl;
	std::vector<int> allocatedHashes = calculator.GetFilledHashBlockIds(scene);
	std::cout << "    Allocated hash count: " << allocatedHashes.size() << std::endl;
	std::cout << "    NonTruncated SDF sum: " << calculator.ComputeNonTruncatedVoxelAbsSdfSum(scene) << std::endl;
	std::cout << "    Truncated SDF sum: " << calculator.ComputeTruncatedVoxelAbsSdfSum(scene) << std::endl;

};

// region ===================================== CALCULATE GRADIENT SMOOTHING ===========================================

template<typename TVoxelCanonical>
struct ClearOutGradientStaticFunctor {
	static void run(TVoxelCanonical& voxel) {
		voxel.gradient0 = Vector3f(0.0f);
		voxel.gradient1 = Vector3f(0.0f);
	}
};


template<typename TVoxelCanonical, typename TVoxelLive>
void
ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::CalculateWarpGradient(
		ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene,
		ITMScene<TVoxelLive, ITMVoxelBlockHash>* liveScene, bool hasFocusCoordinates,
		const Vector3i& focusCoordinates, int sourceFieldIndex, bool restrictZTrackingForDebugging) {

	StaticVoxelTraversal_CPU<ClearOutGradientStaticFunctor<TVoxelCanonical>>(canonicalScene);
	hashManager.AllocateCanonicalFromLive(canonicalScene, liveScene);
	calculateGradientFunctor.PrepareForOptimization(liveScene, canonicalScene, sourceFieldIndex, hasFocusCoordinates,
	                                                focusCoordinates, restrictZTrackingForDebugging);

	DualVoxelPositionTraversal_CPU(liveScene, canonicalScene, calculateGradientFunctor);

	calculateGradientFunctor.FinalizePrintAndRecordStatistics();
}

// endregion ===========================================================================================================
// region ========================================== SOBOLEV GRADIENT SMOOTHING ========================================

enum TraversalDirection : int {
	X = 0, Y = 1, Z = 2
};


template<typename TVoxelCanonical, typename TVoxelLive, TraversalDirection TDirection>
struct GradientSmoothingPassFunctor {
	GradientSmoothingPassFunctor(ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene, int sourceSdfIndex) :
			canonicalScene(canonicalScene),
			canonicalVoxels(canonicalScene->localVBA.GetVoxelBlocks()),
			canoincalHashEntries(canonicalScene->index.GetEntries()),
			canonicalCache(),
			sourceSdfIndex(sourceSdfIndex) {}

	void operator()(TVoxelLive& liveVoxel, TVoxelCanonical& canonicalVoxel, Vector3i position) {
		int vmIndex;
		if (!VoxelIsConsideredForTracking(canonicalVoxel, liveVoxel, sourceSdfIndex)) return;

		const int directionIndex = (int) TDirection;

		Vector3i receptiveVoxelPosition = position;
		receptiveVoxelPosition[directionIndex] -= (sobolevFilterSize / 2);
		Vector3f smoothedGradient(0.0f);

		for (int iVoxel = 0; iVoxel < sobolevFilterSize; iVoxel++, receptiveVoxelPosition[directionIndex]++) {
			const TVoxelCanonical& receptiveVoxel = readVoxel(canonicalVoxels, canoincalHashEntries,
			                                                  receptiveVoxelPosition, vmIndex, canonicalCache);
			smoothedGradient += sobolevFilter1D[iVoxel] * GetGradient(receptiveVoxel);
		}
		SetGradient(canonicalVoxel, smoothedGradient);
	}

private:
	Vector3f GetGradient(const TVoxelCanonical& voxel) const {
		switch (TDirection) {
			case X:
				return voxel.gradient0;
			case Y:
				return voxel.gradient1;
			case Z:
				return voxel.gradient0;
		}
	}

	void SetGradient(TVoxelCanonical& voxel, const Vector3f gradient) const {
		switch (TDirection) {
			case X:
				voxel.gradient1 = gradient;
				return;
			case Y:
				voxel.gradient0 = gradient;
				return;
			case Z:
				voxel.gradient1 = gradient;
				return;
		}
	}

	ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene;
	TVoxelCanonical* canonicalVoxels;
	ITMHashEntry* canoincalHashEntries;
	int sourceSdfIndex;
	typename ITMVoxelBlockHash::IndexCache canonicalCache;

	static const int sobolevFilterSize;
	static const float sobolevFilter1D[];
};

template<typename TVoxelCanonical, typename TVoxelLive, TraversalDirection TDirection>
const int GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, TDirection>::sobolevFilterSize = 7;
template<typename TVoxelCanonical, typename TVoxelLive, TraversalDirection TDirection>
const float GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, TDirection>::sobolevFilter1D[] = {
		2.995861099047703036e-04f,
		4.410932423926419363e-03f,
		6.571314272194948847e-02f,
		9.956527876693953560e-01f,
		6.571314272194946071e-02f,
		4.410932423926422832e-03f,
		2.995861099045313996e-04f};


template<typename TVoxelCanonical, typename TVoxelLive>
void ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::SmoothWarpGradient(
		ITMScene<TVoxelLive, ITMVoxelBlockHash>* liveScene,
		ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene, int sourceFieldIndex) {

	if (this->switches.enableGradientSmoothing) {
		GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, X> passFunctorX(canonicalScene, sourceFieldIndex);
		GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, Y> passFunctorY(canonicalScene, sourceFieldIndex);
		GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, Z> passFunctorZ(canonicalScene, sourceFieldIndex);

		DualVoxelPositionTraversal_CPU(liveScene, canonicalScene, passFunctorX);
		DualVoxelPositionTraversal_CPU(liveScene, canonicalScene, passFunctorY);
		DualVoxelPositionTraversal_CPU(liveScene, canonicalScene, passFunctorZ);
	}
}

// endregion ===========================================================================================================

// region ============================= UPDATE FRAMEWISE & GLOBAL (CUMULATIVE) WARPS ===================================

template<typename TVoxelLive, typename TVoxelCanonical>
struct WarpUpdateFunctor {
	WarpUpdateFunctor(float learningRate, bool gradientSmoothingEnabled, int sourceSdfIndex) :
			learningRate(learningRate), gradientSmoothingEnabled(gradientSmoothingEnabled),
			maxFramewiseWarpLength(0.0f), maxWarpUpdateLength(0.0f), maxFramewiseWarpPosition(0),
			maxWarpUpdatePosition(0), sourceSdfIndex(sourceSdfIndex) {}

	void operator()(TVoxelLive& liveVoxel, TVoxelCanonical& canonicalVoxel, const Vector3i& position) {
		if (!VoxelIsConsideredForTracking(canonicalVoxel, liveVoxel, sourceSdfIndex)) return;
		Vector3f warpUpdate = -learningRate * (gradientSmoothingEnabled ?
		                                       canonicalVoxel.gradient1 : canonicalVoxel.gradient0);

		canonicalVoxel.warp_update = warpUpdate;
		canonicalVoxel.framewise_warp += warpUpdate;

		// update stats
		float framewiseWarpLength = ORUtils::length(canonicalVoxel.framewise_warp);
		float warpUpdateLength = ORUtils::length(warpUpdate);
		if (framewiseWarpLength > maxFramewiseWarpLength) {
			maxFramewiseWarpLength = framewiseWarpLength;
			maxFramewiseWarpPosition = position;
		}
		if (warpUpdateLength > maxWarpUpdateLength) {
			maxWarpUpdateLength = warpUpdateLength;
			maxWarpUpdatePosition = position;
		}
	}

	float maxFramewiseWarpLength;
	float maxWarpUpdateLength;
	Vector3i maxFramewiseWarpPosition;
	Vector3i maxWarpUpdatePosition;


	void PrintWarp() {
		std::cout << green << "Max warp: [" << maxFramewiseWarpLength << " at " << maxFramewiseWarpPosition
		          << "] Max update: [" << maxWarpUpdateLength << " at " << maxWarpUpdatePosition << "]." << reset
		          << std::endl;
	}

private:
	const float learningRate;
	const bool gradientSmoothingEnabled;
	const int sourceSdfIndex;
};

template<typename TVoxelLive, typename TVoxelCanonical>
struct WarpHistogramFunctor {
	WarpHistogramFunctor(float maxWarpLength, float maxWarpUpdateLength, int sourceSdfIndex) :
			maxWarpLength(maxWarpLength), maxWarpUpdateLength(maxWarpUpdateLength), sourceSdfIndex(sourceSdfIndex) {
	}

	static const int histBinCount = 10;

	void operator()(TVoxelLive& liveVoxel, TVoxelCanonical& canonicalVoxel) {
		if (!VoxelIsConsideredForTracking(canonicalVoxel, liveVoxel, sourceSdfIndex)) return;
		float framewiseWarpLength = ORUtils::length(canonicalVoxel.framewise_warp);
		float warpUpdateLength = ORUtils::length(canonicalVoxel.gradient0);
		const int histBinCount = WarpHistogramFunctor<TVoxelLive, TVoxelCanonical>::histBinCount;
		int binIdx = 0;
		if (maxWarpLength > 0) {
			binIdx = std::min(histBinCount - 1, (int) (framewiseWarpLength * histBinCount / maxWarpLength));
		}
		warpBins[binIdx]++;
		if (maxWarpUpdateLength > 0) {
			binIdx = std::min(histBinCount - 1,
			                  (int) (warpUpdateLength * histBinCount / maxWarpUpdateLength));
		}
		updateBins[binIdx]++;
	}

	void PrintHistogram() {
		std::cout << "FW warp length histogram: ";
		for (int iBin = 0; iBin < histBinCount; iBin++) {
			std::cout << std::setfill(' ') << std::setw(7) << warpBins[iBin] << "  ";
		}
		std::cout << std::endl;
		std::cout << "Update length histogram: ";
		for (int iBin = 0; iBin < histBinCount; iBin++) {
			std::cout << std::setfill(' ') << std::setw(7) << updateBins[iBin] << "  ";
		}
		std::cout << std::endl;
	}

private:
	const float maxWarpLength;
	const float maxWarpUpdateLength;
	const int sourceSdfIndex;

	// <20%, 40%, 60%, 80%, 100%
	int warpBins[histBinCount] = {0};
	int updateBins[histBinCount] = {0};
};


template<typename TVoxelCanonical, typename TVoxelLive>
float ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::UpdateWarps(
		ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene,
		ITMScene<TVoxelLive, ITMVoxelBlockHash>* liveScene, int sourceSdfIndex) {

	WarpUpdateFunctor<TVoxelLive, TVoxelCanonical>
			warpUpdateFunctor(this->parameters.gradientDescentLearningRate, this->switches.enableGradientSmoothing,
			                  sourceSdfIndex);

	DualVoxelPositionTraversal_CPU(liveScene, canonicalScene, warpUpdateFunctor);

	WarpHistogramFunctor<TVoxelLive, TVoxelCanonical>
			warpHistogramFunctor(warpUpdateFunctor.maxFramewiseWarpLength, warpUpdateFunctor.maxWarpUpdateLength,
			                     sourceSdfIndex);
	DualVoxelTraversal_CPU(liveScene, canonicalScene, warpHistogramFunctor);

	warpHistogramFunctor.PrintHistogram();
	warpUpdateFunctor.PrintWarp();

	return warpUpdateFunctor.maxWarpUpdateLength;
}

template<typename TVoxelCanonical>
struct AddFramewiseWarpToWarpWithClearStaticFunctor {
	static void run(TVoxelCanonical& voxel) {
		voxel.warp += voxel.framewise_warp;
		voxel.framewise_warp = Vector3f(0.0f);
	}
};

template<typename TVoxelCanonical>
struct AddFramewiseWarpToWarpStaticFunctor {
	static void run(TVoxelCanonical& voxel) {
		voxel.warp += voxel.framewise_warp;
	}
};

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMSceneMotionTracker_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::AddFramewiseWarpToWarp(
		ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* canonicalScene, bool clearFramewiseWarp) {
	if (clearFramewiseWarp) {
		StaticVoxelTraversal_CPU<AddFramewiseWarpToWarpWithClearStaticFunctor<TVoxelCanonical>>(canonicalScene);
	} else {
		StaticVoxelTraversal_CPU<AddFramewiseWarpToWarpStaticFunctor<TVoxelCanonical>>(canonicalScene);
	}
}

//endregion ============================================================================================================