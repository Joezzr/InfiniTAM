//  ================================================================
//  Created by Gregory Kramida on 5/25/18.
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

#include "../../../../ORUtils/PlatformIndependence.h"
#include "../../../Objects/Scene/ITMVoxelBlockHash.h"
#include "../../../Objects/Scene/ITMRepresentationAccess.h"
#include "../../Common/ITMCommonFunctors.h"
#include "../../../Utils/ITMMath.h"
#include "../../../Utils/ITMPixelUtils.h"
#include "../../../Utils/ITMVoxelFlags.h"
#include "../../../Utils/ITMHashBlockProperties.h"
#include "../../../Utils/Geometry/ITMIntersectionChecks.h"

#ifdef __CUDACC__
#include "../../../Utils/ITMCUDAUtils.h"
#include "../../Traversal/CUDA/ITMSceneTraversal_CUDA_VoxelBlockHash.h"
#endif

using namespace ITMLib;


struct AllocationTempData {
	int noAllocatedVoxelEntries;
	int noAllocatedExcessEntries;
	int noVisibleEntries;
};



/**
 * \brief Determines whether the hash block at the specified block position needs it's voxels to be allocated, as well
 * as whether they should be allocated in the excess list or the ordered list of the hash table.
 * If any of these are true, marks the corresponding entry in \param hashEntryStates
 * \param[in,out] hashEntryStates  array where to set the allocation type at final hashIdx index
 * \param[in,out] hashBlockCoordinates  array block coordinates for the new hash blocks at final hashIdx index
 * \param[in,out] hashIdx  takes in original index assuming coords, i.e. \refitem hashIndex(\param desiredHashBlockPosition),
 * returns final index of the hash block to be allocated (may be updated based on hash closed chaining)
 * \param[in] desiredHashBlockPosition  position of the hash block to check / allocate
 * \param[in] hashTable  hash table with existing blocks
 * \param[in] collisionDetected set to true if a block with the same hashcode has already been marked for allocation ( a collision occured )
 * \return true if the block needs allocation, false otherwise
 */
_CPU_AND_GPU_CODE_
inline bool MarkAsNeedingAllocationIfNotFound(ITMLib::HashEntryState* hashEntryStates,
                                              Vector3s* hashBlockCoordinates, int& hashIdx,
                                              const CONSTPTR(Vector3s)& desiredHashBlockPosition,
                                              const CONSTPTR(ITMHashEntry)* hashTable, bool& collisionDetected) {

	ITMHashEntry hashEntry = hashTable[hashIdx];
	//check if hash table contains entry

	if (!(IS_EQUAL3(hashEntry.pos, desiredHashBlockPosition) && hashEntry.ptr >= -1)) {

		auto setHashEntryState = [&](HashEntryState state){
#if defined(__CUDACC__) && defined(__CUDA_ARCH__)
			if (atomicCAS((char*) hashEntryStates + hashIdx,
		              (char) ITMLib::NEEDS_NO_CHANGE,
		              (char) state) != ITMLib::NEEDS_NO_CHANGE){
			//hash code already marked for allocation, but at different coordinates, cannot allocate
			collisionDetected = true;
			return false;
		} else {
			hashBlockCoordinates[hashIdx] = desiredHashBlockPosition;
			return true;
		}
#else
			//TODO: come up with an atomics-based solution for OpenMP
#ifdef WITH_OPENMP
#pragma omp critical
#endif
			{
				//single-threaded version
				if (hashEntryStates[hashIdx] != ITMLib::NEEDS_NO_CHANGE
					/*&& !IS_EQUAL3(hashBlockCoordinates[hashIdx], desiredHashBlockPosition)*/) {
					//hash code already marked for allocation, but at different coordinates, cannot allocate
					collisionDetected = true;
					return false;
				} else {
					hashEntryStates[hashIdx] = state;
					hashBlockCoordinates[hashIdx] = desiredHashBlockPosition;
					return true;
				}
			}
#endif
		};
		if (hashEntry.ptr >= -1) {
			//search excess list only if there is no room in ordered part
			while (hashEntry.offset >= 1) {
				hashIdx = ORDERED_LIST_SIZE + hashEntry.offset - 1;
				hashEntry = hashTable[hashIdx];

				if (IS_EQUAL3(hashEntry.pos, desiredHashBlockPosition) && hashEntry.ptr >= -1) {
					return false;
				}
			}
			return setHashEntryState(ITMLib::NEEDS_ALLOCATION_IN_EXCESS_LIST);
		}
		return setHashEntryState(ITMLib::NEEDS_ALLOCATION_IN_ORDERED_LIST);
	}
	// already have hash block, no allocation needed
	return false;

};


