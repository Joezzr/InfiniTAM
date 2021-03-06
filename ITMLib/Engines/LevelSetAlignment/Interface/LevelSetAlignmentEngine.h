//  ================================================================
//  Created by Gregory Kramida on 11/18/19.
//  Copyright (c) 2019 Gregory Kramida
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

//temporary
#include "LevelSetAlignmentParameters.h"
#include "LevelSetAlignmentEngineInterface.h"
#include "../Functors/WarpGradientFunctor.h"
#include "../../Warping/WarpingEngine.h"
#include "../../../Utils/Configuration/Configuration.h"
#include "../../../Utils/Enums/WarpType.h"


namespace ITMLib {


template<typename TVoxel, typename TWarp, typename TIndex, MemoryDeviceType TMemoryDeviceType, ExecutionMode TExecutionMode>
class LevelSetAlignmentEngine : public LevelSetAlignmentEngineInterface<TVoxel, TWarp, TIndex> {

private: // instance variables
	//TODO: for auto-completion in Clion, remove when CLion is fixed and this is no longer necessary
	const LevelSetAlignmentWeights& weights;
	const LevelSetAlignmentSwitches& switches;
	const LevelSetAlignmentTerminationConditions& termination;
	int iteration;

	WarpingEngineInterface<TVoxel, TWarp, TIndex>* warping_engine;
	// needs to be declared after "parameters", derives value from it during initialization
	const float vector_update_threshold_in_voxels;
	const bool log_settings = false;

public: // instance functions

	LevelSetAlignmentEngine();
	LevelSetAlignmentEngine(const LevelSetAlignmentSwitches& switches);
	LevelSetAlignmentEngine(const LevelSetAlignmentSwitches& switches, const LevelSetAlignmentTerminationConditions& termination_conditions);
	virtual ~LevelSetAlignmentEngine();


	VoxelVolume<TVoxel, TIndex>* Align(
			VoxelVolume<TWarp, TIndex>* warp_field,
			VoxelVolume<TVoxel, TIndex>** live_volume_pair,
			VoxelVolume<TVoxel, TIndex>* canonical_volume) override;

	VoxelVolume<TVoxel, TIndex>* Align(
			VoxelVolume<TWarp, TIndex>* warp_field,
			VoxelVolume<TVoxel, TIndex>** live_volume_pair,
			VoxelVolume<TVoxel, TIndex>* canonical_volume,
			bool& optimizationConverged) override;

	void ClearOutWarpUpdates(VoxelVolume<TWarp, TIndex>* warp_field) const;

protected: // instance functions
	void ClearOutFramewiseWarps(VoxelVolume<TWarp, TIndex>* warp_field) const;
	void ClearOutCumulativeWarps(VoxelVolume<TWarp, TIndex>* warp_field) const;

	void AddFramewiseWarpToWarp(VoxelVolume<TWarp, TIndex>* warp_field, bool clear_framewise_warps);

	void CalculateEnergyGradient(VoxelVolume<TWarp, TIndex>* warp_field, VoxelVolume<TVoxel, TIndex>* canonical_volume,
	                             VoxelVolume<TVoxel, TIndex>* live_volume);

	void SmoothEnergyGradient(VoxelVolume<TWarp, TIndex>* warp_field,
	                          VoxelVolume<TVoxel, TIndex>* canonical_volume,
	                          VoxelVolume<TVoxel, TIndex>* live_volume);

	void UpdateDeformationFieldUsingGradient(VoxelVolume<TWarp, TIndex>* warp_field,
	                                         VoxelVolume<TVoxel, TIndex>* canonical_volume,
	                                         VoxelVolume<TVoxel, TIndex>* live_volume);

	void PerformSingleOptimizationStep(VoxelVolume<TVoxel, TIndex>* canonical_volume,
	                                   VoxelVolume<TVoxel, TIndex>* source_live_volume,
	                                   VoxelVolume<TVoxel, TIndex>* target_live_volume,
	                                   VoxelVolume<TWarp, TIndex>* warp_field,
	                                   float& gradient_length_statistic_in_voxels);

	void UpdateGradientLengthStatistic(float& average_warp_length, VoxelVolume<TWarp, TIndex>* warp_field);

private: // instance functions

	void FindMaximumGradientLength(float& maximum_warp_length, Vector3i& position, VoxelVolume<TWarp, TIndex>* warp_field);
	void FindAverageGradientLength(float& average_warp_length, VoxelVolume<TWarp, TIndex>* warp_field);

	void PerformSingleOptimizationStep_Diagnostic(VoxelVolume<TVoxel, TIndex>* canonical_volume,
	                                              VoxelVolume<TVoxel, TIndex>* source_live_volume,
	                                              VoxelVolume<TVoxel, TIndex>* target_live_volume,
	                                              VoxelVolume<TWarp, TIndex>* warp_field,
	                                              float& gradient_length_statistic_in_voxels);

	void PerformSingleOptimizationStep_Optimized(VoxelVolume<TVoxel, TIndex>* canonical_volume,
	                                             VoxelVolume<TVoxel, TIndex>* source_live_volume,
	                                             VoxelVolume<TVoxel, TIndex>* target_live_volume,
	                                             VoxelVolume<TWarp, TIndex>* warp_field,
	                                             float& gradient_length_statistic_in_voxels);

	template<WarpType TWarpType>
	void ClearOutWarps(VoxelVolume<TWarp, TIndex>* warp_field) const;
	void LogSettings();


};


} //namespace ITMLib

