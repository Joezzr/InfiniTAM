//  ================================================================
//  Created by Gregory Kramida on 10/18/17.
//  Copyright (c) 2017-2025 Gregory Kramida
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

#include <opencv2/core/mat.hpp>
#include "../Interface/ITMSceneMotionTracker.h"
#include "../../Utils/ITMHashBlockProperties.h"

namespace ITMLib {
template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
class ITMSceneMotionTracker_CPU :
		public ITMSceneMotionTracker<TVoxelCanonical, TVoxelLive, TIndex> {
public:

	explicit ITMSceneMotionTracker_CPU(const ITMSceneParams& params, std::string scenePath);
	virtual ~ITMSceneMotionTracker_CPU();
	void FuseFrame(ITMScene<TVoxelCanonical, TIndex>* canonicalScene, ITMScene<TVoxelLive, TIndex>* liveScene) override;
	void ApplyWarp(ITMScene<TVoxelCanonical, TIndex>* canonicalScene, ITMScene<TVoxelLive, TIndex>* liveScene) override;

protected:
	//START _DEBUG

	//timers
	double timeWarpUpdateCompute = 0.0;
	double timeDataJandHCompute = 0.0;
	double timeWarpJandHCompute = 0.0;
	double timeUpdateTermCompute = 0.0;
	double timeWarpUpdateApply = 0.0;

	//END _DEBUG

	float UpdateWarpField(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                      ITMScene<TVoxelLive, TIndex>* liveScene) override;

	void AllocateNewCanonicalHashBlocks(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                                    ITMScene<TVoxelLive, TIndex>* liveScene) override;


private:
	ORUtils::MemoryBlock<unsigned char>* warpedEntryAllocationType;
	ORUtils::MemoryBlock<unsigned char>* canonicalEntryAllocationTypes;
	ORUtils::MemoryBlock<Vector3s>* canonicalBlockCoords;

};


}//namespace ITMLib


