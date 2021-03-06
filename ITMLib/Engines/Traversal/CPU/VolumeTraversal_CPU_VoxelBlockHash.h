//  ================================================================
//  Created by Gregory Kramida on 5/22/18.
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

#pragma once

//local
#include "../Interface/VolumeTraversal.h"
#include "../../../Objects/Volume/VoxelVolume.h"
#include "../../EditAndCopy/CPU/EditAndCopyEngine_CPU.h"
#include "../Shared/VolumeTraversal_Shared.h"
#include "../../../Utils/Analytics/IsAltered.h"

namespace ITMLib {


//======================================================================================================================
//                            CONTAINS TRAVERSAL METHODS FOR VOLUMES USING VoxelBlockHash FOR INDEXING
//======================================================================================================================

//static-member-only classes are used here instead of namespaces to utilize template specialization (and maximize code reuse)
template<typename TVoxel>
class VolumeTraversalEngine<TVoxel, VoxelBlockHash, MEMORYDEVICE_CPU> {
private:

	template<typename TVoxel_Modifiers, typename TProcessFunction>
	inline static void
	TraverseBlock(TVoxel_Modifiers* voxel_block, TProcessFunction&& process_function) {
		for (int z = 0; z < VOXEL_BLOCK_SIZE; z++) {
			for (int y = 0; y < VOXEL_BLOCK_SIZE; y++) {
				for (int x = 0; x < VOXEL_BLOCK_SIZE; x++) {
					int locId = x + y * VOXEL_BLOCK_SIZE + z * VOXEL_BLOCK_SIZE * VOXEL_BLOCK_SIZE;
					TVoxel_Modifiers& voxel = voxel_block[locId];
					std::forward<TProcessFunction>(process_function)(voxel);
				}
			}
		}
	}

	template<typename TVoxel_Modifiers, typename TFunctor>
	inline static void
	TraverseBlockWithFunctor(TVoxel_Modifiers* voxel_block, TFunctor& functor) {
		TraverseBlock(voxel_block, [&functor](TVoxel_Modifiers& voxel) { functor(voxel); });
	}

	template<typename TVoxel_Modifiers, typename TProcessFunction>
	inline static void
	TraverseBlockWithinBounds(TVoxel_Modifiers* voxel_block, const Extent3Di& local_bounds, TProcessFunction&& process_function) {
		for (int z = local_bounds.min_z; z < local_bounds.max_z; z++) {
			for (int y = local_bounds.min_y; y < local_bounds.max_y; y++) {
				for (int x = local_bounds.min_x; x < local_bounds.max_x; x++) {
					int locId = x + y * VOXEL_BLOCK_SIZE + z * VOXEL_BLOCK_SIZE * VOXEL_BLOCK_SIZE;
					TVoxel_Modifiers& voxel = voxel_block[locId];
					std::forward<TProcessFunction>(process_function)(voxel);
				}
			}
		}
	}

	template<typename TVoxel_Modifiers, typename TFunctor>
	inline static void
	TraverseBlockWithinBoundsWithFunctor(TVoxel_Modifiers* voxel_block, const Extent3Di& local_bounds, TFunctor& functor) {
		TraverseBlockWithinBounds(voxel_block, local_bounds, [&functor](TVoxel_Modifiers& voxel) { functor(voxel); });
	}

	template<typename TVoxel_Modifiers, typename TProcessFunction>
	inline static void
	TraverseBlockWithPosition(TVoxel_Modifiers* voxel_block, const Vector3i& block_position_voxels,
	                          TProcessFunction&& process_function) {
		for (int z = 0; z < VOXEL_BLOCK_SIZE; z++) {
			for (int y = 0; y < VOXEL_BLOCK_SIZE; y++) {
				for (int x = 0; x < VOXEL_BLOCK_SIZE; x++) {
					int voxel_index_within_block = x + y * VOXEL_BLOCK_SIZE + z * VOXEL_BLOCK_SIZE * VOXEL_BLOCK_SIZE;
					Vector3i voxel_position = block_position_voxels + Vector3i(x, y, z);
					TVoxel_Modifiers& voxel = voxel_block[voxel_index_within_block];
					std::forward<TProcessFunction>(process_function)(voxel, voxel_position);
				}
			}
		}
	}

