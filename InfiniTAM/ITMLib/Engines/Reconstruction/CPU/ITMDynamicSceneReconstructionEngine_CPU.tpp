// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#include "ITMDynamicSceneReconstructionEngine_CPU.h"

#include "../Shared/ITMDynamicSceneReconstructionEngine_Shared.h"
#include "../../../Objects/RenderStates/ITMRenderState_VH.h"

using namespace ITMLib;


// region ============================================= VOXEL BLOCK HASH ===============================================

template<typename TVoxelCanonical, typename TVoxelLive>
ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::ITMDynamicSceneReconstructionEngine_CPU(
		void) {
	size_t noTotalEntries = ITMVoxelBlockHash::noTotalEntries;
	entriesAllocType = new ORUtils::MemoryBlock<unsigned char>(noTotalEntries, MEMORYDEVICE_CPU);
	blockCoords = new ORUtils::MemoryBlock<Vector4s>(noTotalEntries, MEMORYDEVICE_CPU);
}

template<typename TVoxelCanonical, typename TVoxelLive>
ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::~ITMDynamicSceneReconstructionEngine_CPU(
		void) {
	delete entriesAllocType;
	delete blockCoords;
}


template<typename TVoxelCanonical, typename TVoxelLive>
template<typename TVoxel>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::ResetScene(
		ITMScene<TVoxel, ITMVoxelBlockHash>* scene) {
	int numBlocks = scene->index.getNumAllocatedVoxelBlocks();
	int blockSize = scene->index.getVoxelBlockSize();

	//TODO: that's kind of slow, copy whole pre-filled memory blocks instead -Greg (GitHub: Algomorph)
	TVoxel* voxels = scene->localVBA.GetVoxelBlocks();
	for (int i = 0; i < numBlocks * blockSize; ++i) voxels[i] = TVoxel();
	int* vbaAllocationList_ptr = scene->localVBA.GetAllocationList();
	for (int i = 0; i < numBlocks; ++i) vbaAllocationList_ptr[i] = i;
	scene->localVBA.lastFreeBlockId = numBlocks - 1;

	ITMHashEntry tmpEntry;
	memset(&tmpEntry, 0, sizeof(ITMHashEntry));
	tmpEntry.ptr = -2;
	ITMHashEntry* hashEntry_ptr = scene->index.GetEntries();
	for (int i = 0; i < scene->index.noTotalEntries; ++i) hashEntry_ptr[i] = tmpEntry;
	int* excessList_ptr = scene->index.GetExcessAllocationList();
	for (int i = 0; i < SDF_EXCESS_LIST_SIZE; ++i) excessList_ptr[i] = i;

	scene->index.SetLastFreeExcessListId(SDF_EXCESS_LIST_SIZE - 1);
}

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::ResetCanonicalScene(
		ITMScene<TVoxelCanonical, ITMVoxelBlockHash>* scene) {
	ResetScene(scene);
}

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::ResetLiveScene(
		ITMScene<TVoxelLive, ITMVoxelBlockHash>* scene) {
	ResetScene(scene);
}

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::IntegrateIntoScene(
		ITMScene<TVoxelLive, ITMVoxelBlockHash>* scene, const ITMView* view,
		const ITMTrackingState* trackingState, const ITMRenderState* renderState) {
	Vector2i rgbImgSize = view->rgb->noDims;
	Vector2i depthImgSize = view->depth->noDims;
	float voxelSize = scene->sceneParams->voxelSize;

	Matrix4f M_d, M_rgb;
	Vector4f projParams_d, projParams_rgb;

	ITMRenderState_VH* renderState_vh = (ITMRenderState_VH*) renderState;

	M_d = trackingState->pose_d->GetM();
	if (TVoxelLive::hasColorInformation) M_rgb = view->calib.trafo_rgb_to_depth.calib_inv * M_d;

	projParams_d = view->calib.intrinsics_d.projectionParamsSimple.all;
	projParams_rgb = view->calib.intrinsics_rgb.projectionParamsSimple.all;

	float mu = scene->sceneParams->mu;
	int maxW = scene->sceneParams->maxW;

	float* depth = view->depth->GetData(MEMORYDEVICE_CPU);
	float* confidence = view->depthConfidence->GetData(MEMORYDEVICE_CPU);
	Vector4u* rgb = view->rgb->GetData(MEMORYDEVICE_CPU);
	TVoxelLive* localVBA = scene->localVBA.GetVoxelBlocks();
	ITMHashEntry* hashTable = scene->index.GetEntries();

	int* visibleEntryIds = renderState_vh->GetVisibleEntryIDs();
	int noVisibleEntries = renderState_vh->noVisibleEntries;

	bool stopIntegratingAtMaxW = scene->sceneParams->stopIntegratingAtMaxW;
	//bool approximateIntegration = !trackingState->requiresFullRendering;

	//_DEBUG
//	std::vector<Vector3i> _DEBUG_positions = {
//			Vector3i(1, 59, 217 ),
//			Vector3i(1, 59, 218 ),
//			Vector3i(1, 58, 213 ),
//			Vector3i(1, 58, 214 ),
//			Vector3i(1, 60, 220 ),
//			Vector3i(1, 60, 221 ),
//			Vector3i(1, 61, 224 ),
//			Vector3i(1, 61, 225 ),
//			Vector3i(-23, 62, 214 ),
//			Vector3i(-23, 62, 215 )};

#ifdef WITH_OPENMP
#pragma omp parallel for
#endif
	for (int visibleHash = 0; visibleHash < noVisibleEntries; visibleHash++) {
		Vector3i globalPos;
		int hash = visibleEntryIds[visibleHash];
		//_DEBUG
//		if(hash == 260246){
//			int i = 42;
//		}
		const ITMHashEntry& currentHashEntry = hashTable[hash];

		if (currentHashEntry.ptr < 0) continue;

		globalPos.x = currentHashEntry.pos.x;
		globalPos.y = currentHashEntry.pos.y;
		globalPos.z = currentHashEntry.pos.z;
		globalPos *= SDF_BLOCK_SIZE;

		TVoxelLive* localVoxelBlock = &(localVBA[currentHashEntry.ptr * (SDF_BLOCK_SIZE3)]);

		for (int z = 0; z < SDF_BLOCK_SIZE; z++)
			for (int y = 0; y < SDF_BLOCK_SIZE; y++)
				for (int x = 0; x < SDF_BLOCK_SIZE; x++) {
					Vector4f pt_model;
					int locId;

					locId = x + y * SDF_BLOCK_SIZE + z * SDF_BLOCK_SIZE * SDF_BLOCK_SIZE;

					pt_model.x = (float) (globalPos.x + x) * voxelSize;
					pt_model.y = (float) (globalPos.y + y) * voxelSize;
					pt_model.z = (float) (globalPos.z + z) * voxelSize;
					pt_model.w = 1.0f;
					//_DEBUG
//			Vector3i voxelPos = globalPos + Vector3i(x,y,z);
//			if(std::find(_DEBUG_positions.begin(),_DEBUG_positions.end(), voxelPos) != _DEBUG_positions.end()){
//				Vector4f pt_camera; Vector2f pt_image;
//				float depth_measure;
//				pt_camera = M_d * pt_model;
//				pt_image.x = projParams_d.x * pt_camera.x / pt_camera.z + projParams_d.z;
//				pt_image.y = projParams_d.y * pt_camera.y / pt_camera.z + projParams_d.w;
//				Vector2i pt_image_int = Vector2i((int)(pt_image.x + 0.5f), (int)(pt_image.y + 0.5f));
//				depth_measure = depth[(int)(pt_image.x + 0.5f) + (int)(pt_image.y + 0.5f) * depthImgSize.x];
//				std::cout << "Processed voxel at " << voxelPos  << ". Image sampled at: " << pt_image_int << ". Depth (m): " << depth_measure << ". Image float coord: " << pt_image << std::endl;
//			}
					ComputeUpdatedLiveVoxelInfo<
							TVoxelLive::hasColorInformation,
							TVoxelLive::hasConfidenceInformation,
							TVoxelLive::hasSemanticInformation,
							TVoxelLive>::compute(localVoxelBlock[locId], pt_model, M_d,
					                             projParams_d, M_rgb, projParams_rgb, mu, maxW, depth, confidence,
					                             depthImgSize, rgb, rgbImgSize);
				}
	}
}

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMVoxelBlockHash>::AllocateSceneFromDepth(
		ITMScene<TVoxelLive, ITMVoxelBlockHash>* scene, const ITMView* view,
		const ITMTrackingState* trackingState, const ITMRenderState* renderState, bool onlyUpdateVisibleList,
		bool resetVisibleList) {
	Vector2i depthImgSize = view->depth->noDims;
	float voxelSize = scene->sceneParams->voxelSize;

	Matrix4f M_d, invM_d;
	Vector4f projParams_d, invProjParams_d;

	ITMRenderState_VH* renderState_vh = (ITMRenderState_VH*) renderState;
	if (resetVisibleList) renderState_vh->noVisibleEntries = 0;

	M_d = trackingState->pose_d->GetM();
	M_d.inv(invM_d);

	projParams_d = view->calib.intrinsics_d.projectionParamsSimple.all;
	invProjParams_d = projParams_d;
	invProjParams_d.x = 1.0f / invProjParams_d.x;
	invProjParams_d.y = 1.0f / invProjParams_d.y;

	float mu = scene->sceneParams->mu;

	float* depth = view->depth->GetData(MEMORYDEVICE_CPU);
	int* voxelAllocationList = scene->localVBA.GetAllocationList();
	int* excessAllocationList = scene->index.GetExcessAllocationList();
	ITMHashEntry* hashTable = scene->index.GetEntries();
	ITMHashSwapState* swapStates = scene->globalCache != nullptr ? scene->globalCache->GetSwapStates(false) : 0;
	int* visibleEntryIDs = renderState_vh->GetVisibleEntryIDs();
	uchar* entriesVisibleType = renderState_vh->GetEntriesVisibleType();
	uchar* entriesAllocType = this->entriesAllocType->GetData(MEMORYDEVICE_CPU);
	Vector4s* blockCoords = this->blockCoords->GetData(MEMORYDEVICE_CPU);
	int noTotalEntries = scene->index.noTotalEntries;

	bool useSwapping = scene->globalCache != nullptr;

	float oneOverHashEntrySize = 1.0f / (voxelSize * SDF_BLOCK_SIZE);//m

	int lastFreeVoxelBlockId = scene->localVBA.lastFreeBlockId;
	int lastFreeExcessListId = scene->index.GetLastFreeExcessListId();

	int noVisibleEntries = 0;

	memset(entriesAllocType, 0, noTotalEntries);

	for (int i = 0; i < renderState_vh->noVisibleEntries; i++)
		entriesVisibleType[visibleEntryIDs[i]] = 3; // visible at previous frame and unstreamed

	//build hashVisibility
#ifdef WITH_OPENMP
#pragma omp parallel for
#endif
	for (int locId = 0; locId < depthImgSize.x * depthImgSize.y; locId++) {
		int y = locId / depthImgSize.x;
		int x = locId - y * depthImgSize.x;
		buildLiveHashAllocAndVisibleTypePP(entriesAllocType, entriesVisibleType, x, y, blockCoords, depth, invM_d,
		                               invProjParams_d, mu, depthImgSize, oneOverHashEntrySize, hashTable,
		                               scene->sceneParams->viewFrustum_min,
		                               scene->sceneParams->viewFrustum_max);
	}

	if (onlyUpdateVisibleList) useSwapping = false;
	if (!onlyUpdateVisibleList) {
		//allocate
		for (int targetIdx = 0; targetIdx < noTotalEntries; targetIdx++) {
			int vbaIdx, exlIdx;
			unsigned char hashChangeType = entriesAllocType[targetIdx];

			switch (hashChangeType) {
				case 1: //needs allocation, fits in the ordered list
					vbaIdx = lastFreeVoxelBlockId;
					lastFreeVoxelBlockId--;

					if (vbaIdx >= 0) { //there is room in the voxel block array
						Vector4s pt_block_all = blockCoords[targetIdx];

						ITMHashEntry hashEntry;
						hashEntry.pos.x = pt_block_all.x;
						hashEntry.pos.y = pt_block_all.y;
						hashEntry.pos.z = pt_block_all.z;
						hashEntry.ptr = voxelAllocationList[vbaIdx];
						hashEntry.offset = 0;

						hashTable[targetIdx] = hashEntry;
					} else {
						// Mark entry as not visible since we couldn't allocate it but buildHashAllocAndVisibleTypePP changed its state.
						entriesVisibleType[targetIdx] = 0;

						// Restore previous value to avoid leaks.
						lastFreeVoxelBlockId++;
					}

					break;
				case 2: //needs allocation in the excess list
					vbaIdx = lastFreeVoxelBlockId;
					lastFreeVoxelBlockId--;
					exlIdx = lastFreeExcessListId;
					lastFreeExcessListId--;

					if (vbaIdx >= 0 && exlIdx >= 0) { //there is room in the voxel block array and excess list
						Vector4s pt_block_all = blockCoords[targetIdx];

						ITMHashEntry hashEntry;
						hashEntry.pos.x = pt_block_all.x;
						hashEntry.pos.y = pt_block_all.y;
						hashEntry.pos.z = pt_block_all.z;
						hashEntry.ptr = voxelAllocationList[vbaIdx];
						hashEntry.offset = 0;

						int exlOffset = excessAllocationList[exlIdx];

						hashTable[targetIdx].offset = exlOffset + 1; //connect to child

						hashTable[SDF_BUCKET_NUM + exlOffset] = hashEntry; //add child to the excess list

						entriesVisibleType[SDF_BUCKET_NUM + exlOffset] = 1; //make child visible and in memory
					} else {
						// No need to mark the entry as not visible since buildHashAllocAndVisibleTypePP did not mark it.
						// Restore previous value to avoid leaks.
						lastFreeVoxelBlockId++;
						lastFreeExcessListId++;
					}

					break;
			}
		}
	}

	//build visible list
	for (int targetIdx = 0; targetIdx < noTotalEntries; targetIdx++) {
		unsigned char hashVisibleType = entriesVisibleType[targetIdx];
		const ITMHashEntry& hashEntry = hashTable[targetIdx];

		if (hashVisibleType == 3) {
			bool isVisibleEnlarged, isVisible;

			if (useSwapping) {
				checkLiveBlockVisibility<true>(isVisible, isVisibleEnlarged, hashEntry.pos, M_d, projParams_d, voxelSize,
				                           depthImgSize);
				if (!isVisibleEnlarged) hashVisibleType = 0;
			} else {
				checkLiveBlockVisibility<false>(isVisible, isVisibleEnlarged, hashEntry.pos, M_d, projParams_d, voxelSize,
				                            depthImgSize);
				if (!isVisible) { hashVisibleType = 0; }
			}
			entriesVisibleType[targetIdx] = hashVisibleType;
		}

		if (useSwapping) {
			if (hashVisibleType > 0 && swapStates[targetIdx].state != 2) swapStates[targetIdx].state = 1;
		}

		if (hashVisibleType > 0) {
			visibleEntryIDs[noVisibleEntries] = targetIdx;
			noVisibleEntries++;
		}

	}

	//reallocate deleted ones from previous swap operation
	if (useSwapping) {
		for (int targetIdx = 0; targetIdx < noTotalEntries; targetIdx++) {
			int vbaIdx;
			ITMHashEntry hashEntry = hashTable[targetIdx];

			if (entriesVisibleType[targetIdx] > 0 && hashEntry.ptr == -1) {
				vbaIdx = lastFreeVoxelBlockId;
				lastFreeVoxelBlockId--;
				if (vbaIdx >= 0) hashTable[targetIdx].ptr = voxelAllocationList[vbaIdx];
				else lastFreeVoxelBlockId++; // Avoid leaks
			}
		}
	}

	renderState_vh->noVisibleEntries = noVisibleEntries;

	scene->localVBA.lastFreeBlockId = lastFreeVoxelBlockId;
	scene->index.SetLastFreeExcessListId(lastFreeExcessListId);
}

