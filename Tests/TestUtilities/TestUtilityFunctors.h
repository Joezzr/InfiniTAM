//  ================================================================
//  Created by Gregory Kramida on 3/2/20.
//  Copyright (c) 2020 Gregory Kramida
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

// stdlib
#include <mutex>
#include <random>

// local
#include "../../ITMLib/Objects/Volume/VoxelBlockHash.h"
#include "../../ORUtils/MemoryDeviceType.h"
#include "../../ITMLib/Utils/Geometry/GeometryBooleanOperations.h"
#include <limits>

#ifdef __CUDACC__
#include "TestUtilityKernels.h"
#endif


template<typename TVoxel, typename TIndex, MemoryDeviceType TMemoryDeviceType>
struct AssignRandomDepthWeightsInRangeFunctor;

template<typename TVoxel, MemoryDeviceType TMemoryDeviceType>
struct DepthOutsideRangeFinder;
template<typename TVoxel, MemoryDeviceType TMemoryDeviceType>
struct DepthOutsideRangeInExtentFinder;

using namespace ITMLib;

#ifdef __CUDACC__
template<typename TVoxel>
struct AssignRandomDepthWeightsInRangeFunctor<TVoxel, VoxelBlockHash, MEMORYDEVICE_CUDA> {

	AssignRandomDepthWeightsInRangeFunctor(const Extent2Di& range, const Extent3Di& bounds) :
			range(range), bounds(bounds), scale(range.to - range.from) {

		ORcudaSafeCall(cudaMalloc(&random_states, total_state_count * sizeof(curandState)));
		dim3 rand_init_cuda_block_size(VOXEL_BLOCK_SIZE3);
		dim3 rand_init_cuda_grid_size(random_hash_block_count);

		initialize_random_numbers_for_spatial_blocks
				<<< rand_init_cuda_grid_size, rand_init_cuda_block_size >>>
											   (random_states);
		ORcudaKernelCheck;

	}

	~AssignRandomDepthWeightsInRangeFunctor() {
		ORcudaSafeCall(cudaFree(random_states));
	}

	__device__
	inline void operator()(TVoxel& voxel, const Vector3i& position) {
		if (IsPointInBounds(position, bounds)) {
			unsigned int thread_id = threadIdx.x * threadIdx.y * threadIdx.z;
			unsigned int state_index = (blockIdx.x * blockDim.x + thread_id) % total_state_count;
			voxel.w_depth = (uchar) ((float) range.from + curand_uniform(&random_states[state_index]) * scale);
			while(voxel.w_depth == range.to){
				voxel.w_depth = (uchar) ((float) range.from + curand_uniform(&random_states[state_index]) * scale);
			}
		}
	}

private:
	const unsigned int random_hash_block_count = 64;
	const unsigned int total_state_count = random_hash_block_count * VOXEL_BLOCK_SIZE3;
	curandState* random_states;
	const Extent2Di range;
	const Extent3Di bounds;
	float scale;

};

// use for debugging ranges in reduction tests
template<typename TVoxel>
struct DepthOutsideRangeFinder<TVoxel, MEMORYDEVICE_CUDA>{

public:
	DepthOutsideRangeFinder(const Extent2Di& range) : range(range){}

	__device__
	void operator()(TVoxel& voxel, const Vector3i& position) {
		if((range.from > voxel.w_depth) || (voxel.w_depth >= range.to)){
			printf("Out of range: %d, at %d, %d, %d\n", voxel.w_depth, position.x, position.y, position.z);
		}
	}

private:
	Extent2Di range;
};
template<typename TVoxel>
struct DepthOutsideRangeInExtentFinder<TVoxel, MEMORYDEVICE_CUDA>{

public:
	DepthOutsideRangeInExtentFinder(const Extent2Di& range, const Extent3Di& bounds) : range(range), bounds(bounds){}

	__device__
	void operator()(TVoxel& voxel, const Vector3i& position) {
		if (IsPointInBounds(position, bounds)) {
			if((range.from > voxel.w_depth) || (voxel.w_depth >= range.to)){
				printf("Out of range: %d, at %d, %d, %d\n", voxel.w_depth, position.x, position.y, position.z);
			}
		}
	}

private:
	Extent2Di range;
	Extent3Di bounds;
};

#else
template<typename TVoxel>
struct AssignRandomDepthWeightsInRangeFunctor<TVoxel, VoxelBlockHash, MEMORYDEVICE_CPU> {

	AssignRandomDepthWeightsInRangeFunctor(const Extent2Di& range, const Extent3Di& bounds) :
			range(range), bounds(bounds), generator(random_device()), distribution(range.from, range.to-1) {
		if(range.from < 0 || range.to < 0 || range.to > std::numeric_limits<unsigned char>::max() + 1 || range.from > std::numeric_limits<unsigned char>::max() + 1){
			DIEWITHEXCEPTION_REPORTLOCATION("Range bound outside of limits for depth weight.");
		}
		if(range.to <= range.from){
			DIEWITHEXCEPTION_REPORTLOCATION("Invalid range.");
		}
	}

	inline void operator()(TVoxel& voxel, const Vector3i& position) {
		if (IsPointInBounds(position, bounds)) {
			const std::lock_guard<std::mutex> lock(mutex);
			voxel.w_depth = static_cast<unsigned char>(distribution(generator));
		}
	}

private:
	std::random_device random_device;
	std::mt19937 generator;
	const Extent2Di range;
	const Extent3Di bounds;
	std::uniform_int_distribution<unsigned int> distribution;
	std::mutex mutex;

};
#endif