	template<typename TVoxel_Modifiers, typename TFunctor>
	inline static void
	TraverseBlockWithPositionWithFunctor(TVoxel_Modifiers* voxel_block, const Vector3i& block_position_voxels,
	                                     TFunctor& functor) {
		TraverseBlockWithPosition(
				voxel_block, block_position_voxels,
				[&functor](TVoxel_Modifiers& voxel, const Vector3i& voxel_position) {
					functor(voxel, voxel_position);
				}
		);
	}


	template<typename TVoxel_Modifiers, typename TProcessFunction>
	inline static void
	TraverseBlockWithinBoundsWithPosition(TVoxel_Modifiers* voxel_block, const Vector3i& block_position_voxels,
	                                      const Extent3Di& local_bounds, TProcessFunction&& process_function) {
		for (int z = local_bounds.min_z; z < local_bounds.max_z; z++) {
			for (int y = local_bounds.min_y; y < local_bounds.max_y; y++) {
				for (int x = local_bounds.min_x; x < local_bounds.max_x; x++) {
					int voxel_index_within_block = x + y * VOXEL_BLOCK_SIZE + z * VOXEL_BLOCK_SIZE * VOXEL_BLOCK_SIZE;
					Vector3i voxel_position = block_position_voxels + Vector3i(x, y, z);
					TVoxel_Modifiers& voxel = voxel_block[voxel_index_within_block];
					std::forward<TProcessFunction>(process_function)(voxel, voxel_position);
				}
			}
		}
	}

	template<typename TVoxel_Modifiers, typename TFunctor>
	inline static void
	TraverseBlockWithinBoundsWithPositionWithFunctor(TVoxel_Modifiers* voxel_block, const Vector3i& block_position_voxels,
	                                                 const Extent3Di& local_bounds, TFunctor& functor) {
		TraverseBlockWithinBoundsWithPosition(
				voxel_block, block_position_voxels, local_bounds,
				[&functor](TVoxel_Modifiers& voxel, const Vector3i& voxel_position) {
					functor(voxel, voxel_position);
				}
		);
	}

	template<typename TVoxel_Modifiers, typename TVolume, typename TProcessBlockFunction>
	inline static void
	TraverseAll_Generic(TVolume* volume, TProcessBlockFunction&& block_function) {
		TVoxel_Modifiers* voxels = volume->GetVoxels();
		const HashEntry* const hash_table = volume->index.GetEntries();
		const int hash_entry_count = volume->index.hash_entry_count;
#ifdef WITH_OPENMP
#pragma omp parallel for default(none) shared(block_function, voxels, hash_table) firstprivate(hash_entry_count)
#endif
		for (int hash_code = 0; hash_code < hash_entry_count; hash_code++) {
			const HashEntry& hash_entry = hash_table[hash_code];
			if (hash_entry.ptr < 0) continue;
			TVoxel_Modifiers* voxel_block = &(voxels[hash_entry.ptr * (VOXEL_BLOCK_SIZE3)]);
			std::forward<TProcessBlockFunction>(block_function)(voxel_block, hash_entry);
		}
	}

