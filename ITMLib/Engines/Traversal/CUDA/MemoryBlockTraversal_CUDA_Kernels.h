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
//local
#include "../../../Utils/Math.h"
#include "../../../Objects/Volume/VoxelBlockHash.h"

namespace {
// CUDA global kernels

template<typename TData, typename TFunctor>
__global__ void memoryBlockTraversalWithItemIndex_device(TData* data, const unsigned int element_count, TFunctor* functor_device){
	unsigned int i_item = threadIdx.x + blockIdx.x * blockDim.x;
	if (i_item >= element_count) return;
	(*functor_device)(data[i_item], i_item);
}

} // end anonymous namespace: CUDA global kernels