template<typename TWarp, typename TVoxel, WarpType TWarpType>
struct WarpBasedAllocationMarkerFunctor {
	WarpBasedAllocationMarkerFunctor(
			ITMVoxelVolume<TVoxel, ITMVoxelBlockHash>* sourceVolume,
			ITMVoxelVolume<TVoxel, ITMVoxelBlockHash>* volumeToAllocate,
			Vector3s* allocationBlockCoords,
			HashEntryState* warpedEntryAllocationStates) :

			collisionDetected(false),

			targetTSDFScene(volumeToAllocate),
			targetTSDFVoxels(volumeToAllocate->localVBA.GetVoxelBlocks()),
			targetTSDFHashEntries(volumeToAllocate->index.GetEntries()),
			targetTSDFCache(),

			sourceTSDFScene(sourceVolume),
			sourceTSDFVoxels(sourceVolume->localVBA.GetVoxelBlocks()),
			sourceTSDFHashEntries(sourceVolume->index.GetEntries()),
			sourceTSDFCache(),

			allocationBlockCoords(allocationBlockCoords),
			warpedEntryAllocationStates(warpedEntryAllocationStates) {}

	_CPU_AND_GPU_CODE_
	inline
	void operator()(TWarp& warpVoxel, Vector3i voxelPosition, Vector3s hashBlockPosition) {

		Vector3f warpVector = ITMLib::WarpVoxelStaticFunctor<TWarp, TWarpType>::GetWarp(warpVoxel);
		Vector3f warpedPosition = warpVector + TO_FLOAT3(voxelPosition);
		Vector3i warpedPositionTruncated = warpedPosition.toInt();

		// perform lookup in source volume
		int vmIndex;
#if !defined(__CUDACC__) && !defined(WITH_OPENMP)
		const TVoxel& sourceTSDFVoxelAtWarp = readVoxel(sourceTSDFVoxels, sourceTSDFHashEntries,
		                                                warpedPositionTruncated,
		                                                vmIndex, sourceTSDFCache);
#else //don't use cache when multithreading!
		const TVoxel& sourceTSDFVoxelAtWarp = readVoxel(sourceTSDFVoxels, sourceTSDFHashEntries,
														warpedPositionTruncated,
														vmIndex);
#endif

		int targetBlockHash = hashIndex(hashBlockPosition);

		MarkAsNeedingAllocationIfNotFound(warpedEntryAllocationStates, allocationBlockCoords, targetBlockHash,
		                                  hashBlockPosition, targetTSDFHashEntries, collisionDetected);
	}

	bool collisionDetected;

private:


	ITMVoxelVolume<TVoxel, ITMVoxelBlockHash>* targetTSDFScene;
	TVoxel* targetTSDFVoxels;
	ITMHashEntry* targetTSDFHashEntries;
	ITMVoxelBlockHash::IndexCache targetTSDFCache;

	ITMVoxelVolume<TVoxel, ITMVoxelBlockHash>* sourceTSDFScene;
	TVoxel* sourceTSDFVoxels;
	ITMHashEntry* sourceTSDFHashEntries;
	ITMVoxelBlockHash::IndexCache sourceTSDFCache;