	template<typename TVoxel_Modifiers, typename TVolume, typename TProcessBlockFunction>
	inline static void
	TraverseUtilized_Generic(TVolume* volume, TProcessBlockFunction&& block_function) {
		TVoxel_Modifiers* voxels = volume->GetVoxels();
		const HashEntry* const hash_table = volume->index.GetEntries();
		const int utilized_entry_count = volume->index.GetUtilizedBlockCount();
		const int* utilized_hash_codes = volume->index.GetUtilizedBlockHashCodes();
#ifdef WITH_OPENMP
#pragma omp parallel for default(none) shared(block_function, voxels, hash_table, utilized_hash_codes) firstprivate(utilized_entry_count)
#endif
		for (int hash_code_index = 0; hash_code_index < utilized_entry_count; hash_code_index++) {
			int hash_code = utilized_hash_codes[hash_code_index];
			const HashEntry& hash_entry = hash_table[hash_code];
			TVoxel_Modifiers* voxel_block = &(voxels[hash_entry.ptr * (VOXEL_BLOCK_SIZE3)]);
			std::forward<TProcessBlockFunction>(block_function)(voxel_block, hash_entry);
		}
	}

public:
// region ================================ STATIC SINGLE-VOLUME TRAVERSAL ===============================================
	template<typename TStaticFunctor>
	inline static void
	TraverseAll(VoxelVolume <TVoxel, VoxelBlockHash>* volume) {
		TraverseAll_Generic<TVoxel>(
				volume,
				[](TVoxel* voxel_block, const HashEntry& hash_entry) {
					TraverseBlock(voxel_block, [](TVoxel& voxel) {
						TStaticFunctor::run(voxel);
					});
				}
		);
	}

	template<typename TStaticFunctor>
	inline static void
	TraverseAll(const VoxelVolume <TVoxel, VoxelBlockHash>* volume) {
		TraverseAll_Generic<const TVoxel>(
				volume,
				[](const TVoxel* voxel_block, const HashEntry& hash_entry) {
					TraverseBlock(voxel_block, [](const TVoxel& voxel) {
						TStaticFunctor::run(voxel);
					});
				}
		);
	}

	template<typename TStaticFunctor>
	inline static void
	TraverseUtilized(VoxelVolume <TVoxel, VoxelBlockHash>* volume) {
		TraverseUtilized_Generic<TVoxel>(
				volume,
				[](TVoxel* voxel_block, const HashEntry& hash_entry) {
					TraverseBlock(voxel_block, [](TVoxel& voxel) {
						TStaticFunctor::run(voxel);
					});
				}
		);
	}


	template<typename TStaticFunctor>
	inline static void
	TraverseUtilized(const VoxelVolume <TVoxel, VoxelBlockHash>* volume) {
		TraverseUtilized_Generic<const TVoxel>(
				volume,
				[](const TVoxel* voxel_block, const HashEntry& hash_entry) {
					TraverseBlock(voxel_block, [](const TVoxel& voxel) {
						TStaticFunctor::run(voxel);
					});
				}
		);
	}

	template<typename TStaticFunctor>
	inline static void
	TraverseAllWithPosition(VoxelVolume <TVoxel, VoxelBlockHash>* volume) {
		TraverseAll_Generic<TVoxel>(
				volume,
				[](TVoxel* voxel_block, const HashEntry& hash_entry) {
					Vector3i block_position = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
					TraverseBlockWithPosition(voxel_block, block_position, [](TVoxel& voxel) {
						TStaticFunctor::run(voxel);
					});
				}
		);
	}

	template<typename TStaticFunctor>
	inline static void
	TraverseAllWithPosition(const VoxelVolume <TVoxel, VoxelBlockHash>* volume) {
		TraverseAll_Generic<const TVoxel>(
				volume,
				[](const TVoxel* voxel_block, const HashEntry& hash_entry) {
					Vector3i block_position = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
					TraverseBlockWithPosition(voxel_block, block_position, [](const TVoxel& voxel) {
						TStaticFunctor::run(voxel);
					});
				}
		);
	}

	template<typename TStaticFunctor>
	inline static void
	TraverseUtilizedWithPosition(VoxelVolume <TVoxel, VoxelBlockHash>* volume) {
		TraverseUtilized_Generic<TVoxel>(
				volume,
				[](TVoxel* voxel_block, const HashEntry& hash_entry) {
					Vector3i block_position = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
					TraverseBlockWithPosition(voxel_block, block_position, [](TVoxel& voxel) {
						TStaticFunctor::run(voxel);
					});
				}
		);
	}

