//  ================================================================
//  Created by Gregory Kramida on 7/12/18.
//  Copyright (c) 2018-2025 Gregory Kramida
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


#include "../../Utils/ITMMath.h"
#include "../../Objects/Scene/ITMScene.h"
#include "ITMSceneMotionTracker_Shared.h"

template<typename TVoxel>
struct WarpClearFunctor {
	_CPU_AND_GPU_CODE_
	static inline void run(TVoxel& voxel) {
		voxel.warp = Vector3f(0.0f);
	}
};

template<typename TVoxelCanonical>
struct ClearOutFramewiseWarpStaticFunctor {
	_CPU_AND_GPU_CODE_
	static inline void run(TVoxelCanonical& voxel) {
		voxel.framewise_warp = Vector3f(0.0f);
	}
};


template<typename TVoxelCanonical>
struct ClearOutGradientStaticFunctor {
	_CPU_AND_GPU_CODE_
	static inline void run(TVoxelCanonical& voxel) {
		voxel.gradient0 = Vector3f(0.0f);
		voxel.gradient1 = Vector3f(0.0f);
	}
};


template<typename TVoxelLive, typename TVoxelCanonical>
struct WarpUpdateFunctor {
	WarpUpdateFunctor(float learningRate, bool gradientSmoothingEnabled, int sourceSdfIndex) :
			learningRate(learningRate), gradientSmoothingEnabled(gradientSmoothingEnabled),
			maxFramewiseWarpLength(0.0f), maxWarpUpdateLength(0.0f), maxFramewiseWarpPosition(0),
			maxWarpUpdatePosition(0), sourceSdfIndex(sourceSdfIndex) {}

	_CPU_AND_GPU_CODE_
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