	Vector3s* allocationBlockCoords;
	HashEntryState* warpedEntryAllocationStates;
};


_CPU_AND_GPU_CODE_ inline void
markForAllocationAndSetVisibilityTypeIfNotFound(Vector3s hashBlockPosition, const CONSTPTR(ITMHashEntry)* hashTable,
                                                ITMLib::HashEntryState* hashEntryStates, uchar* entriesVisibleType,
                                                Vector3s* blockCoords, bool& collisionDetected) {

	int hashIdx;
	//compute index in hash table
	hashIdx = hashIndex(hashBlockPosition);

	//check if hash table contains entry
	bool isFound = false;

	ITMHashEntry hashEntry = hashTable[hashIdx];

	if (IS_EQUAL3(hashEntry.pos, hashBlockPosition) && hashEntry.ptr >= -1) {
		//entry has been streamed out but is visible or in memory and visible
		entriesVisibleType[hashIdx] = (hashEntry.ptr == -1) ? (uchar) 2 : (uchar) 1;

		isFound = true;
	}

	if (!isFound) {
		bool isExcess = false;
		if (hashEntry.ptr >= -1) //search excess list only if there is no room in ordered part
		{
			while (hashEntry.offset >= 1) {
				hashIdx = ORDERED_LIST_SIZE + hashEntry.offset - 1;
				hashEntry = hashTable[hashIdx];

				if (IS_EQUAL3(hashEntry.pos, hashBlockPosition) && hashEntry.ptr >= -1) {
					//entry has been streamed out but is visible or in memory and visible
					entriesVisibleType[hashIdx] = (hashEntry.ptr == -1) ? (uchar) 2 : (uchar) 1;

					isFound = true;
					break;
				}
			}

			isExcess = true;
		}

		if (!isFound) //still not found
		{
			if (hashEntryStates[hashIdx] != ITMLib::NEEDS_NO_CHANGE) {
				collisionDetected = true;
			} else {
				//needs allocation
				hashEntryStates[hashIdx] = isExcess ?
				                           ITMLib::NEEDS_ALLOCATION_IN_EXCESS_LIST
				                                    : ITMLib::NEEDS_ALLOCATION_IN_ORDERED_LIST;
				if (!isExcess) entriesVisibleType[hashIdx] = 1; //new entry is visible
				blockCoords[hashIdx] = hashBlockPosition;
			}
		}
	}
}

_CPU_AND_GPU_CODE_ inline int getIncrementCount(Vector3s coord1, Vector3s coord2) {
	return static_cast<int>(coord1.x != coord2.x) +
	       static_cast<int>(coord1.y != coord2.y) +
	       static_cast<int>(coord1.z != coord2.z);
}

