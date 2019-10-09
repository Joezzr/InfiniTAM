//  ================================================================
//  Created by Gregory Kramida on 7/26/18.
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

//stdlib
#include <cassert>

//local
#include "../Interface/ITMSceneTraversal.h"
#include "../../../Objects/Scene/ITMVoxelVolume.h"
#include "../../../Objects/Scene/ITMPlainVoxelArray.h"
#include "../../../Utils/ITMLibSettings.h"
#include "ITMSceneTraversal_CUDA_PlainVoxelArray_Kernels.h"

namespace ITMLib {


template<typename TVoxel>
class ITMSceneTraversalEngine<TVoxel, ITMPlainVoxelArray, ITMLibSettings::DEVICE_CUDA> {
public:
// region ================================ STATIC SINGLE-SCENE TRAVERSAL ===============================================
	template<typename TStaticFunctor>
	inline static void StaticVoxelTraversal(ITMVoxelVolume<TVoxel, ITMPlainVoxelArray>* scene) {
		TVoxel* voxelArray = scene->localVBA.GetVoxelBlocks();
		const ITMPlainVoxelArray::ITMVoxelArrayInfo* arrayInfo = scene->index.getIndexData();

		dim3 cudaBlockSize(SDF_BLOCK_SIZE, SDF_BLOCK_SIZE, SDF_BLOCK_SIZE);
		dim3 gridSize(scene->index.getVolumeSize().x / cudaBlockSize.x,
		              scene->index.getVolumeSize().y / cudaBlockSize.y,
		              scene->index.getVolumeSize().z / cudaBlockSize.z);

		staticVoxelTraversal_device<TStaticFunctor, TVoxel> << < gridSize, cudaBlockSize >> > (voxelArray, arrayInfo);
		ORcudaKernelCheck;
	}
// endregion ===========================================================================================================
// region ================================ DYNAMIC SINGLE-SCENE TRAVERSAL ==============================================
	template<typename TFunctor>
	inline static void
	VoxelTraversal(ITMVoxelVolume<TVoxel, ITMPlainVoxelArray>* scene, TFunctor& functor) {
		TVoxel* voxelArray = scene->localVBA.GetVoxelBlocks();
		const ITMPlainVoxelArray::ITMVoxelArrayInfo* arrayInfo = scene->index.getIndexData();

		dim3 cudaBlockSize(SDF_BLOCK_SIZE, SDF_BLOCK_SIZE, SDF_BLOCK_SIZE);
		dim3 gridSize(scene->index.getVolumeSize().x / cudaBlockSize.x,
		              scene->index.getVolumeSize().y / cudaBlockSize.y,
		              scene->index.getVolumeSize().z / cudaBlockSize.z);

		// transfer functor from RAM to VRAM
		TFunctor* functor_device = nullptr;
		ORcudaSafeCall(cudaMalloc((void**) &functor_device, sizeof(TFunctor)));
		ORcudaSafeCall(cudaMemcpy(functor_device, &functor, sizeof(TFunctor), cudaMemcpyHostToDevice));

		voxelTraversal_device<TFunctor, TVoxel> << < gridSize, cudaBlockSize >> > (voxelArray, arrayInfo, functor_device);
		ORcudaKernelCheck;

		// transfer functor from VRAM back to RAM
		ORcudaSafeCall(cudaMemcpy(&functor, functor_device, sizeof(TFunctor), cudaMemcpyDeviceToHost));
		ORcudaSafeCall(cudaFree(functor_device));
	}
// endregion ===========================================================================================================
};

template<typename TVoxelPrimary, typename TVoxelSecondary>
class ITMDualSceneTraversalEngine<TVoxelPrimary, TVoxelSecondary, ITMPlainVoxelArray, ITMPlainVoxelArray, ITMLibSettings::DEVICE_CUDA> {
public:
// region ============================== DUAL-SCENE STATIC TRAVERSAL ===================================================

