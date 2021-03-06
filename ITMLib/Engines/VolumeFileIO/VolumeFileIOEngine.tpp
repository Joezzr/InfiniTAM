//  ================================================================
//  Created by Gregory Kramida on 7/10/18.
//  Copyright (c) 2018-2000 Gregory Kramida
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

//boost
#include "../../../ORUtils/OStreamWrapper.h"
#include "../../../ORUtils/IStreamWrapper.h"

//local
#include "VolumeFileIOEngine.h"
#include "../Analytics/AnalyticsEngineFactory.h"

using namespace ITMLib;


// region ==================================== VOXEL BLOCK HASH ========================================================

template<typename TVoxel>
void VolumeFileIOEngine<TVoxel, VoxelBlockHash>::SaveVolumeCompact(
		const VoxelVolume<TVoxel, VoxelBlockHash>& volume,
		const std::string& path) {

	ORUtils::OStreamWrapper file(path, true);
	std::ostream& out_filter = file.OStream();

	bool temporary_volume_used = false;

	const VoxelVolume<TVoxel, VoxelBlockHash>* volume_to_save = &volume;

	if (volume.index.memory_type == MEMORYDEVICE_CUDA) {
		// we cannot access CUDA blocks directly, so the easiest solution here is to make a local main-memory copy first
		auto volume_cpu_copy = new VoxelVolume<TVoxel, VoxelBlockHash>(volume, MEMORYDEVICE_CPU);
		volume_to_save = volume_cpu_copy;
		temporary_volume_used = true;
	}

	const TVoxel* voxels = volume_to_save->GetVoxels();
	const HashEntry* hash_table = volume_to_save->index.GetEntries();

	int last_free_block_id = volume_to_save->index.GetLastFreeBlockListId();
	int last_excess_list_id = volume_to_save->index.GetLastFreeExcessListId();
	int utilized_block_count = volume_to_save->index.GetUtilizedBlockCount();
	int visible_block_count = volume_to_save->index.GetVisibleBlockCount();

	out_filter.write(reinterpret_cast<const char* >(&last_free_block_id), sizeof(int));
	out_filter.write(reinterpret_cast<const char* >(&last_excess_list_id), sizeof(int));
	out_filter.write(reinterpret_cast<const char* >(&utilized_block_count), sizeof(int));
	out_filter.write(reinterpret_cast<const char* >(&visible_block_count), sizeof(int));

	const int* utilized_hash_codes = volume_to_save->index.GetUtilizedBlockHashCodes();

	for (int i_utilized_hash_code = 0; i_utilized_hash_code < utilized_block_count; i_utilized_hash_code++) {
		const int hash_code = utilized_hash_codes[i_utilized_hash_code];
		const HashEntry& hash_entry = hash_table[hash_code];
		out_filter.write(reinterpret_cast<const char* >(&hash_code), sizeof(int));
		out_filter.write(reinterpret_cast<const char* >(&hash_entry), sizeof(HashEntry));
		const TVoxel* voxel_block = &(voxels[hash_entry.ptr * (VOXEL_BLOCK_SIZE3)]);
		out_filter.write(reinterpret_cast<const char* >(voxel_block), sizeof(TVoxel) * VOXEL_BLOCK_SIZE3);
	}

	const int* visible_hash_codes = volume_to_save->index.GetVisibleBlockHashCodes();
	for (int i_visible_hash_code = 0; i_visible_hash_code < visible_block_count; i_visible_hash_code++) {
		out_filter.write(reinterpret_cast<const char* >(visible_hash_codes + i_visible_hash_code), sizeof(int));
	}

	if (temporary_volume_used) {
		delete volume_to_save;
	}
}