_CPU_AND_GPU_CODE_ inline void
buildHashAllocAndVisibleTypePP(ITMLib::HashEntryState* hashEntryStates, uchar* entriesVisibleType, int x, int y,
                               Vector3s* blockCoords,
                               const CONSTPTR(float)* depth, Matrix4f invertedCameraPose,
                               Vector4f invertedCameraProjectionParameters, float mu,
                               Vector2i imgSize, float oneOverVoxelBlockSize_Meters,
                               const CONSTPTR(ITMHashEntry)* hashTable,
                               float viewFrustum_min, float viewFrustum_max, bool& collisionDetected,
                               float bandFactor = 2.0f) {
	float depth_measure;
	int stepCount;
	Vector4f pt_camera_f;

	depth_measure = depth[x + y * imgSize.x];
	if (depth_measure <= 0 || (depth_measure - mu) < 0 || (depth_measure - mu) < viewFrustum_min ||
	    (depth_measure + mu) > viewFrustum_max)
		return;


	pt_camera_f.z = depth_measure; // (orthogonal) distance to the point from the image plane (meters)
	pt_camera_f.x =
			pt_camera_f.z * ((float(x) - invertedCameraProjectionParameters.z) * invertedCameraProjectionParameters.x);
	pt_camera_f.y =
			pt_camera_f.z * ((float(y) - invertedCameraProjectionParameters.w) * invertedCameraProjectionParameters.y);

	// distance to the point along camera ray
	float norm = sqrtf(pt_camera_f.x * pt_camera_f.x + pt_camera_f.y * pt_camera_f.y + pt_camera_f.z * pt_camera_f.z);

	Vector4f pt_buff;

	//Vector3f offset(-halfVoxelSize);
	pt_buff = pt_camera_f * (1.0f - (mu * bandFactor) / norm);
	pt_buff.w = 1.0f;
	//position along segment to march along ray in hash blocks (here -- starting point)
	// account for the fact that voxel coordinates represent the voxel center, and we need the extreme corner position of
	// the hash block, i.e. 0.5 voxel (1/16 block) offset from the position along the ray
	Vector3f currentCheckPosition_HashBlocks = (TO_VECTOR3(invertedCameraPose * pt_buff)) * oneOverVoxelBlockSize_Meters
	                                           + Vector3f(1.0f / (2.0f * VOXEL_BLOCK_SIZE));

	pt_buff = pt_camera_f * (1.0f + (mu * bandFactor) / norm);
	pt_buff.w = 1.0f;
	//end position of the segment to march along the ray
	Vector3f endCheckPosition_HashBlocks = (TO_VECTOR3(invertedCameraPose * pt_buff)) * oneOverVoxelBlockSize_Meters
	                                       + Vector3f(1.0f / (2.0f * VOXEL_BLOCK_SIZE));

	// segment from start of the (truncated SDF) band, through the observed point, and to the opposite (occluded)
	// end of the (truncated SDF) band (increased by backBandFactor), along the ray cast from the camera through the
	// point, in camera space
	ITMLib::ITMSegment marchSegment(currentCheckPosition_HashBlocks, endCheckPosition_HashBlocks);

	// number of steps to take along the truncated SDF band
	stepCount = (int) std::ceil(2.0f * marchSegment.length());

	// a single stride along the sdf band segment from one step to the next
	Vector3f strideVector = marchSegment.direction / (float) (stepCount - 1);

	Vector3s previousHashBlockPosition;

	//add neighbouring blocks
	for (int i = 0; i < stepCount; i++) {
		//find block position at current step
		Vector3s currentHashBlockPosition = TO_SHORT_FLOOR3(currentCheckPosition_HashBlocks);
		int incrementCount;
		if (i > 0 && (incrementCount = getIncrementCount(currentHashBlockPosition, previousHashBlockPosition)) > 1) {
			if (incrementCount == 2) {
				for (int iDirection = 0; iDirection < 3; iDirection++) {
					if (currentHashBlockPosition.values[iDirection] != previousHashBlockPosition.values[iDirection]) {
						Vector3s potentiallyMissedBlockPosition = previousHashBlockPosition;
						potentiallyMissedBlockPosition.values[iDirection] = currentHashBlockPosition.values[iDirection];
						if (SegmentIntersectsGridAlignedCube3D(marchSegment, TO_FLOAT3(potentiallyMissedBlockPosition),
						                                       1.0f)) {
							markForAllocationAndSetVisibilityTypeIfNotFound(potentiallyMissedBlockPosition, hashTable,
							                                                hashEntryStates,
							                                                entriesVisibleType,
							                                                blockCoords, collisionDetected);
						}
					}
				}
			} else {
				//incrementCount == 3
				for (int iDirection = 0; iDirection < 3; iDirection++) {
					Vector3s potentiallyMissedBlockPosition = previousHashBlockPosition;
					potentiallyMissedBlockPosition.values[iDirection] = currentHashBlockPosition.values[iDirection];
					if (SegmentIntersectsGridAlignedCube3D(marchSegment, TO_FLOAT3(potentiallyMissedBlockPosition),
					                                       1.0f)) {
						markForAllocationAndSetVisibilityTypeIfNotFound(potentiallyMissedBlockPosition, hashTable,
						                                                hashEntryStates,
						                                                entriesVisibleType,
						                                                blockCoords, collisionDetected);
					}
					potentiallyMissedBlockPosition = currentHashBlockPosition;
					potentiallyMissedBlockPosition.values[iDirection] = previousHashBlockPosition.values[iDirection];
					if (SegmentIntersectsGridAlignedCube3D(marchSegment, TO_FLOAT3(potentiallyMissedBlockPosition),
					                                       1.0f)) {
						markForAllocationAndSetVisibilityTypeIfNotFound(potentiallyMissedBlockPosition, hashTable,
						                                                hashEntryStates,
						                                                entriesVisibleType,
						                                                blockCoords, collisionDetected);
					}
				}
			}
		}
		markForAllocationAndSetVisibilityTypeIfNotFound(currentHashBlockPosition, hashTable, hashEntryStates,
		                                                entriesVisibleType,
		                                                blockCoords, collisionDetected);

		currentCheckPosition_HashBlocks += strideVector;
		previousHashBlockPosition = currentHashBlockPosition;
	}
}