// endregion ===========================================================================================================
// region ========================================= PLAIN VOXEL ARRAY ==================================================

template<typename TVoxelCanonical, typename TVoxelLive>
ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMPlainVoxelArray>::ITMDynamicSceneReconstructionEngine_CPU(
		void) {}

template<typename TVoxelCanonical, typename TVoxelLive>
ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMPlainVoxelArray>::~ITMDynamicSceneReconstructionEngine_CPU(
		void) {}


template<typename TVoxelCanonical, typename TVoxelLive>
template<typename TVoxel>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMPlainVoxelArray>::ResetScene(
		ITMScene<TVoxel, ITMPlainVoxelArray>* scene) {
	int numBlocks = scene->index.getNumAllocatedVoxelBlocks();
	int blockSize = scene->index.getVoxelBlockSize();

	TVoxelCanonical* voxelBlocks_ptr = scene->localVBA.GetVoxelBlocks();
	for (int i = 0; i < numBlocks * blockSize; ++i) voxelBlocks_ptr[i] = TVoxelCanonical();
	int* vbaAllocationList_ptr = scene->localVBA.GetAllocationList();
	for (int i = 0; i < numBlocks; ++i) vbaAllocationList_ptr[i] = i;
	scene->localVBA.lastFreeBlockId = numBlocks - 1;
}

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMPlainVoxelArray>::ResetCanonicalScene(
		ITMScene<TVoxelCanonical, ITMPlainVoxelArray>* scene) {
	ResetScene(scene);
}

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMPlainVoxelArray>::ResetLiveScene(
		ITMScene<TVoxelLive, ITMPlainVoxelArray>* scene) {
	ResetScene(scene);
}

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMPlainVoxelArray>::AllocateSceneFromDepth(
		ITMScene<TVoxelLive, ITMPlainVoxelArray>* scene, const ITMView* view,
		const ITMTrackingState* trackingState, const ITMRenderState* renderState, bool onlyUpdateVisibleList,
		bool resetVisibleList) {}

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMPlainVoxelArray>::IntegrateIntoScene(
		ITMScene<TVoxelLive, ITMPlainVoxelArray>* scene, const ITMView* view,
		const ITMTrackingState* trackingState, const ITMRenderState* renderState) {
	Vector2i rgbImgSize = view->rgb->noDims;
	Vector2i depthImgSize = view->depth->noDims;
	float voxelSize = scene->sceneParams->voxelSize;

	Matrix4f M_d, M_rgb;
	Vector4f projParams_d, projParams_rgb;

	M_d = trackingState->pose_d->GetM();
	if (TVoxelLive::hasColorInformation) M_rgb = view->calib.trafo_rgb_to_depth.calib_inv * M_d;

	projParams_d = view->calib.intrinsics_d.projectionParamsSimple.all;
	projParams_rgb = view->calib.intrinsics_rgb.projectionParamsSimple.all;

	float mu = scene->sceneParams->mu;
	int maxW = scene->sceneParams->maxW;

	float* depth = view->depth->GetData(MEMORYDEVICE_CPU);
	Vector4u* rgb = view->rgb->GetData(MEMORYDEVICE_CPU);
	TVoxelLive* voxelArray = scene->localVBA.GetVoxelBlocks();

	const ITMPlainVoxelArray::IndexData* arrayInfo = scene->index.getIndexData();

	bool stopIntegratingAtMaxW = scene->sceneParams->stopIntegratingAtMaxW;

#ifdef WITH_OPENMP
#pragma omp parallel for
#endif
	for (int locId = 0; locId < scene->index.getVolumeSize().x * scene->index.getVolumeSize().y *
	                            scene->index.getVolumeSize().z; ++locId) {
		int z = locId / (scene->index.getVolumeSize().x * scene->index.getVolumeSize().y);
		int tmp = locId - z * scene->index.getVolumeSize().x * scene->index.getVolumeSize().y;
		int y = tmp / scene->index.getVolumeSize().x;
		int x = tmp - y * scene->index.getVolumeSize().x;

		Vector4f pt_model;

		if (stopIntegratingAtMaxW) if (voxelArray[locId].w_depth == maxW) continue;

		pt_model.x = (float) (x + arrayInfo->offset.x) * voxelSize;
		pt_model.y = (float) (y + arrayInfo->offset.y) * voxelSize;
		pt_model.z = (float) (z + arrayInfo->offset.z) * voxelSize;
		pt_model.w = 1.0f;

		ComputeUpdatedLiveVoxelInfo<
				TVoxelLive::hasColorInformation,
				TVoxelLive::hasConfidenceInformation,
				TVoxelLive::hasSemanticInformation,
				TVoxelLive>::compute(voxelArray[locId], pt_model, M_d, projParams_d, M_rgb, projParams_rgb, mu, maxW,
		                             depth, depthImgSize, rgb, rgbImgSize);
	}
}


// endregion ===========================================================================================================