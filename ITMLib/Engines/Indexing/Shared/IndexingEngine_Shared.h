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
#include "../../../Objects/Volume/VoxelBlockHash.h"
#include "../../../Objects/Volume/RepresentationAccess.h"
#include "../../Common/CommonFunctors.h"
#include "../../Common/AllocationTempData.h"
#include "../../../Utils/Math.h"
#include "../../../Utils/PixelUtils.h"
#include "../../../Utils/VoxelFlags.h"
#include "../../../Utils/HashBlockProperties.h"
#include "../../../Utils/Geometry/IntersectionChecks.h"

#ifdef __CUDACC__
#include "../../../Utils/CUDAUtils.h"
#include "../../Traversal/CUDA/VolumeTraversal_CUDA_VoxelBlockHash.h"
#endif

using namespace ITMLib;




/**
 * \brief Determines whether the hash block at the specified block position needs it's voxels to be allocated, as well
 * as whether they should be allocated in the excess list or the ordered list of the hash table.
 * If any of these are true, marks the corresponding entry in \param hashEntryStates
 * \param[in,out] hashEntryStates  array where to set the allocation type at final hashIdx index
 * \param[in,out] hashBlockCoordinates  array block coordinates for the new hash blocks at final hashIdx index
 * \param[in,out] hashCode  takes in original index assuming coords, i.e. \refitem HashCodeFromBlockPosition(\param desiredHashBlockPosition),
 * returns final index of the hash block to be allocated (may be updated based on hash closed chaining)
 * \param[in] desiredHashBlockPosition  position of the hash block to check / allocate
 * \param[in] hashTable  hash table with existing blocks
 * \param[in] collisionDetected set to true if a block with the same hashcode has already been marked for allocation ( a collision occured )
 * \return true if the block needs allocation, false otherwise
 */