template<bool useSwapping>
_CPU_AND_GPU_CODE_ inline void checkPointVisibility(THREADPTR(bool)& isVisible, THREADPTR(bool)& isVisibleEnlarged,
                                                    const THREADPTR(Vector4f)& pt_image, const CONSTPTR(Matrix4f)& M_d,
                                                    const CONSTPTR(Vector4f)& projParams_d,
                                                    const CONSTPTR(Vector2i)& imgSize) {
	Vector4f pt_buff;

	pt_buff = M_d * pt_image;

	if (pt_buff.z < 1e-10f) return;

	pt_buff.x = projParams_d.x * pt_buff.x / pt_buff.z + projParams_d.z;
	pt_buff.y = projParams_d.y * pt_buff.y / pt_buff.z + projParams_d.w;

	if (pt_buff.x >= 0 && pt_buff.x < imgSize.x && pt_buff.y >= 0 && pt_buff.y < imgSize.y) {
		isVisible = true;
		isVisibleEnlarged = true;
	} else if (useSwapping) {
		Vector4i lims;
		lims.x = -imgSize.x / 8;
		lims.y = imgSize.x + imgSize.x / 8;
		lims.z = -imgSize.y / 8;
		lims.w = imgSize.y + imgSize.y / 8;

		if (pt_buff.x >= lims.x && pt_buff.x < lims.y && pt_buff.y >= lims.z && pt_buff.y < lims.w)
			isVisibleEnlarged = true;
	}
}

template<bool useSwapping>
_CPU_AND_GPU_CODE_ inline void checkBlockVisibility(THREADPTR(bool)& isVisible, THREADPTR(bool)& isVisibleEnlarged,
                                                    const THREADPTR(Vector3s)& hashPos, const CONSTPTR(Matrix4f)& M_d,
                                                    const CONSTPTR(Vector4f)& projParams_d,
                                                    const CONSTPTR(float)& voxelSize,
                                                    const CONSTPTR(Vector2i)& imgSize) {
	Vector4f pt_image;
	float factor = (float) VOXEL_BLOCK_SIZE * voxelSize;

	isVisible = false;
	isVisibleEnlarged = false;

	// 0 0 0
	pt_image.x = (float) hashPos.x * factor;
	pt_image.y = (float) hashPos.y * factor;
	pt_image.z = (float) hashPos.z * factor;
	pt_image.w = 1.0f;
	checkPointVisibility<useSwapping>(isVisible, isVisibleEnlarged, pt_image, M_d, projParams_d, imgSize);
	if (isVisible) return;

	// 0 0 1
	pt_image.z += factor;
	checkPointVisibility<useSwapping>(isVisible, isVisibleEnlarged, pt_image, M_d, projParams_d, imgSize);
	if (isVisible) return;

	// 0 1 1
	pt_image.y += factor;
	checkPointVisibility<useSwapping>(isVisible, isVisibleEnlarged, pt_image, M_d, projParams_d, imgSize);
	if (isVisible) return;

	// 1 1 1
	pt_image.x += factor;
	checkPointVisibility<useSwapping>(isVisible, isVisibleEnlarged, pt_image, M_d, projParams_d, imgSize);
	if (isVisible) return;

	// 1 1 0
	pt_image.z -= factor;
	checkPointVisibility<useSwapping>(isVisible, isVisibleEnlarged, pt_image, M_d, projParams_d, imgSize);
	if (isVisible) return;

	// 1 0 0
	pt_image.y -= factor;
	checkPointVisibility<useSwapping>(isVisible, isVisibleEnlarged, pt_image, M_d, projParams_d, imgSize);
	if (isVisible) return;

	// 0 1 0
	pt_image.x -= factor;
	pt_image.y += factor;
	checkPointVisibility<useSwapping>(isVisible, isVisibleEnlarged, pt_image, M_d, projParams_d, imgSize);
	if (isVisible) return;

	// 1 0 1
	pt_image.x += factor;
	pt_image.y -= factor;
	pt_image.z += factor;
	checkPointVisibility<useSwapping>(isVisible, isVisibleEnlarged, pt_image, M_d, projParams_d, imgSize);
	if (isVisible) return;
}