	_CPU_AND_GPU_CODE_
	void PrintWarp() {
#ifndef __CUDACC__
		std::cout << green << "Max warp: [" << maxFramewiseWarpLength << " at " << maxFramewiseWarpPosition
		          << "] Max update: [" << maxWarpUpdateLength << " at " << maxWarpUpdatePosition << "]." << reset
		          << std::endl;
#endif
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
			binIdx = MIN(histBinCount - 1, (int) (framewiseWarpLength * histBinCount / maxWarpLength));
		}
		warpBins[binIdx]++;
		if (maxWarpUpdateLength > 0) {
			binIdx = MIN(histBinCount - 1,
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

enum TraversalDirection : int {
	X = 0, Y = 1, Z = 2
};

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex, TraversalDirection TDirection>
struct GradientSmoothingPassFunctor {
	GradientSmoothingPassFunctor(ITMLib::ITMScene<TVoxelCanonical, TIndex>* canonicalScene, int sourceSdfFieldIndex) :
			canonicalScene(canonicalScene),
			canonicalVoxels(canonicalScene->localVBA.GetVoxelBlocks()),
			canonicalIndexData(canonicalScene->index.getIndexData()),
			canonicalCache(),
			sourceSdfFieldIndex(sourceSdfFieldIndex) {}

	_CPU_AND_GPU_CODE_
	void operator()(TVoxelLive& liveVoxel, TVoxelCanonical& canonicalVoxel, Vector3i position) {
		const int sobolevFilterSize = 7;
		const float sobolevFilter1D[sobolevFilterSize] = {
				2.995861099047703036e-04f,
				4.410932423926419363e-03f,
				6.571314272194948847e-02f,
				9.956527876693953560e-01f,
				6.571314272194946071e-02f,
				4.410932423926422832e-03f,
				2.995861099045313996e-04f
		};

		int vmIndex = 0;
		if (!VoxelIsConsideredForTracking(canonicalVoxel, liveVoxel, sourceSdfFieldIndex)) return;

		const auto directionIndex = (int) TDirection;

		Vector3i receptiveVoxelPosition = position;
		receptiveVoxelPosition[directionIndex] -= (sobolevFilterSize / 2);
		Vector3f smoothedGradient(0.0f);

		for (int iVoxel = 0; iVoxel < sobolevFilterSize; iVoxel++, receptiveVoxelPosition[directionIndex]++) {
			const TVoxelCanonical& receptiveVoxel = readVoxel(canonicalVoxels, canonicalIndexData,
			                                                  receptiveVoxelPosition, vmIndex, canonicalCache);
			smoothedGradient += sobolevFilter1D[iVoxel] * GetGradient(receptiveVoxel);
		}
		SetGradient(canonicalVoxel, smoothedGradient);
	}

private:
	_CPU_AND_GPU_CODE_
	static inline Vector3f GetGradient(const TVoxelCanonical& voxel) {
		switch (TDirection) {
			case X:
				return voxel.gradient0;
			case Y:
				return voxel.gradient1;
			case Z:
				return voxel.gradient0;
			default:
				return Vector3f(0.0);
		}
	}

	_CPU_AND_GPU_CODE_
	static inline void SetGradient(TVoxelCanonical& voxel, const Vector3f gradient) {
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

	ITMLib::ITMScene<TVoxelCanonical, TIndex>* canonicalScene;
	TVoxelCanonical* canonicalVoxels;
	typename TIndex::IndexData* canonicalIndexData;
	int sourceSdfFieldIndex;
	typename TIndex::IndexCache canonicalCache;

};


template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
inline
void SmoothWarpGradient_common(ITMLib::ITMScene<TVoxelLive, TIndex>* liveScene,
                               ITMLib::ITMScene<TVoxelCanonical, TIndex>* canonicalScene, int sourceFieldIndex) {
	GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, TIndex, X> passFunctorX(canonicalScene, sourceFieldIndex);
	GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, TIndex, Y> passFunctorY(canonicalScene, sourceFieldIndex);
	GradientSmoothingPassFunctor<TVoxelCanonical, TVoxelLive, TIndex, Z> passFunctorZ(canonicalScene, sourceFieldIndex);

	DualVoxelPositionTraversal(liveScene, canonicalScene, passFunctorX);
	DualVoxelPositionTraversal(liveScene, canonicalScene, passFunctorY);
	DualVoxelPositionTraversal(liveScene, canonicalScene, passFunctorZ);
}

template<typename TVoxelCanonical>
struct AddFramewiseWarpToWarpWithClearStaticFunctor {
	_CPU_AND_GPU_CODE_
	static inline void run(TVoxelCanonical& voxel) {
		voxel.warp += voxel.framewise_warp;
		voxel.framewise_warp = Vector3f(0.0f);
	}
};

template<typename TVoxelCanonical>
struct AddFramewiseWarpToWarpStaticFunctor {
	_CPU_AND_GPU_CODE_
	static inline void run(TVoxelCanonical& voxel) {
		voxel.warp += voxel.framewise_warp;
	}
};

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
inline float UpdateWarps_common(
		ITMLib::ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
		ITMLib::ITMScene<TVoxelLive, TIndex>* liveScene, int sourceSdfIndex, float gradientDescentLearningRate,
		bool gradeintSmoothingEnabled) {
	WarpUpdateFunctor<TVoxelLive, TVoxelCanonical>
			warpUpdateFunctor(gradientDescentLearningRate, gradeintSmoothingEnabled,
			                  sourceSdfIndex);

	DualVoxelPositionTraversal(liveScene, canonicalScene, warpUpdateFunctor);
	//don't compute histogram in CUDA version
#ifndef __CUDACC__
	WarpHistogramFunctor<TVoxelLive, TVoxelCanonical>
			warpHistogramFunctor(warpUpdateFunctor.maxFramewiseWarpLength, warpUpdateFunctor.maxWarpUpdateLength,
			                     sourceSdfIndex);
	DualVoxelTraversal(liveScene, canonicalScene, warpHistogramFunctor);
	warpHistogramFunctor.PrintHistogram();
	warpUpdateFunctor.PrintWarp();
#endif
	return warpUpdateFunctor.maxWarpUpdateLength;
}

template<typename TVoxelCanonical, typename TIndex>
inline void
AddFramewiseWarpToWarp_common(ITMLib::ITMScene<TVoxelCanonical, TIndex>* canonicalScene, bool clearFramewiseWarp) {
	if (clearFramewiseWarp) {
		ITMLib::StaticVoxelTraversal<AddFramewiseWarpToWarpWithClearStaticFunctor<TVoxelCanonical>>(canonicalScene);
	} else {
		ITMLib::StaticVoxelTraversal<AddFramewiseWarpToWarpStaticFunctor<TVoxelCanonical>>(canonicalScene);
	}
};