	template<typename TStaticFunctor>
	inline static void
	TraverseUtilizedWithPosition(const VoxelVolume <TVoxel, VoxelBlockHash>* volume) {
		TraverseUtilized_Generic<const TVoxel>(
				volume,
				[](const TVoxel* voxel_block, const HashEntry& hash_entry) {
					Vector3i block_position = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
					TraverseBlockWithPosition(voxel_block, block_position, [](const TVoxel& voxel) {
						TStaticFunctor::run(voxel);
					});
				}
		);
	}
// endregion
// region ================================ DYNAMIC SINGLE-VOLUME TRAVERSAL ==============================================

	template<typename TFunctor>
	inline static void
	TraverseAll(VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor) {
		TraverseAll_Generic<TVoxel>(
				volume, [&functor](TVoxel* voxel_block, const HashEntry& hash_entry) {
					TraverseBlockWithFunctor(voxel_block, functor);
				}
		);
	}

	template<typename TFunctor>
	inline static void
	TraverseAll(const VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor) {
		TraverseAll_Generic<const TVoxel>(
				volume, [&functor](const TVoxel* voxel_block, const HashEntry& hash_entry) {
					TraverseBlockWithFunctor(voxel_block, functor);
				}
		);
	}

	template<typename TFunctor>
	inline static void
	TraverseUtilized(VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor) {
		TraverseUtilized_Generic<TVoxel>(
				volume, [&functor](TVoxel* voxel_block, const HashEntry& hash_entry) {
					TraverseBlockWithFunctor(voxel_block, functor);
				}
		);
	}

	template<typename TFunctor>
	inline static void
	TraverseUtilized(const VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor) {
		TraverseUtilized_Generic<const TVoxel>(
				volume, [&functor](const TVoxel* voxel_block, const HashEntry& hash_entry) {
					TraverseBlockWithFunctor(voxel_block, functor);
				}
		);
	}
//TODO: remove
	template<typename TFunctor>
	inline static void
	TraverseAll_ST(VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor) {
		TVoxel* voxels = volume->GetVoxels();
		const HashEntry* hash_table = volume->index.GetEntries();
		int hash_entry_count = volume->index.hash_entry_count;
		for (int hash_code = 0; hash_code < hash_entry_count; hash_code++) {
			const HashEntry& hash_entry = hash_table[hash_code];
			if (hash_entry.ptr < 0) continue;
			TVoxel* voxel_block = &(voxels[hash_entry.ptr * (VOXEL_BLOCK_SIZE3)]);
			TraverseBlockWithFunctor(voxel_block, functor);
		}
	}
//TODO: remove
	template<typename TFunctor>
	inline static void
	TraverseUtilized_ST(VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor) {
		TVoxel* const voxels = volume->GetVoxels();
		const HashEntry* const hash_table = volume->index.GetEntries();
		const int utilized_entry_count = volume->index.GetUtilizedBlockCount();
		const int* utilized_hash_codes = volume->index.GetUtilizedBlockHashCodes();
		for (int hash_code_index = 0; hash_code_index < utilized_entry_count; hash_code_index++) {
			const HashEntry& hash_entry = hash_table[utilized_hash_codes[hash_code_index]];
			TVoxel* voxel_block = &(voxels[hash_entry.ptr * (VOXEL_BLOCK_SIZE3)]);
			TraverseBlockWithFunctor(voxel_block, functor);
		}
	}

	template<typename TFunctor>
	inline static void
	TraverseAllWithPosition(VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor) {
		TraverseAll_Generic<TVoxel>(
				volume, [&functor](TVoxel* voxel_block, const HashEntry& hash_entry) {
					Vector3i block_position = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
					TraverseBlockWithPositionWithFunctor(voxel_block, block_position, functor);
				}
		);
	}

