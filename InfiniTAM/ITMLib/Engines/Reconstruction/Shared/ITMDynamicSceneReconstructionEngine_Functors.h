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

#include "ITMDynamicSceneReconstructionEngine_Shared.h"
#include "../../../Objects/Scene/ITMTrilinearInterpolation.h"
#include "../../../Objects/Scene/ITMVoxelVolume.h"
#include "../../../Utils/Configuration.h"
#include "../../../../ORUtils/PlatformIndependentAtomics.h"
#include "../../Common/ITMWarpEnums.h"
#include "../../Common/ITMCommonFunctors.h"
#include "../../Manipulation/Shared/ITMSceneManipulationEngine_Shared.h"


template<typename TVoxel>
struct FieldClearFunctor {
	FieldClearFunctor() {}

	_CPU_AND_GPU_CODE_
	void operator()(TVoxel& voxel) {
		voxel.flags = ITMLib::VOXEL_UNKNOWN;
		voxel.sdf = TVoxel::SDF_initialValue();
	}
};


template<typename TVoxel>
struct TSDFFusionFunctor {
	TSDFFusionFunctor(int maximumWeight) :
			maximumWeight(maximumWeight) {}

	_CPU_AND_GPU_CODE_
	void operator()(TVoxel& liveVoxel, TVoxel& canonicalVoxel) {
		fuseLiveVoxelIntoCanonical(liveVoxel, maximumWeight, canonicalVoxel);
	}

private:
	const int maximumWeight;
};


template<typename TVoxel, typename TWarp, typename TIndex, ITMLib::WarpType TWarpType, MemoryDeviceType TMemoryDeviceType>
struct TrilinearInterpolationFunctor {
	/**
	 * \brief Initialize to transfer data from source sdf scene to a target sdf scene using the warps in the warp source scene
	 * \details traverses
	 * \param sourceTSDF
	 * \param warpSourceScene
	 */
	TrilinearInterpolationFunctor(ITMLib::ITMVoxelVolume<TVoxel, TIndex>* sourceTSDF,
	                              ITMLib::ITMVoxelVolume<TWarp, TIndex>* warpField) :

			sdfSourceScene(sourceTSDF),
			sdfSourceVoxels(sourceTSDF->localVBA.GetVoxelBlocks()),
			sdfSourceIndexData(sourceTSDF->index.GetIndexData()),
			sdfSourceCache(),

			warpSourceScene(warpField),
			warpSourceVoxels(warpField->localVBA.GetVoxelBlocks()),
			warpSourceHashEntries(warpField->index.GetIndexData()),
			warpSourceCache(),

			hasFocusCoordinates(ITMLib::Configuration::get().telemetry_settings.focus_coordinates_specified),
			focusCoordinates(ITMLib::Configuration::get().telemetry_settings.focus_coordinates) {
		INITIALIZE_ATOMIC(int, hedgehog, 0);
	}

	~TrilinearInterpolationFunctor() {
		CLEAN_UP_ATOMIC(hedgehog);
	}

	_DEVICE_WHEN_AVAILABLE_
	void operator()(TVoxel& destinationVoxel, TWarp& warp,
	                Vector3i warpAndDestinationVoxelPosition) {

		bool printResult = hasFocusCoordinates && warpAndDestinationVoxelPosition == focusCoordinates;

		interpolateTSDFVolume<TVoxel, TWarp, TIndex, TWarpType>(
				sdfSourceVoxels, sdfSourceIndexData, sdfSourceCache, warp, destinationVoxel,
				warpAndDestinationVoxelPosition, printResult, checkVoxelIndex);

		if (TMemoryDeviceType == MEMORYDEVICE_CUDA) {
			TVoxel& voxToCheck = targetVoxelBlocks[checkVoxelIndex];
			int x;
			if (voxToCheck.sdf != 1.0f && 0 == (x = ATOMIC_ADD(hedgehog, 1))) {

				Vector3s blockA(-7, -1, 20);
				Vector3s blockB(-1, 1, 25);
				int codeA = FindHashCodeAt(targetEntries, blockA);
				int codeB = FindHashCodeAt(targetEntries, blockB);

				printf("DISASTA! Reported on voxel %d %d %d. Entry A: %d %d. Entry B: %d %d\n",
						warpAndDestinationVoxelPosition.x, warpAndDestinationVoxelPosition.y,
				       warpAndDestinationVoxelPosition.z,
				       codeA, targetEntries[codeA].ptr, codeB, targetEntries[codeB].ptr);
			}
		}


	}

//_DEBUG
	int checkVoxelIndex;
	TVoxel* targetVoxelBlocks;
	ITMHashEntry* targetEntries;
	DECLARE_ATOMIC_INT(hedgehog);
private:


	ITMLib::ITMVoxelVolume<TVoxel, TIndex>* sdfSourceScene;
	TVoxel* sdfSourceVoxels;
	typename TIndex::IndexData* sdfSourceIndexData;
	typename TIndex::IndexCache sdfSourceCache;

	ITMLib::ITMVoxelVolume<TWarp, TIndex>* warpSourceScene;
	TWarp* warpSourceVoxels;
	typename TIndex::IndexData* warpSourceHashEntries;
	typename TIndex::IndexCache warpSourceCache;


	//_DEBUG
	bool hasFocusCoordinates;
	Vector3i focusCoordinates;
};

template<typename TVoxel>
struct CopySceneFunctor {
	CopySceneFunctor() = default;

	static void run(TVoxel& sourceVoxel, TVoxel& destinationVoxel, Vector3i voxelPosition) {
		destinationVoxel.sdf = sourceVoxel.sdf;
		destinationVoxel.flags = sourceVoxel.flags;
	}
};