	template<typename TStaticBooleanFunctor>
	inline static bool StaticDualVoxelTraversal_AllTrue(
			ITMVoxelVolume<TVoxelPrimary, ITMPlainVoxelArray>* primaryScene,
			ITMVoxelVolume<TVoxelSecondary, ITMPlainVoxelArray>* secondaryScene) {

		bool* falseEncountered_device = nullptr;
		ORcudaSafeCall(cudaMalloc((void**) &falseEncountered_device, sizeof(bool)));
		ORcudaSafeCall(cudaMemset(falseEncountered_device, 0, sizeof(bool)));


		TVoxelPrimary* primaryVoxels = primaryScene->localVBA.GetVoxelBlocks();
		TVoxelSecondary* secondaryVoxels = secondaryScene->localVBA.GetVoxelBlocks();
		const ITMPlainVoxelArray::ITMVoxelArrayInfo* arrayInfo = primaryScene->index.getIndexData();
		dim3 cudaBlockSize(SDF_BLOCK_SIZE, SDF_BLOCK_SIZE, SDF_BLOCK_SIZE);
		dim3 gridSize(
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().x) / cudaBlockSize.x)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().y) / cudaBlockSize.y)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().z) / cudaBlockSize.z))
		);

		staticDualVoxelTraversal_AllTrue_device<TStaticBooleanFunctor, TVoxelPrimary, TVoxelSecondary>
				<< < gridSize, cudaBlockSize >> >
		                       (primaryVoxels, secondaryVoxels, arrayInfo, falseEncountered_device);
		ORcudaKernelCheck;

		bool falseEncountered;
		ORcudaSafeCall(cudaMemcpy(&falseEncountered, falseEncountered_device, sizeof(bool), cudaMemcpyDeviceToHost));
		ORcudaSafeCall(cudaFree(falseEncountered_device));

		return !falseEncountered;
	}

// endregion
// region ============================== DUAL-SCENE DYNAMIC TRAVERSAL ==================================================

	template<typename TFunctor>
	inline static void
	DualVoxelPositionTraversal(
			ITMVoxelVolume<TVoxelPrimary, ITMPlainVoxelArray>* primaryScene,
			ITMVoxelVolume<TVoxelSecondary, ITMPlainVoxelArray>* secondaryScene,
			TFunctor& functor) {

		assert(primaryScene->index.getVolumeSize() == secondaryScene->index.getVolumeSize());

		// transfer functor from RAM to VRAM
		TFunctor* functor_device = nullptr;
		ORcudaSafeCall(cudaMalloc((void**) &functor_device, sizeof(TFunctor)));
		ORcudaSafeCall(cudaMemcpy(functor_device, &functor, sizeof(TFunctor), cudaMemcpyHostToDevice));

		// perform traversal on the GPU
		TVoxelPrimary* primaryVoxels = primaryScene->localVBA.GetVoxelBlocks();
		TVoxelSecondary* secondaryVoxels = secondaryScene->localVBA.GetVoxelBlocks();
		const ITMPlainVoxelArray::ITMVoxelArrayInfo* arrayInfo = primaryScene->index.getIndexData();
		dim3 cudaBlockSize(SDF_BLOCK_SIZE, SDF_BLOCK_SIZE, SDF_BLOCK_SIZE);
		dim3 gridSize(
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().x) / cudaBlockSize.x)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().y) / cudaBlockSize.y)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().z) / cudaBlockSize.z))
		);
		dualVoxelPositionTraversal_device<TFunctor, TVoxelPrimary, TVoxelSecondary>
				<< < gridSize, cudaBlockSize >> >
		                       (primaryVoxels, secondaryVoxels, arrayInfo, functor_device);
		ORcudaKernelCheck;

		// transfer functor from VRAM back to RAM
		ORcudaSafeCall(cudaMemcpy(&functor, functor_device, sizeof(TFunctor), cudaMemcpyDeviceToHost));
		ORcudaSafeCall(cudaFree(functor_device));

	}

	template<typename TBooleanFunctor>
	inline static bool
	DualVoxelTraversal_AllTrue(
			ITMVoxelVolume<TVoxelPrimary, ITMPlainVoxelArray>* primaryScene,
			ITMVoxelVolume<TVoxelSecondary, ITMPlainVoxelArray>* secondaryScene,
			TBooleanFunctor& functor) {

		assert(primaryScene->index.getVolumeSize() == secondaryScene->index.getVolumeSize());

		// allocate boolean varaible for answer
		bool* falseEncountered_device = nullptr;
		ORcudaSafeCall(cudaMalloc((void**) &falseEncountered_device, sizeof(bool)));
		ORcudaSafeCall(cudaMemset(falseEncountered_device, 0, sizeof(bool)));

		// transfer functor from RAM to VRAM
		TBooleanFunctor* functor_device = nullptr;
		ORcudaSafeCall(cudaMalloc((void**) &functor_device, sizeof(TBooleanFunctor)));
		ORcudaSafeCall(cudaMemcpy(functor_device, &functor, sizeof(TBooleanFunctor), cudaMemcpyHostToDevice));


		// perform traversal on the GPU
		TVoxelPrimary* primaryVoxels = primaryScene->localVBA.GetVoxelBlocks();
		TVoxelSecondary* secondaryVoxels = secondaryScene->localVBA.GetVoxelBlocks();
		const ITMPlainVoxelArray::ITMVoxelArrayInfo* arrayInfo = primaryScene->index.getIndexData();
		dim3 cudaBlockSize(SDF_BLOCK_SIZE, SDF_BLOCK_SIZE, SDF_BLOCK_SIZE);
		dim3 gridSize(
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().x) / cudaBlockSize.x)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().y) / cudaBlockSize.y)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().z) / cudaBlockSize.z))
		);

		dualVoxelTraversal_AllTrue_device<TBooleanFunctor, TVoxelPrimary, TVoxelSecondary>
				<< < gridSize, cudaBlockSize >> >
		                       (primaryVoxels, secondaryVoxels, arrayInfo, functor_device, falseEncountered_device);
		ORcudaKernelCheck;

		// transfer functor from VRAM back to RAM
		ORcudaSafeCall(cudaMemcpy(&functor, functor_device, sizeof(TBooleanFunctor), cudaMemcpyDeviceToHost));
		ORcudaSafeCall(cudaFree(functor_device));

		bool falseEncountered;
		ORcudaSafeCall(cudaMemcpy(&falseEncountered, falseEncountered_device, sizeof(bool), cudaMemcpyDeviceToHost));
		ORcudaSafeCall(cudaFree(falseEncountered_device));

		return !falseEncountered;
	}

	template<typename TFunctor>
	inline static void
	DualVoxelTraversal(
			ITMVoxelVolume<TVoxelPrimary, ITMPlainVoxelArray>* primaryScene,
			ITMVoxelVolume<TVoxelSecondary, ITMPlainVoxelArray>* secondaryScene,
			TFunctor& functor) {

		assert(primaryScene->index.getVolumeSize() == secondaryScene->index.getVolumeSize());

		// transfer functor from RAM to VRAM
		TFunctor* functor_device = nullptr;
		ORcudaSafeCall(cudaMalloc((void**) &functor_device, sizeof(TFunctor)));
		ORcudaSafeCall(cudaMemcpy(functor_device, &functor, sizeof(TFunctor), cudaMemcpyHostToDevice));

		// perform traversal on the GPU
		TVoxelPrimary* primaryVoxels = primaryScene->localVBA.GetVoxelBlocks();
		TVoxelSecondary* secondaryVoxels = secondaryScene->localVBA.GetVoxelBlocks();
		const ITMPlainVoxelArray::ITMVoxelArrayInfo* arrayInfo = primaryScene->index.getIndexData();
		dim3 cudaBlockSize(SDF_BLOCK_SIZE, SDF_BLOCK_SIZE, SDF_BLOCK_SIZE);
		dim3 gridSize(
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().x) / cudaBlockSize.x)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().y) / cudaBlockSize.y)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().z) / cudaBlockSize.z))
		);

		dualVoxelTraversal_device<TFunctor, TVoxelPrimary, TVoxelSecondary>
				<< < gridSize, cudaBlockSize >> >
		                       (primaryVoxels, secondaryVoxels, arrayInfo, functor_device);
		ORcudaKernelCheck;

		// transfer functor from VRAM back to RAM
		ORcudaSafeCall(cudaMemcpy(&functor, functor_device, sizeof(TFunctor), cudaMemcpyDeviceToHost));
		ORcudaSafeCall(cudaFree(functor_device));
	}

