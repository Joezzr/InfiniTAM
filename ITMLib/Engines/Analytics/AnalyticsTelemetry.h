//  ================================================================
//  Created by Gregory Kramida (https://github.com/Algomorph) on 4/18/20.
//  Copyright (c) $YEAR-2020 Gregory Kramida
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


//Boost
#include <boost/filesystem.hpp>

//local
#include "../VolumeFileIO/VolumeFileIOEngine.h"
#include "../../Objects/Volume/VoxelVolume.h"
#include "../../Utils/Configuration.h"


namespace fs = boost::filesystem;

namespace ITMLib {

//TODO: revise TelemetryRecorder and move all this functionality there
void InitializePerFrameAnalyticsTelemetry(ORUtils::OStreamWrapper** canonical_volume_memory_usage_file);


template<typename TVoxel, typename TIndex>
void RecordVolumeMemoryUsageInfo(ORUtils::OStreamWrapper& file, const VoxelVolume<TVoxel, TIndex>& volume) {
	if (configuration::get().telemetry_settings.record_volume_memory_usage) {
		VolumeFileIOEngine<TVoxel, TIndex>::AppendFileWithUtilizedMemoryInformation(file, volume);
	}
}


extern template void RecordVolumeMemoryUsageInfo<TSDFVoxel, VoxelBlockHash>(
		ORUtils::OStreamWrapper& file,
		const VoxelVolume<TSDFVoxel, VoxelBlockHash>& volume);

extern template void RecordVolumeMemoryUsageInfo<TSDFVoxel, PlainVoxelArray>(
		ORUtils::OStreamWrapper& file,
		const VoxelVolume<TSDFVoxel, PlainVoxelArray>& volume);

} // namespace ITMLib