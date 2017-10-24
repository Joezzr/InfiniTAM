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


#include "../../Objects/Scene/ITMScene.h"

namespace ITMLib {
	template<class TVoxel, class TIndex>
	class ITMSceneMotionTracker {
	protected:

		virtual void DeformScene(ITMScene <TVoxel, TIndex>* sceneOld, ITMScene <TVoxel, TIndex>* sceneNew) = 0;

		virtual float UpdateWarpField(ITMScene <TVoxel, TIndex>* canonicalScene,
		                              ITMScene <TVoxel, TIndex>* liveScene) = 0;

	public:

		void ProcessFrame(ITMScene <TVoxel, TIndex>* canonicalScene,
		                  ITMScene <TVoxel, TIndex>* liveScene);
	};


}//namespace ITMLib