_CPU_AND_GPU_CODE_
inline bool MarkAsNeedingAllocationIfNotFound(ITMLib::HashEntryAllocationState* hashEntryStates,
                                              Vector3s* hashBlockCoordinates, int& hashCode,
                                              const CONSTPTR(Vector3s)& desiredHashBlockPosition,
                                              const CONSTPTR(HashEntry)* hashTable, bool& collisionDetected) {

	HashEntry hashEntry = hashTable[hashCode];
	//check if hash table contains entry

	if (!(IS_EQUAL3(hashEntry.pos, desiredHashBlockPosition) && hashEntry.ptr >= -1)) {

		auto setHashEntryState = [&](HashEntryAllocationState state) {
#if defined(__CUDACC__) && defined(__CUDA_ARCH__)
			if (atomicCAS((char*) hashEntryStates + hashCode,
					  (char) ITMLib::NEEDS_NO_CHANGE,
					  (char) state) != ITMLib::NEEDS_NO_CHANGE){
			if (IS_EQUAL3(hashBlockCoordinates[hashCode], desiredHashBlockPosition)) return false;
			//hash code already marked for allocation, but at different coordinates, cannot allocate
			collisionDetected = true;
			return false;
		} else {
			hashBlockCoordinates[hashCode] = desiredHashBlockPosition;
			return true;
		}
#else
			//TODO: come up with an atomics-based solution for OpenMP
			bool success = false;
#ifdef WITH_OPENMP
#pragma omp critical
#endif
			{
				//single-threaded version
				if (hashEntryStates[hashCode] != ITMLib::NEEDS_NO_CHANGE) {
					if (!IS_EQUAL3(hashBlockCoordinates[hashCode], desiredHashBlockPosition)) {
						//hash code already marked for allocation, but at different coordinates, cannot allocate
						collisionDetected = true;
					}
					success = false;
				} else {
					hashEntryStates[hashCode] = state;
					hashBlockCoordinates[hashCode] = desiredHashBlockPosition;
					success = true;
				}
			}
			return success;
#endif
		};
		if (hashEntry.ptr >= -1) {
			//search excess list only if there is no room in ordered part
			while (hashEntry.offset >= 1) {
				hashCode = ORDERED_LIST_SIZE + hashEntry.offset - 1;
				hashEntry = hashTable[hashCode];

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


_CPU_AND_GPU_CODE_ inline void
MarkForAllocationAndSetVisibilityTypeIfNotFound(ITMLib::HashEntryAllocationState* hashEntryStates,
                                                Vector3s* hashBlockCoordinates,
                                                HashBlockVisibility* blockVisibilityTypes,
                                                Vector3s desiredHashBlockPosition,
                                                const CONSTPTR(HashEntry)* hashTable, bool& collisionDetected) {

	int hashCode = HashCodeFromBlockPosition(desiredHashBlockPosition);

	HashEntry hashEntry = hashTable[hashCode];

	//check if hash table contains entry
	if (IS_EQUAL3(hashEntry.pos, desiredHashBlockPosition) && hashEntry.ptr >= -1) {
		//entry has been streamed out but is visible or in memory and visible
		blockVisibilityTypes[hashCode] = (hashEntry.ptr == -1) ? STREAMED_OUT_AND_VISIBLE
		                                                       : IN_MEMORY_AND_VISIBLE;
		return;
	}

	HashEntryAllocationState allocationState = NEEDS_ALLOCATION_IN_ORDERED_LIST;
	if (hashEntry.ptr >= -1) //search excess list only if there is no room in ordered part
	{
		while (hashEntry.offset >= 1) {
			hashCode = ORDERED_LIST_SIZE + hashEntry.offset - 1;
			hashEntry = hashTable[hashCode];

			if (IS_EQUAL3(hashEntry.pos, desiredHashBlockPosition) && hashEntry.ptr >= -1) {
				//entry has been streamed out but is visible or in memory and visible
				blockVisibilityTypes[hashCode] = (hashEntry.ptr == -1) ? STREAMED_OUT_AND_VISIBLE
				                                                       : IN_MEMORY_AND_VISIBLE;
				return;
			}
		}
		allocationState = NEEDS_ALLOCATION_IN_EXCESS_LIST;
	}

#if defined(__CUDACC__) && defined(__CUDA_ARCH__)
	if (atomicCAS((char*) hashEntryStates + hashCode,
				  (char) ITMLib::NEEDS_NO_CHANGE,
				  (char) allocationState) != ITMLib::NEEDS_NO_CHANGE) {
		if (IS_EQUAL3(hashBlockCoordinates[hashCode], desiredHashBlockPosition)) return;
		collisionDetected = true;
	} else {
		//needs allocation
		if (allocationState == NEEDS_ALLOCATION_IN_ORDERED_LIST)
			blockVisibilityTypes[hashCode] = HashBlockVisibility::IN_MEMORY_AND_VISIBLE; //new entry is visible
		hashBlockCoordinates[hashCode] = desiredHashBlockPosition;
	}
#else
#if defined(WITH_OPENMP)
#pragma omp critical
#endif
	{
		if (hashEntryStates[hashCode] != ITMLib::NEEDS_NO_CHANGE) {
			collisionDetected = true;
		} else {
			//needs allocation
			hashEntryStates[hashCode] = allocationState;
			if (allocationState == NEEDS_ALLOCATION_IN_ORDERED_LIST)
				blockVisibilityTypes[hashCode] = HashBlockVisibility::IN_MEMORY_AND_VISIBLE; //new entry is visible
			hashBlockCoordinates[hashCode] = desiredHashBlockPosition;
		}
	}
#endif
}


template<typename TWarp, typename TVoxel, WarpType TWarpType>
struct WarpBasedAllocationMarkerFunctor {
	WarpBasedAllocationMarkerFunctor(
			VoxelVolume<TVoxel, VoxelBlockHash>* sourceVolume,
			VoxelVolume<TVoxel, VoxelBlockHash>* volumeToAllocate,
			Vector3s* allocationBlockCoords,
			HashEntryAllocationState* warpedEntryAllocationStates) :

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

		int targetBlockHash = HashCodeFromBlockPosition(hashBlockPosition);

		MarkAsNeedingAllocationIfNotFound(warpedEntryAllocationStates, allocationBlockCoords, targetBlockHash,
		                                  hashBlockPosition, targetTSDFHashEntries, collisionDetected);
	}

	bool collisionDetected;

private:


	VoxelVolume<TVoxel, VoxelBlockHash>* targetTSDFScene;
	TVoxel* targetTSDFVoxels;
	HashEntry* targetTSDFHashEntries;
	VoxelBlockHash::IndexCache targetTSDFCache;

	VoxelVolume<TVoxel, VoxelBlockHash>* sourceTSDFScene;
	TVoxel* sourceTSDFVoxels;
	HashEntry* sourceTSDFHashEntries;
	VoxelBlockHash::IndexCache sourceTSDFCache;

	Vector3s* allocationBlockCoords;
	HashEntryAllocationState* warpedEntryAllocationStates;
};


_CPU_AND_GPU_CODE_ inline int getIncrementCount(Vector3s coord1, Vector3s coord2) {
	return static_cast<int>(coord1.x != coord2.x) +
	       static_cast<int>(coord1.y != coord2.y) +
	       static_cast<int>(coord1.z != coord2.z);
}

_CPU_AND_GPU_CODE_ inline void
findVoxelHashBlocksAlongSegment(ITMLib::HashEntryAllocationState* hash_entry_allocation_states,
                                Vector3s* hash_block_coordinates,
                                HashBlockVisibility* hash_block_visibility_types,
                                const CONSTPTR(HashEntry)* hash_table,
                                const ITMLib::Segment& segment_in_hash_blocks,
                                bool& collision_detected) {

	// number of steps to take along the truncated SDF band
	int step_count = (int) std::ceil(2.0f * segment_in_hash_blocks.length());

	// a single stride along the sdf band segment from one step to the next
	Vector3f strideVector = segment_in_hash_blocks.direction / (float) (step_count - 1);

	Vector3s previousHashBlockPosition;
	Vector3f check_position = segment_in_hash_blocks.origin;

	//add neighbouring blocks
	for (int i = 0; i < step_count; i++) {
		//find block position at current step
		Vector3s currentHashBlockPosition = TO_SHORT_FLOOR3(check_position);
		int incrementCount;
		if (i > 0 && (incrementCount = getIncrementCount(currentHashBlockPosition, previousHashBlockPosition)) > 1) {
			if (incrementCount == 2) {
				for (int iDirection = 0; iDirection < 3; iDirection++) {
					if (currentHashBlockPosition.values[iDirection] != previousHashBlockPosition.values[iDirection]) {
						Vector3s potentiallyMissedBlockPosition = previousHashBlockPosition;
						potentiallyMissedBlockPosition.values[iDirection] = currentHashBlockPosition.values[iDirection];
						if (SegmentIntersectsGridAlignedCube3D(segment_in_hash_blocks,
						                                       TO_FLOAT3(potentiallyMissedBlockPosition),
						                                       1.0f)) {
							MarkForAllocationAndSetVisibilityTypeIfNotFound(
									hash_entry_allocation_states,
									hash_block_coordinates, hash_block_visibility_types, potentiallyMissedBlockPosition,
									hash_table,
									collision_detected);
						}
					}
				}
			} else {
				//incrementCount == 3
				for (int iDirection = 0; iDirection < 3; iDirection++) {
					Vector3s potentiallyMissedBlockPosition = previousHashBlockPosition;
					potentiallyMissedBlockPosition.values[iDirection] = currentHashBlockPosition.values[iDirection];
					if (SegmentIntersectsGridAlignedCube3D(segment_in_hash_blocks,
					                                       TO_FLOAT3(potentiallyMissedBlockPosition),
					                                       1.0f)) {
						MarkForAllocationAndSetVisibilityTypeIfNotFound(
								hash_entry_allocation_states,
								hash_block_coordinates, hash_block_visibility_types, potentiallyMissedBlockPosition,
								hash_table,
								collision_detected);
					}
					potentiallyMissedBlockPosition = currentHashBlockPosition;
					potentiallyMissedBlockPosition.values[iDirection] = previousHashBlockPosition.values[iDirection];
					if (SegmentIntersectsGridAlignedCube3D(segment_in_hash_blocks,
					                                       TO_FLOAT3(potentiallyMissedBlockPosition),
					                                       1.0f)) {
						MarkForAllocationAndSetVisibilityTypeIfNotFound(
								hash_entry_allocation_states,
								hash_block_coordinates, hash_block_visibility_types, potentiallyMissedBlockPosition,
								hash_table,
								collision_detected);
					}
				}
			}
		}
		MarkForAllocationAndSetVisibilityTypeIfNotFound(hash_entry_allocation_states,
		                                                hash_block_coordinates,
		                                                hash_block_visibility_types, currentHashBlockPosition, hash_table,
		                                                collision_detected);

		check_position += strideVector;
		previousHashBlockPosition = currentHashBlockPosition;
	}
}

//TODO: function to find segment in narrow band around point along ray
//_CPU_AND_GPU_CODE_ inline void 

_CPU_AND_GPU_CODE_ inline void
findVoxelHashBlocksOnRayNearAndBetweenTwoSurfaces(ITMLib::HashEntryAllocationState* hash_entry_allocation_states,
                                                  Vector3s* hash_block_coordinates,
                                                  HashBlockVisibility* hash_block_visibility_types,
                                                  const CONSTPTR(HashEntry)* hash_table, int x, int y,
                                                  const CONSTPTR(float)* surface_1_depth,
                                                  const CONSTPTR(Vector4f)* surface_2_points,
                                                  float surface_distance_cutoff, float one_over_block_size,
                                                  Matrix4f inverted_camera_pose,
                                                  Vector4f inverted_camera_projection_parameters,
                                                  Vector2i offset_to_depth_image, Vector2i depth_image_size,
                                                  float near_clipping_distance, float far_clipping_distance,
                                                  bool& collisionDetected) {

	if(x >= offset_to_depth_image.x && y >= offset_to_depth_image.y && x < offset_to_depth_image.x + depth_image_size.x &&
		y < offset_to_depth_image.y + depth_image_size.y){

		float depth_measure = surface_1_depth[x + y * depth_image_size.x];
		if (depth_measure <= 0 || (depth_measure - surface_distance_cutoff) < 0 ||
		    (depth_measure - surface_distance_cutoff) < near_clipping_distance ||
		    (depth_measure + surface_distance_cutoff) > far_clipping_distance)
			return;

		Vector4f surface1_point_in_camera_space;
		surface1_point_in_camera_space.z = depth_measure; // (orthogonal) distance to the point from the image plane (meters)
		surface1_point_in_camera_space.x = surface1_point_in_camera_space.z *
		                                   ((float(x) - inverted_camera_projection_parameters.z) * inverted_camera_projection_parameters.x);
		surface1_point_in_camera_space.y = surface1_point_in_camera_space.z *
		                                   ((float(y) - inverted_camera_projection_parameters.w) * inverted_camera_projection_parameters.y);

	}


	Vector4f pt_camera_f;
	pt_camera_f.z = depth_measure; // (orthogonal) distance to the point from the image plane (meters)
	pt_camera_f.x =
			pt_camera_f.z * ((float(x) - inverted_camera_projection_parameters.z) * inverted_camera_projection_parameters.x);
	pt_camera_f.y =
			pt_camera_f.z * ((float(y) - inverted_camera_projection_parameters.w) * inverted_camera_projection_parameters.y);

	// distance to the point along camera ray
	float norm = sqrtf(pt_camera_f.x * pt_camera_f.x + pt_camera_f.y * pt_camera_f.y + pt_camera_f.z * pt_camera_f.z);

	Vector4f pt_buff;

	//Vector3f offset(-halfVoxelSize);
	pt_buff = pt_camera_f * (1.0f - surface_distance_cutoff / norm);
	pt_buff.w = 1.0f;
	//position along segment to march along ray in hash blocks (here -- starting point)
	// account for the fact that voxel coordinates represent the voxel center, and we need the extreme corner position of
	// the hash block, i.e. 0.5 voxel (1/16 block) offset from the position along the ray
	Vector3f march_segment_origin_in_hash_blocks = (TO_VECTOR3(inverted_camera_pose * pt_buff)) * one_over_block_size
	                                               + Vector3f(1.0f / (2.0f * VOXEL_BLOCK_SIZE));

	pt_buff = pt_camera_f * (1.0f + surface_distance_cutoff / norm);
	pt_buff.w = 1.0f;
	//end position of the segment to march along the ray
	Vector3f march_segment_end_in_hash_blocks = (TO_VECTOR3(inverted_camera_pose * pt_buff)) * one_over_block_size
	                                            + Vector3f(1.0f / (2.0f * VOXEL_BLOCK_SIZE));

	// segment from start of the (truncated SDF) band, through the observed point, and to the opposite (occluded)
	// end of the (truncated SDF) band (increased by backBandFactor), along the ray cast from the camera through the
	// point, in camera space
	ITMLib::Segment march_segment(march_segment_origin_in_hash_blocks, march_segment_end_in_hash_blocks);

	findVoxelHashBlocksAlongSegment(hash_entry_allocation_states, hash_block_coordinates, hash_block_visibility_types,
	                                hash_table, march_segment, collisionDetected);

}

_CPU_AND_GPU_CODE_ inline void
findVoxelBlocksForRayNearSurface(ITMLib::HashEntryAllocationState* hash_entry_allocation_states, Vector3s* hash_block_coordinates,
                                 HashBlockVisibility* hash_block_visibility_types, const CONSTPTR(HashEntry)* hash_table,
                                 int x, int y, const CONSTPTR(float)* depth, float surface_distance_cutoff,
                                 Matrix4f inverted_camera_pose, Vector4f inverted_camera_projection_parameters,
                                 float one_over_hash_block_size, Vector2i depth_image_size,
                                 float near_clipping_distance, float far_clipping_distance, bool& collision_detected) {

	float depth_measure = depth[x + y * depth_image_size.x];
	if (depth_measure <= 0 || (depth_measure - surface_distance_cutoff) < 0 ||
	    (depth_measure - surface_distance_cutoff) < near_clipping_distance ||
	    (depth_measure + surface_distance_cutoff) > far_clipping_distance)
		return;

	Vector4f pt_camera_f;
	pt_camera_f.z = depth_measure; // (orthogonal) distance to the point from the image plane (meters)
	pt_camera_f.x =
			pt_camera_f.z * ((float(x) - inverted_camera_projection_parameters.z) * inverted_camera_projection_parameters.x);
	pt_camera_f.y =
			pt_camera_f.z * ((float(y) - inverted_camera_projection_parameters.w) * inverted_camera_projection_parameters.y);

	// distance to the point along camera ray
	float norm = sqrtf(pt_camera_f.x * pt_camera_f.x + pt_camera_f.y * pt_camera_f.y + pt_camera_f.z * pt_camera_f.z);

	Vector4f pt_buff;

	//Vector3f offset(-halfVoxelSize);
	pt_buff = pt_camera_f * (1.0f - surface_distance_cutoff / norm);
	pt_buff.w = 1.0f;
	//position along segment to march along ray in hash blocks (here -- starting point)
	// account for the fact that voxel coordinates represent the voxel center, and we need the extreme corner position of
	// the hash block, i.e. 0.5 voxel (1/16 block) offset from the position along the ray
	Vector3f currentCheckPosition_HashBlocks = (TO_VECTOR3(inverted_camera_pose * pt_buff)) * one_over_hash_block_size
	                                           + Vector3f(1.0f / (2.0f * VOXEL_BLOCK_SIZE));

	pt_buff = pt_camera_f * (1.0f + surface_distance_cutoff / norm);
	pt_buff.w = 1.0f;
	//end position of the segment to march along the ray
	Vector3f endCheckPosition_HashBlocks = (TO_VECTOR3(inverted_camera_pose * pt_buff)) * one_over_hash_block_size
	                                       + Vector3f(1.0f / (2.0f * VOXEL_BLOCK_SIZE));

	// segment from start of the (truncated SDF) band, through the observed point, and to the opposite (occluded)
	// end of the (truncated SDF) band (increased by backBandFactor), along the ray cast from the camera through the
	// point, in camera space
	ITMLib::Segment march_segment(currentCheckPosition_HashBlocks, endCheckPosition_HashBlocks);

	findVoxelHashBlocksAlongSegment(hash_entry_allocation_states, hash_block_coordinates, hash_block_visibility_types,
	                                hash_table, march_segment, collision_detected);
}


_CPU_AND_GPU_CODE_
inline
bool FindOrAllocateHashEntry(const Vector3s& hashEntryPosition, HashEntry* hashTable, HashEntry*& resultEntry,
                             int& lastFreeVoxelBlockId, int& lastFreeExcessListId, const int* voxelAllocationList,
                             const int* excessAllocationList, int& hashCode) {
	hashCode = HashCodeFromBlockPosition(hashEntryPosition);
	HashEntry hashEntry = hashTable[hashCode];
	if (!IS_EQUAL3(hashEntry.pos, hashEntryPosition) || hashEntry.ptr < -1) {
		bool isExcess = false;
		//search excess list only if there is no room in ordered part
		if (hashEntry.ptr >= -1) {
			while (hashEntry.offset >= 1) {
				hashCode = ORDERED_LIST_SIZE + hashEntry.offset - 1;
				hashEntry = hashTable[hashCode];
				if (IS_EQUAL3(hashEntry.pos, hashEntryPosition) && hashEntry.ptr >= -1) {
					resultEntry = &hashTable[hashCode];
					return true;
				}
			}
			isExcess = true;

		}
		//still not found, allocate
		if (isExcess && lastFreeVoxelBlockId >= 0 && lastFreeExcessListId >= 0) {
			//there is room in the voxel block array and excess list
			HashEntry newHashEntry;
			newHashEntry.pos = hashEntryPosition;
			newHashEntry.ptr = voxelAllocationList[lastFreeVoxelBlockId];
			newHashEntry.offset = 0;
			int exlOffset = excessAllocationList[lastFreeExcessListId];
			hashTable[hashCode].offset = exlOffset + 1; //connect to child
			hashCode = ORDERED_LIST_SIZE + exlOffset;
			hashTable[hashCode] = newHashEntry; //add child to the excess list
			resultEntry = &hashTable[hashCode];
			lastFreeVoxelBlockId--;
			lastFreeExcessListId--;
			return true;
		} else if (lastFreeVoxelBlockId >= 0) {
			//there is room in the voxel block array
			HashEntry newHashEntry;
			newHashEntry.pos = hashEntryPosition;
			newHashEntry.ptr = voxelAllocationList[lastFreeVoxelBlockId];
			newHashEntry.offset = 0;
			hashTable[hashCode] = newHashEntry;
			resultEntry = &hashTable[hashCode];
			lastFreeVoxelBlockId--;
			return true;
		} else {
			return false;
		}
	} else {
		//HashEntry already exists, return the pointer to it
		resultEntry = &hashTable[hashCode];
		return true;
	}
}
