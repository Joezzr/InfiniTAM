//  ================================================================
//  Created by Gregory Kramida on 1/30/20.
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
#include "VolumeFusionEngine.h"
#include "VolumeFusionFunctors.h"
#include "../Traversal/Interface/TwoVolumeTraversal.h"

using namespace ITMLib;

template<typename TVoxel, typename TIndex, MemoryDeviceType TMemoryDeviceType>
void VolumeFusionEngine<TVoxel, TIndex, TMemoryDeviceType>::FuseOneTsdfVolumeIntoAnother(
		VoxelVolume<TVoxel, TIndex>* target_volume, VoxelVolume<TVoxel, TIndex>* source_volume,
		unsigned short timestamp) {
	if (this->parameters.use_surface_thickness_cutoff) {
		TSDFFusionFunctor<TVoxel, TMemoryDeviceType, true> fusion_functor(target_volume->GetParameters().max_integration_weight, timestamp,
		                                                                  negative_surface_thickness_sdf_scale);
		TwoVolumeTraversalEngine<TVoxel, TVoxel, TIndex, TIndex, TMemoryDeviceType>::
		TraverseUtilized(source_volume, target_volume, fusion_functor);
	} else {
		TSDFFusionFunctor<TVoxel, TMemoryDeviceType, false> fusion_functor(target_volume->GetParameters().max_integration_weight, timestamp,
		                                                                   negative_surface_thickness_sdf_scale);
		TwoVolumeTraversalEngine<TVoxel, TVoxel, TIndex, TIndex, TMemoryDeviceType>::
		TraverseUtilized(source_volume, target_volume, fusion_functor);
	}
}

template<typename TVoxel, typename TIndex, MemoryDeviceType TMemoryDeviceType>
VolumeFusionEngine<TVoxel, TIndex, TMemoryDeviceType>::VolumeFusionEngine()
		: negative_surface_thickness_sdf_scale(-this->parameters.surface_thickness /
		                                       (configuration::Get().general_voxel_volume_parameters.truncation_distance *
		                                        configuration::Get().general_voxel_volume_parameters.voxel_size)) {}
