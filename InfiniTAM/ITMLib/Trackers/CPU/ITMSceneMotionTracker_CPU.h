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

	explicit ITMSceneMotionTracker_CPU(const ITMSceneParams& params, std::string scenePath,
	                                   bool enableDataTerm = true,
	                                   bool enableLevelSetTerm = true,
	                                   bool enableKillingTerm = true);
	explicit ITMSceneMotionTracker_CPU(const ITMSceneParams& params, std::string scenePath, Vector3i focusCoordinates,
	                                   bool enableDataTerm = true,
	                                   bool enableLevelSetTerm = true,
	                                   bool enableKillingTerm = true);
	virtual ~ITMSceneMotionTracker_CPU();
	void FuseFrame(ITMScene<TVoxelCanonical, TIndex>* canonicalScene, ITMScene<TVoxelLive, TIndex>* liveScene) override;
	void WarpCanonicalToLive(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                         ITMScene<TVoxelLive, TIndex>* liveScene) override;

protected:

	const bool enableDataTerm;
	const bool enableLevelSetTerm;
	const bool enableSmoothingTerm;


	template<typename TWarpedPositionFunctor>
	void ApplyWarpVectorToLiveHelper(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                                 ITMScene<TVoxelLive, TIndex>* sourceLiveScene,
	                                 ITMScene<TVoxelLive, TIndex>* targetLiveScene);
	void ApplyWarpFieldToLive(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                          ITMScene<TVoxelLive, TIndex>* sourceLiveScene,
	                          ITMScene<TVoxelLive, TIndex>* targetLiveScene) override;




	float CalculateWarpUpdate(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                          ITMScene<TVoxelLive, TIndex>* liveScene) override;

	void ApplyWarpUpdateToLive(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                           ITMScene<TVoxelLive, TIndex>* sourceLiveScene,
	                           ITMScene<TVoxelLive, TIndex>* targetLiveScene) override;
	float ApplyWarpUpdateToWarp(ITMScene<TVoxelCanonical, TIndex>* canonicalScene) override;

	void AllocateNewCanonicalHashBlocks(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                                    ITMScene<TVoxelLive, TIndex>* liveScene) override;


private:


	float CalculateWarpUpdate_SingleThreadedVerbose(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                                                ITMScene<TVoxelLive, TIndex>* liveScene);
	float CalculateWarpUpdate_Multithreaded(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                                        ITMScene<TVoxelLive, TIndex>* liveScene);

	void InitializeHelper(const ITMLib::ITMSceneParams& sceneParams);
	ORUtils::MemoryBlock<unsigned char>* hashEntryAllocationTypes;
	ORUtils::MemoryBlock<unsigned char>* canonicalEntryAllocationTypes;
	ORUtils::MemoryBlock<Vector3s>* allocationBlockCoordinates;

	//BEGIN _DEBUG
	float maxWarpUpdateLength = 0.0f;
	float maxWarpLength = 0.0;

};


}//namespace ITMLib