_CPU_AND_GPU_CODE_
inline
bool FindOrAllocateHashEntry(const Vector3s& hashEntryPosition, ITMHashEntry* hashTable, ITMHashEntry*& resultEntry,
                             int& lastFreeVoxelBlockId, int& lastFreeExcessListId, const int* voxelAllocationList,
                             const int* excessAllocationList, int& hash) {
	hash = hashIndex(hashEntryPosition);
	ITMHashEntry hashEntry = hashTable[hash];
	if (!IS_EQUAL3(hashEntry.pos, hashEntryPosition) || hashEntry.ptr < -1) {
		bool isExcess = false;
		//search excess list only if there is no room in ordered part
		if (hashEntry.ptr >= -1) {
			while (hashEntry.offset >= 1) {
				hash = ORDERED_LIST_SIZE + hashEntry.offset - 1;
				hashEntry = hashTable[hash];
				if (IS_EQUAL3(hashEntry.pos, hashEntryPosition) && hashEntry.ptr >= -1) {
					resultEntry = &hashTable[hash];
					return true;
				}
			}
			isExcess = true;

		}
		//still not found, allocate
		if (isExcess && lastFreeVoxelBlockId >= 0 && lastFreeExcessListId >= 0) {
			//there is room in the voxel block array and excess list
			ITMHashEntry newHashEntry;
			newHashEntry.pos = hashEntryPosition;
			newHashEntry.ptr = voxelAllocationList[lastFreeVoxelBlockId];
			newHashEntry.offset = 0;
			int exlOffset = excessAllocationList[lastFreeExcessListId];
			hashTable[hash].offset = exlOffset + 1; //connect to child
			hashTable[ORDERED_LIST_SIZE +
			          exlOffset] = newHashEntry; //add child to the excess list
			resultEntry = &hashTable[ORDERED_LIST_SIZE +
			                         exlOffset];
			lastFreeVoxelBlockId--;
			lastFreeExcessListId--;
			return true;
		} else if (lastFreeVoxelBlockId >= 0) {
			//there is room in the voxel block array
			ITMHashEntry newHashEntry;
			newHashEntry.pos = hashEntryPosition;
			newHashEntry.ptr = voxelAllocationList[lastFreeVoxelBlockId];
			newHashEntry.offset = 0;
			hashTable[hash] = newHashEntry;
			resultEntry = &hashTable[hash];
			lastFreeVoxelBlockId--;
			return true;
		} else {
			return false;
		}
	} else {
		//HashEntry already exists, return the pointer to it
		resultEntry = &hashTable[hash];
		return true;
	}
}