	template<typename TFunctor>
	inline static void
	TraverseAllWithPosition(const VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor) {
		TraverseAll_Generic<const TVoxel>(
				volume, [&functor](const TVoxel* voxel_block, const HashEntry& hash_entry) {
					Vector3i block_position = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
					TraverseBlockWithPositionWithFunctor(voxel_block, block_position, functor);
				}
		);
	}

	template<typename TFunctor>
	inline static void
	TraverseUtilizedWithPosition(VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor) {
		TraverseUtilized_Generic<TVoxel>(
				volume, [&functor](TVoxel* voxel_block, const HashEntry& hash_entry) {
					Vector3i block_position = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
					TraverseBlockWithPositionWithFunctor(voxel_block, block_position, functor);
				}
		);
	}

	template<typename TFunctor>
	inline static void
	TraverseUtilizedWithPosition(const VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor) {
		TraverseUtilized_Generic<const TVoxel>(
				volume, [&functor](const TVoxel* voxel_block, const HashEntry& hash_entry) {
					Vector3i block_position = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
					TraverseBlockWithPositionWithFunctor(voxel_block, block_position, functor);
				}
		);
	}

	template<typename TFunctor>
	inline static void
	TraverseAllWithinBounds(VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor,
	                        const Vector6i& bounds) {
		TraverseAll_Generic<TVoxel>(
				volume, [&functor, &bounds](TVoxel* voxel_block, const HashEntry& hash_entry) {
					Vector3i hash_entry_min = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
					Vector3i hash_entry_max = hash_entry_min + Vector3i(VOXEL_BLOCK_SIZE);
					if (!HashBlockDoesNotIntersectBounds(hash_entry_min, hash_entry_max, bounds)) {
						Vector6i local_bounds = ComputeLocalBounds(hash_entry_min, hash_entry_max, bounds);
						TraverseBlockWithinBoundsWithFunctor(voxel_block, local_bounds, functor);
					}
				}
		);
	}


	template<typename TFunctor>
	inline static void
	TraverseAllWithinBoundsWithPosition(VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor,
	                                    Vector6i bounds) {
		TraverseAll_Generic<TVoxel>(
				volume, [&functor, &bounds](TVoxel* voxel_block, const HashEntry& hash_entry) {
					Vector3i hash_entry_min = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
					Vector3i hash_entry_max = hash_entry_min + Vector3i(VOXEL_BLOCK_SIZE);
					if (!HashBlockDoesNotIntersectBounds(hash_entry_min, hash_entry_max, bounds)) {
						Vector3i block_position_voxels = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
						Vector6i local_bounds = ComputeLocalBounds(hash_entry_min, hash_entry_max, bounds);
						TraverseBlockWithinBoundsWithPositionWithFunctor(voxel_block, block_position_voxels,
						                                                 local_bounds, functor);
					}
				}
		);
	}

	template<typename TFunctor>
	inline static void
	TraverseAllWithinBoundsWithPositionAndHashEntry(VoxelVolume <TVoxel, VoxelBlockHash>* volume, TFunctor& functor,
	                                                Vector6i bounds) {
		TraverseAll_Generic<TVoxel>(
				volume, [&functor, &bounds](TVoxel* voxel_block, const HashEntry& hash_entry) {
					Vector3i hash_entry_min = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
					Vector3i hash_entry_max = hash_entry_min + Vector3i(VOXEL_BLOCK_SIZE);
					if (!HashBlockDoesNotIntersectBounds(hash_entry_min, hash_entry_max, bounds)) {
						Vector6i local_bounds = ComputeLocalBounds(hash_entry_min, hash_entry_max, bounds);
						functor.processHashEntry(hash_entry);
						Vector3i block_position_voxels = hash_entry.pos.toInt() * VOXEL_BLOCK_SIZE;
						TraverseBlockWithinBoundsWithPositionWithFunctor(voxel_block, block_position_voxels,
						                                                 local_bounds,
						                                                 functor);
					}
				}
		);
	}
// endregion
};

}//namespace ITMLib