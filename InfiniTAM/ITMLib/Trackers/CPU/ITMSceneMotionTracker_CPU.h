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
#include "../../Utils/ITMLibHashBlockProperties.h"

namespace ITMLib {
	template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
	class ITMSceneMotionTracker_CPU :
			public ITMSceneMotionTracker<TVoxelCanonical, TVoxelLive, TIndex> {
	public:

		explicit ITMSceneMotionTracker_CPU(const ITMSceneParams& params);
		virtual ~ITMSceneMotionTracker_CPU();

	protected:
		//START _DEBUG

		//timers
		double timeWarpUpdateCompute = 0.0;
		double timeDataJandHCompute = 0.0;
		double timeWarpJandHCompute = 0.0;
		double timeUpdateTermCompute = 0.0;
		double timeWarpUpdateApply = 0.0;

		//END _DEBUG

		float
		UpdateWarpField(ITMScene <TVoxelCanonical, TIndex>* canonicalScene, ITMScene <TVoxelLive, TIndex>* liveScene) override;

		void AllocateBoundaryHashBlocks(ITMScene <TVoxelCanonical, TIndex>* canonicalScene,
				                                ITMScene <TVoxelLive, TIndex>* liveScene) override;


		void FuseFrame(ITMScene <TVoxelCanonical, TIndex>* canonicalScene, ITMScene <TVoxelLive, TIndex>* liveScene) override;

	private:
		template <typename TVoxel>
		void AllocateBoundaryHashBlocks(ITMScene <TVoxel, TIndex>* scene, uchar* entriesAllocType);

		ORUtils::MemoryBlock<bool> *entriesAllocFill;
		ORUtils::MemoryBlock<unsigned char> *canonicalEntriesAllocType;
		ORUtils::MemoryBlock<unsigned char> *liveEntriesAllocType;
		ORUtils::MemoryBlock<Vector3s> *blockCoords;

	};


}//namespace ITMLib


