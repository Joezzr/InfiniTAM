// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#include "ITMSceneReconstructionEngine_CPU.h"

#include "../Shared/ITMSceneReconstructionEngine_Shared.h"
#include "../../../Objects/RenderStates/ITMRenderState_VH.h"
using namespace ITMLib;
template<class TVoxel>
void ITMSceneReconstructionEngine_CPU<TVoxel,ITMVoxelBlockHash>::ResetScene(ITMVoxelVolume<TVoxel, ITMVoxelBlockHash> *scene)
{
	int numBlocks = scene->index.GetAllocatedBlockCount();
	int blockSize = scene->index.GetVoxelBlockSize();

	TVoxel *voxelBlocks_ptr = scene->localVBA.GetVoxelBlocks();
	for (int i = 0; i < numBlocks * blockSize; ++i) voxelBlocks_ptr[i] = TVoxel();
	int *vbaAllocationList_ptr = scene->localVBA.GetAllocationList();
	for (int i = 0; i < numBlocks; ++i) vbaAllocationList_ptr[i] = i;
	scene->localVBA.lastFreeBlockId = numBlocks - 1;

	ITMHashEntry tmpEntry;
	memset(&tmpEntry, 0, sizeof(ITMHashEntry));
	tmpEntry.ptr = -2;
	ITMHashEntry *hashEntry_ptr = scene->index.GetEntries();
	for (int i = 0; i < scene->index.hashEntryCount; ++i) hashEntry_ptr[i] = tmpEntry;
	int *excessList_ptr = scene->index.GetExcessAllocationList();
	for (int i = 0; i < scene->index.excessListSize; ++i) excessList_ptr[i] = i;

	scene->index.SetLastFreeExcessListId(scene->index.excessListSize - 1);
}

template<class TVoxel>
void ITMSceneReconstructionEngine_CPU<TVoxel, ITMVoxelBlockHash>::IntegrateIntoScene(ITMVoxelVolume<TVoxel, ITMVoxelBlockHash> *scene, const ITMView *view,
                                                                                     const ITMTrackingState *trackingState, const ITMRenderState *renderState)
{
	Vector2i rgbImgSize = view->rgb->noDims;
	Vector2i depthImgSize = view->depth->noDims;
	float voxelSize = scene->sceneParams->voxelSize;

	Matrix4f M_d, M_rgb;
	Vector4f projParams_d, projParams_rgb;

	ITMRenderState_VH *renderState_vh = (ITMRenderState_VH*)renderState;

	M_d = trackingState->pose_d->GetM();
	if (TVoxel::hasColorInformation) M_rgb = view->calib.trafo_rgb_to_depth.calib_inv * M_d;

	projParams_d = view->calib.intrinsics_d.projectionParamsSimple.all;
	projParams_rgb = view->calib.intrinsics_rgb.projectionParamsSimple.all;

	float mu = scene->sceneParams->mu; int maxW = scene->sceneParams->maxW;

	float *depth = view->depth->GetData(MEMORYDEVICE_CPU);
	float *confidence = view->depthConfidence->GetData(MEMORYDEVICE_CPU);
	Vector4u *rgb = view->rgb->GetData(MEMORYDEVICE_CPU);
	TVoxel *localVBA = scene->localVBA.GetVoxelBlocks();
	ITMHashEntry *hashTable = scene->index.GetEntries();

	int *visibleEntryIds = renderState_vh->GetVisibleEntryIDs();
	int noVisibleEntries = renderState_vh->noVisibleEntries;

	bool stopIntegratingAtMaxW = scene->sceneParams->stopIntegratingAtMaxW;

#ifdef WITH_OPENMP
	#pragma omp parallel for
#endif
	for (int visibleHash = 0; visibleHash < noVisibleEntries; visibleHash++)
	{
		Vector3i globalPos;
		int hash = visibleEntryIds[visibleHash];

		const ITMHashEntry &currentHashEntry = hashTable[hash];

		if (currentHashEntry.ptr < 0) continue;

		globalPos.x = currentHashEntry.pos.x;
		globalPos.y = currentHashEntry.pos.y;
		globalPos.z = currentHashEntry.pos.z;
		globalPos *= VOXEL_BLOCK_SIZE;

		TVoxel *localVoxelBlock = &(localVBA[currentHashEntry.ptr * (VOXEL_BLOCK_SIZE3)]);

		for (int z = 0; z < VOXEL_BLOCK_SIZE; z++) for (int y = 0; y < VOXEL_BLOCK_SIZE; y++) for (int x = 0; x < VOXEL_BLOCK_SIZE; x++)
		{
			Vector4f pt_model; int locId;

			locId = x + y * VOXEL_BLOCK_SIZE + z * VOXEL_BLOCK_SIZE * VOXEL_BLOCK_SIZE;

			if (stopIntegratingAtMaxW) if (localVoxelBlock[locId].w_depth == maxW) continue;

			pt_model.x = (float)(globalPos.x + x) * voxelSize;
			pt_model.y = (float)(globalPos.y + y) * voxelSize;
			pt_model.z = (float)(globalPos.z + z) * voxelSize;
			pt_model.w = 1.0f;

			ComputeUpdatedVoxelInfo<
					TVoxel::hasColorInformation,
					TVoxel::hasConfidenceInformation,
					TVoxel::hasSemanticInformation,
					TVoxel>::compute(localVoxelBlock[locId], pt_model, M_d,
				projParams_d, M_rgb, projParams_rgb, mu, maxW, depth, confidence, depthImgSize, rgb, rgbImgSize);
		}
	}
}