template<typename TVoxel>
void
VolumeFileIOEngine<TVoxel, VoxelBlockHash>::LoadVolumeCompact(VoxelVolume<TVoxel, VoxelBlockHash>& volume,
                                                              const std::string& path) {
	ORUtils::IStreamWrapper file(path, true);
	std::istream& in_filter = file.IStream();

	VoxelVolume<TVoxel, VoxelBlockHash>* volume_to_load = &volume;
	bool temporary_volume_used = false;
	if (volume.index.memory_type == MEMORYDEVICE_CUDA) {
		// we cannot access CUDA blocks directly, so the easiest solution here is to make a local main (CPU) memory copy
		// first, read it in from disk, and then copy it over into the target
		auto scene_cpu_copy = new VoxelVolume<TVoxel, VoxelBlockHash>(*volume_to_load, MEMORYDEVICE_CPU);
		volume_to_load = scene_cpu_copy;
		temporary_volume_used = true;
	}

	TVoxel* voxel_blocks = volume_to_load->GetVoxels();
	HashEntry* hash_table = volume_to_load->index.GetEntries();

	int last_free_voxel_block_id, last_free_excess_list_id, utilized_block_count, visible_block_count;

	in_filter.read(reinterpret_cast<char* >(&last_free_voxel_block_id), sizeof(int));
	in_filter.read(reinterpret_cast<char* >(&last_free_excess_list_id), sizeof(int));
	in_filter.read(reinterpret_cast<char* >(&utilized_block_count), sizeof(int));
	in_filter.read(reinterpret_cast<char* >(&visible_block_count), sizeof(int));

	volume_to_load->index.SetLastFreeBlockListId(last_free_voxel_block_id);
	volume_to_load->index.SetLastFreeExcessListId(last_free_excess_list_id);
	volume_to_load->index.SetUtilizedBlockCount(utilized_block_count);
	volume_to_load->index.SetVisibleBlockCount(visible_block_count);

	int* utilized_hash_codes = volume_to_load->index.GetUtilizedBlockHashCodes();

	for (int i_utilized_hash_code = 0; i_utilized_hash_code < utilized_block_count; i_utilized_hash_code++) {
		int hash_code;
		in_filter.read(reinterpret_cast<char* >(&hash_code), sizeof(int));
		utilized_hash_codes[i_utilized_hash_code] = hash_code;
		HashEntry& hash_entry = hash_table[hash_code];
		in_filter.read(reinterpret_cast<char* >(&hash_entry), sizeof(HashEntry));
		TVoxel* voxel_block = &(voxel_blocks[hash_entry.ptr * (VOXEL_BLOCK_SIZE3)]);
		in_filter.read(reinterpret_cast<char* >(voxel_block), sizeof(TVoxel) * VOXEL_BLOCK_SIZE3);
	}

	int* visible_hash_codes = volume_to_load->index.GetVisibleBlockHashCodes();
	for (int i_visible_hash_code = 0; i_visible_hash_code < utilized_block_count; i_visible_hash_code++) {
		in_filter.read(reinterpret_cast<char* >(visible_hash_codes + i_visible_hash_code), sizeof(int));
	}

	if (temporary_volume_used) {
		volume.SetFrom(*volume_to_load);
		delete volume_to_load;
	}
}

template<typename TVoxel>
void VolumeFileIOEngine<TVoxel, VoxelBlockHash>::AppendFileWithUtilizedMemoryInformation(
		ORUtils::OStreamWrapper& file, const VoxelVolume<TVoxel, VoxelBlockHash>& volume) {
	std::vector<Vector3s> positions = Analytics_Accessor::Get<TVoxel, VoxelBlockHash>(volume.index.memory_type).GetUtilizedHashBlockPositions(&volume);
	size_t position_count = positions.size();
	file.OStream().write(reinterpret_cast<const char*>(&position_count), sizeof(size_t));
	for(auto& position : positions){
		file.OStream().write(reinterpret_cast<const char*>(&position), sizeof(position));
	}
}

// endregion ===========================================================================================================
// region ================================= PLAIN VOXEL ARRAY ==========================================================

template<typename TVoxel>
void
VolumeFileIOEngine<TVoxel, PlainVoxelArray>::SaveVolumeCompact(
		const VoxelVolume<TVoxel, PlainVoxelArray>& volume,
		const std::string& path) {
	ORUtils::OStreamWrapper file(path, true);
	volume.index.Save(file);
	volume.SaveVoxels(file);
}


template<typename TVoxel>
void
VolumeFileIOEngine<TVoxel, PlainVoxelArray>::LoadVolumeCompact(
		VoxelVolume<TVoxel, PlainVoxelArray>& volume,
		const std::string& path) {
	ORUtils::IStreamWrapper file(path, true);
	volume.index.Load(file);
	volume.LoadVoxels(file);
}

template<typename TVoxel>
void VolumeFileIOEngine<TVoxel, PlainVoxelArray>::AppendFileWithUtilizedMemoryInformation(
		ORUtils::OStreamWrapper& file, const VoxelVolume<TVoxel, PlainVoxelArray>& volume) {
	//FIXME
	DIEWITHEXCEPTION_REPORTLOCATION("Not yet implemented.");
}

// endregion ===========================================================================================================