// endregion ===========================================================================================================
};


template<typename TVoxel, typename TWarp>
class ITMDualSceneWarpTraversalEngine<TVoxel, TWarp, ITMPlainVoxelArray, ITMLibSettings::DEVICE_CUDA> {
	/**
	 * \brief Concurrent traversal of 2 scenes with the same voxel type and a warp field
	 * \details All scenes must have matching dimensions
	 */
public:
// region ================================ STATIC TWO-SCENE TRAVERSAL WITH WARPS =======================================

	template<typename TStaticFunctor>
	inline static void
	StaticDualVoxelTraversal(
			ITMVoxelVolume<TVoxel, ITMPlainVoxelArray>* primaryScene,
			ITMVoxelVolume<TVoxel, ITMPlainVoxelArray>* secondaryScene,
			ITMVoxelVolume<TWarp, ITMPlainVoxelArray>* warpField) {
		assert(primaryScene->index.getVolumeSize() == secondaryScene->index.getVolumeSize() &&
		       primaryScene->index.getVolumeSize() == warpField->index.getVolumeSize());
// *** traversal vars
		TVoxel* secondaryVoxels = secondaryScene->localVBA.GetVoxelBlocks();
		TVoxel* primaryVoxels = primaryScene->localVBA.GetVoxelBlocks();
		TWarp* warpVoxels = warpField->localVBA.GetVoxelBlocks();

		const ITMPlainVoxelArray::ITMVoxelArrayInfo* arrayInfo = primaryScene->index.getIndexData();

		dim3 cudaBlockSize(SDF_BLOCK_SIZE, SDF_BLOCK_SIZE, SDF_BLOCK_SIZE);
		dim3 gridSize(
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().x) / cudaBlockSize.x)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().y) / cudaBlockSize.y)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().z) / cudaBlockSize.z))
		);

		staticDualVoxelWarpTraversal_device<TStaticFunctor, TVoxel>
				<< < gridSize, cudaBlockSize >> >
		                       (primaryVoxels, secondaryVoxels, warpVoxels, arrayInfo);
		ORcudaKernelCheck;
	}