template<class TVoxel>
void ITMSceneReconstructionEngine_CPU<TVoxel, ITMVoxelBlockHash>::AllocateSceneFromDepth(ITMVoxelVolume<TVoxel, ITMVoxelBlockHash> *scene, const ITMView *view,
                                                                                         const ITMTrackingState *trackingState, const ITMRenderState *renderState, bool onlyUpdateVisibleList, bool resetVisibleList)
{
	Vector2i depthImgSize = view->depth->noDims;
	float voxelSize = scene->sceneParams->voxelSize;

	Matrix4f M_d, invM_d;
	Vector4f projParams_d, invProjParams_d;

	ITMRenderState_VH *renderState_vh = (ITMRenderState_VH*)renderState;
	if (resetVisibleList) renderState_vh->noVisibleEntries = 0;

	M_d = trackingState->pose_d->GetM(); M_d.inv(invM_d);

	projParams_d = view->calib.intrinsics_d.projectionParamsSimple.all;
	invProjParams_d = projParams_d;
	invProjParams_d.x = 1.0f / invProjParams_d.x;
	invProjParams_d.y = 1.0f / invProjParams_d.y;

	float mu = scene->sceneParams->mu;

	float *depth = view->depth->GetData(MEMORYDEVICE_CPU);
	int *voxelAllocationList = scene->localVBA.GetAllocationList();
	int *excessAllocationList = scene->index.GetExcessAllocationList();
	ITMHashEntry *hashTable = scene->index.GetEntries();
	ITMHashSwapState *swapStates = scene->Swapping() ? scene->globalCache->GetSwapStates(false) : 0;
	int *visibleEntryIDs = renderState_vh->GetVisibleEntryIDs();
	uchar *entriesVisibleType = renderState_vh->GetEntriesVisibleType();

	int hashEntryCount = scene->index.hashEntryCount;
	HashEntryState* hashEntryStates_device = scene->index.GetHashEntryStates();
	Vector3s* blockCoords_device = scene->index.GetAllocationBlockCoordinates();


	bool useSwapping = scene->Swapping();

	float oneOverHashEntrySize = 1.0f / (voxelSize * VOXEL_BLOCK_SIZE);//m

	int lastFreeVoxelBlockId = scene->localVBA.lastFreeBlockId;
	int lastFreeExcessListId = scene->index.GetLastFreeExcessListId();

	int noVisibleEntries = 0;

	scene->index.ClearHashEntryStates();

	for (int i = 0; i < renderState_vh->noVisibleEntries; i++)
		entriesVisibleType[visibleEntryIDs[i]] = 3; // visible at previous frame and unstreamed

	//build hashVisibility
#ifdef WITH_OPENMP
	#pragma omp parallel for
#endif
	for (int locId = 0; locId < depthImgSize.x*depthImgSize.y; locId++)
	{
		int y = locId / depthImgSize.x;
		int x = locId - y * depthImgSize.x;
		bool collisionDetected = false;
		buildHashAllocAndVisibleTypePP(hashEntryStates_device, entriesVisibleType, x, y, blockCoords_device, depth, invM_d,
		                               invProjParams_d, mu, depthImgSize, oneOverHashEntrySize,
		                               hashTable, scene->sceneParams->viewFrustum_min,
		                               scene->sceneParams->viewFrustum_max, collisionDetected);
	}

	if (onlyUpdateVisibleList) useSwapping = false;
	if (!onlyUpdateVisibleList)
	{
		//allocate
		for (int targetIdx = 0; targetIdx < hashEntryCount; targetIdx++)
		{
			int vbaIdx, exlIdx;
			unsigned char hashChangeType = hashEntryStates_device[targetIdx];

			switch (hashChangeType)
			{
			case 1: //needs allocation, fits in the ordered list
				vbaIdx = lastFreeVoxelBlockId; lastFreeVoxelBlockId--;

				if (vbaIdx >= 0) //there is room in the voxel block array
				{
					ITMHashEntry hashEntry;
					hashEntry.pos = blockCoords_device[targetIdx];
					hashEntry.ptr = voxelAllocationList[vbaIdx];
					hashEntry.offset = 0;

					hashTable[targetIdx] = hashEntry;
				}
				else
				{
					// Mark entry as not visible since we couldn't allocate it but buildHashAllocAndVisibleTypePP changed its state.
					entriesVisibleType[targetIdx] = 0;

					// Restore previous value to avoid leaks.
					lastFreeVoxelBlockId++;
				}

				break;
			case 2: //needs allocation in the excess list
				vbaIdx = lastFreeVoxelBlockId; lastFreeVoxelBlockId--;
				exlIdx = lastFreeExcessListId; lastFreeExcessListId--;

				if (vbaIdx >= 0 && exlIdx >= 0) //there is room in the voxel block array and excess list
				{
					ITMHashEntry hashEntry;
					hashEntry.pos = blockCoords_device[targetIdx];
					hashEntry.ptr = voxelAllocationList[vbaIdx];
					hashEntry.offset = 0;

					int exlOffset = excessAllocationList[exlIdx];

					hashTable[targetIdx].offset = exlOffset + 1; //connect to child

					hashTable[ORDERED_LIST_SIZE + exlOffset] = hashEntry; //add child to the excess list

					entriesVisibleType[ORDERED_LIST_SIZE + exlOffset] = 1; //make child visible and in memory
				}
				else
				{
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
	for (int targetIdx = 0; targetIdx < hashEntryCount; targetIdx++)
	{
		unsigned char hashVisibleType = entriesVisibleType[targetIdx];
		const ITMHashEntry &hashEntry = hashTable[targetIdx];

		if (hashVisibleType == 3)
		{
			bool isVisibleEnlarged, isVisible;

			if (useSwapping)
			{
				checkBlockVisibility<true>(isVisible, isVisibleEnlarged, hashEntry.pos, M_d, projParams_d, voxelSize, depthImgSize);
				if (!isVisibleEnlarged) hashVisibleType = 0;
			} else {
				checkBlockVisibility<false>(isVisible, isVisibleEnlarged, hashEntry.pos, M_d, projParams_d, voxelSize, depthImgSize);
				if (!isVisible) { hashVisibleType = 0; }
			}
			entriesVisibleType[targetIdx] = hashVisibleType;
		}

		if (useSwapping)
		{
			if (hashVisibleType > 0 && swapStates[targetIdx].state != 2) swapStates[targetIdx].state = 1;
		}

		if (hashVisibleType > 0)
		{
			visibleEntryIDs[noVisibleEntries] = targetIdx;
			noVisibleEntries++;
		}

#if 0
		// "active list", currently disabled
		if (hashVisibleType == 1)
		{
			activeEntryIDs[noActiveEntries] = targetIdx;
			noActiveEntries++;
		}
#endif
	}

	//reallocate deleted ones from previous swap operation
	if (useSwapping)
	{
		for (int targetIdx = 0; targetIdx < hashEntryCount; targetIdx++)
		{
			int vbaIdx;
			ITMHashEntry hashEntry = hashTable[targetIdx];

			if (entriesVisibleType[targetIdx] > 0 && hashEntry.ptr == -1)
			{
				vbaIdx = lastFreeVoxelBlockId; lastFreeVoxelBlockId--;
				if (vbaIdx >= 0) hashTable[targetIdx].ptr = voxelAllocationList[vbaIdx];
				else lastFreeVoxelBlockId++; // Avoid leaks
			}
		}
	}

	renderState_vh->noVisibleEntries = noVisibleEntries;

	scene->localVBA.lastFreeBlockId = lastFreeVoxelBlockId;
	scene->index.SetLastFreeExcessListId(lastFreeExcessListId);
}

template<class TVoxel>
ITMSceneReconstructionEngine_CPU<TVoxel,ITMPlainVoxelArray>::ITMSceneReconstructionEngine_CPU(void)
{}

template<class TVoxel>
ITMSceneReconstructionEngine_CPU<TVoxel,ITMPlainVoxelArray>::~ITMSceneReconstructionEngine_CPU(void)
{}

template<class TVoxel>
void ITMSceneReconstructionEngine_CPU<TVoxel,ITMPlainVoxelArray>::ResetScene(ITMVoxelVolume<TVoxel, ITMPlainVoxelArray> *scene)
{
	int numBlocks = scene->index.GetAllocatedBlockCount();
	int blockSize = scene->index.GetVoxelBlockSize();

	TVoxel *voxelBlocks_ptr = scene->localVBA.GetVoxelBlocks();
	for (int i = 0; i < numBlocks * blockSize; ++i) voxelBlocks_ptr[i] = TVoxel();
	int *vbaAllocationList_ptr = scene->localVBA.GetAllocationList();
	for (int i = 0; i < numBlocks; ++i) vbaAllocationList_ptr[i] = i;
	scene->localVBA.lastFreeBlockId = numBlocks - 1;
}

template<class TVoxel>
void ITMSceneReconstructionEngine_CPU<TVoxel, ITMPlainVoxelArray>::AllocateSceneFromDepth(ITMVoxelVolume<TVoxel, ITMPlainVoxelArray> *scene, const ITMView *view,
                                                                                          const ITMTrackingState *trackingState, const ITMRenderState *renderState, bool onlyUpdateVisibleList, bool resetVisibleList)
{}

template<class TVoxel>
void ITMSceneReconstructionEngine_CPU<TVoxel, ITMPlainVoxelArray>::IntegrateIntoScene(ITMVoxelVolume<TVoxel, ITMPlainVoxelArray> *scene, const ITMView *view,
                                                                                      const ITMTrackingState *trackingState, const ITMRenderState *renderState)
{
	Vector2i rgbImgSize = view->rgb->noDims;
	Vector2i depthImgSize = view->depth->noDims;
	float voxelSize = scene->sceneParams->voxelSize;

	Matrix4f M_d, M_rgb;
	Vector4f projParams_d, projParams_rgb;

	M_d = trackingState->pose_d->GetM();
	if (TVoxel::hasColorInformation) M_rgb = view->calib.trafo_rgb_to_depth.calib_inv * M_d;

	projParams_d = view->calib.intrinsics_d.projectionParamsSimple.all;
	projParams_rgb = view->calib.intrinsics_rgb.projectionParamsSimple.all;

	float mu = scene->sceneParams->mu; int maxW = scene->sceneParams->maxW;

	float *depth = view->depth->GetData(MEMORYDEVICE_CPU);
	float *confidence = view->depthConfidence->GetData(MEMORYDEVICE_CPU);
	Vector4u *rgb = view->rgb->GetData(MEMORYDEVICE_CPU);
	TVoxel *voxelArray = scene->localVBA.GetVoxelBlocks();

	const ITMPlainVoxelArray::IndexData *arrayInfo = scene->index.GetIndexData();

	bool stopIntegratingAtMaxW = scene->sceneParams->stopIntegratingAtMaxW;
	//bool approximateIntegration = !trackingState->requiresFullRendering;

#ifdef WITH_OPENMP
	#pragma omp parallel for
#endif
	for (int locId = 0; locId < scene->index.GetVolumeSize().x * scene->index.GetVolumeSize().y *
	                            scene->index.GetVolumeSize().z; ++locId)
	{
		int z = locId / (scene->index.GetVolumeSize().x * scene->index.GetVolumeSize().y);
		int tmp = locId - z * scene->index.GetVolumeSize().x * scene->index.GetVolumeSize().y;
		int y = tmp / scene->index.GetVolumeSize().x;
		int x = tmp - y * scene->index.GetVolumeSize().x;

		Vector4f pt_model;

		if (stopIntegratingAtMaxW) if (voxelArray[locId].w_depth == maxW) continue;
		//if (approximateIntegration) if (voxelArray[locId].w_depth != 0) continue;

		pt_model.x = (float)(x + arrayInfo->offset.x) * voxelSize;
		pt_model.y = (float)(y + arrayInfo->offset.y) * voxelSize;
		pt_model.z = (float)(z + arrayInfo->offset.z) * voxelSize;
		pt_model.w = 1.0f;

		ComputeUpdatedVoxelInfo<
				TVoxel::hasColorInformation,
				TVoxel::hasConfidenceInformation,
				TVoxel::hasSemanticInformation,
				TVoxel>::compute(voxelArray[locId], pt_model, M_d,
		                         projParams_d, M_rgb, projParams_rgb, mu, maxW, depth, confidence, depthImgSize, rgb, rgbImgSize);
	}
}
