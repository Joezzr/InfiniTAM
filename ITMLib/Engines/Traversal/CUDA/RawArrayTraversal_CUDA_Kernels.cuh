//  ================================================================
//  Created by Gregory Kramida on 2/4/20.
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
//local
#include "../../../Utils/Math.h"
#include "../../../Objects/Volume/VoxelBlockHash.h"
#include "../Shared/JobCountPolicy.h"

namespace {
// CUDA global kernels
// region ====================================== CONTIGUOUS TRAVERSAL PER-CUDA-THREAD FUNCTIONS ======================================================

// *** Exact job count ***
template<typename TApplyFunction>
__device__ inline void TraverseContiguous_Exact_Generic_device(const unsigned int element_count, TApplyFunction&& apply) {
	unsigned int i_item = threadIdx.x + blockIdx.x * blockDim.x;
	if (i_item >= element_count) return;
	apply(i_item);
}

template<typename TData, typename TFunctor>
__global__ void TraverseContiguousWithoutItemIndex_Exact_device(TData* data, const unsigned int element_count, TFunctor* functor_device) {
	TraverseContiguous_Exact_Generic_device(element_count, [&data, &functor_device](const int i_item) { (*functor_device)(data[i_item]); });
}

template<typename TData, typename TFunctor>
__global__ void TraverseContiguousWithIndex_Exact_device(TData* data, const unsigned int element_count, TFunctor* functor_device) {
	TraverseContiguous_Exact_Generic_device(element_count, [&data, &functor_device](const int i_item) { (*functor_device)(data[i_item], i_item); });
}

template<typename TData, typename TFunctor>
__global__ void TraverseContiguous_Padded_Generic_device(TData* data, const unsigned int element_count, TFunctor* functor_device) {
	unsigned int i_item = threadIdx.x + blockIdx.x * blockDim.x;
	(*functor_device)(data, i_item, i_item >= element_count);
}

//endregion
// region ====================================== SAMPLE TRAVERSAL PER-CUDA-THREAD FUNCTIONS ==========================================================

// *** Exact job count ***
template<typename TApplyFunction>
__device__ inline void TraverseSample_Exact_Generic_device(const int* sample_indices, const int sample_size, TApplyFunction&& apply) {
	int i_index = threadIdx.x + blockIdx.x * blockDim.x;
	if (i_index >= sample_size) return;
	int i_item = sample_indices[i_index];
	apply(i_item);
}

template<typename TData, typename TFunctor>
__global__ void TraverseSampleWithIndex_Exact_device(TData* data, const int* sample_indices, const int sample_size, TFunctor* functor_device) {
	TraverseSample_Exact_Generic_device(sample_indices, sample_size,
	                                    [&data, &functor_device](const int i_item) { (*functor_device)(data[i_item], i_item); });
}

template<typename TData, typename TFunctor>
__global__ void TraverseSampleWithoutIndex_Exact_device(TData* data, const int* sample_indices, const int sample_size, TFunctor* functor_device) {
	TraverseSample_Exact_Generic_device(sample_indices, sample_size, [&data, &functor_device](const int i_item) { (*functor_device)(data[i_item]); });
}

// *** Padded job count ***
template<typename TData, typename TFunctor>
__global__ void TraverseSample_Padded_Generic_device(TData* data, const int* sample_indices, const int sample_size, TFunctor* functor_device) {
	int i_index = threadIdx.x + blockIdx.x * blockDim.x;
	(*functor_device)(data, sample_indices[i_index], i_index >= sample_size);
}


// endregion
} // end anonymous namespace: CUDA global kernels