//  ================================================================
//  Created by Gregory Kramida on 6/19/18.
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
#include "../Traversal/CUDA/ImageTraversal_CUDA.h"
#include "../Traversal/CUDA/TwoImageTraversal_CUDA.h"
#include "../Traversal/CUDA/HashTableTraversal_CUDA.h"
#include "../Traversal/CUDA/VolumeTraversal_CUDA_VoxelBlockHash.h"
#include "../Indexing/VBH/IndexingEngine_VoxelBlockHash.tpp"
#include "../Indexing/VBH/CUDA/IndexingEngine_CUDA_VoxelBlockHash.tcu"
#include "DepthFusionEngine.tpp"
#include "../../GlobalTemplateDefines.h"

namespace ITMLib {
template
class DepthFusionEngine<TSDFVoxel, WarpVoxel, VoxelBlockHash, MEMORYDEVICE_CUDA>;
}