// endregion
// region ================================ DYNAMIC TWO-SCENE TRAVERSAL WITH WARPS ======================================



	template<typename TFunctor>
	inline static void
	DualVoxelTraversal(
			ITMVoxelVolume<TVoxel, ITMPlainVoxelArray>* primaryScene,
			ITMVoxelVolume<TVoxel, ITMPlainVoxelArray>* secondaryScene,
			ITMVoxelVolume<TWarp, ITMPlainVoxelArray>* warpField,
			TFunctor& functor) {

		assert(primaryScene->index.getVolumeSize() == secondaryScene->index.getVolumeSize() &&
		       primaryScene->index.getVolumeSize() == warpField->index.getVolumeSize());
// *** traversal vars
		TVoxel* secondaryVoxels = secondaryScene->localVBA.GetVoxelBlocks();
		TVoxel* primaryVoxels = primaryScene->localVBA.GetVoxelBlocks();
		TWarp* warpVoxels = warpField->localVBA.GetVoxelBlocks();

		const ITMPlainVoxelArray::ITMVoxelArrayInfo* arrayInfo = primaryScene->index.getIndexData();

		dim3 cudaBlockSize(SDF_BLOCK_SIZE, SDF_BLOCK_SIZE, SDF_BLOCK_SIZE);
		dim3 gridSize(
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().x) / cudaBlockSize.x)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().y) / cudaBlockSize.y)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().z) / cudaBlockSize.z))
		);

		dualVoxelWarpTraversal_device<TFunctor, TVoxel>
				<< < gridSize, cudaBlockSize >> >
		                       (primaryVoxels, secondaryVoxels, warpVoxels, arrayInfo, functor);
		ORcudaKernelCheck;
	}


	template<typename TFunctor>
	inline static void
	DualVoxelPositionTraversal(
			ITMVoxelVolume<TVoxel, ITMPlainVoxelArray>* primaryScene,
			ITMVoxelVolume<TVoxel, ITMPlainVoxelArray>* secondaryScene,
			ITMVoxelVolume<TWarp, ITMPlainVoxelArray>* warpField,
			TFunctor& functor) {

		assert(primaryScene->index.getVolumeSize() == secondaryScene->index.getVolumeSize() &&
		       primaryScene->index.getVolumeSize() == warpField->index.getVolumeSize());

		// transfer functor from RAM to VRAM
		TFunctor* functor_device = nullptr;
		ORcudaSafeCall(cudaMalloc((void**) &functor_device, sizeof(TFunctor)));
		ORcudaSafeCall(cudaMemcpy(functor_device, &functor, sizeof(TFunctor), cudaMemcpyHostToDevice));

// *** traversal vars
		TVoxel* secondaryVoxels = secondaryScene->localVBA.GetVoxelBlocks();
		TVoxel* primaryVoxels = primaryScene->localVBA.GetVoxelBlocks();
		TWarp* warpVoxels = warpField->localVBA.GetVoxelBlocks();

		const ITMPlainVoxelArray::ITMVoxelArrayInfo* arrayInfo = primaryScene->index.getIndexData();

		dim3 cudaBlockSize(SDF_BLOCK_SIZE, SDF_BLOCK_SIZE, SDF_BLOCK_SIZE);
		dim3 gridSize(
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().x) / cudaBlockSize.x)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().y) / cudaBlockSize.y)),
				static_cast<int>(ceil(static_cast<float>(primaryScene->index.getVolumeSize().z) / cudaBlockSize.z))
		);

		dualVoxelWarpPositionTraversal_device<TFunctor, TVoxel>
				<< < gridSize, cudaBlockSize >> >
		                       (primaryVoxels, secondaryVoxels, warpVoxels, arrayInfo, functor_device);
		// transfer functor from VRAM back to RAM
		ORcudaSafeCall(cudaMemcpy(&functor, functor_device, sizeof(TFunctor), cudaMemcpyDeviceToHost));
		ORcudaSafeCall(cudaFree(functor_device));
		ORcudaKernelCheck;
	}
// endregion ===========================================================================================================
};

}// namespace ITMLib