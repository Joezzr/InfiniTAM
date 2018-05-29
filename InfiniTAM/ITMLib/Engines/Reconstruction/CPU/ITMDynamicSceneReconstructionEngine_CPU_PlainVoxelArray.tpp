//  ================================================================
//  Created by Gregory Kramida on 5/28/18.
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

#include "ITMDynamicSceneReconstructionEngine_CPU.h"
#include "../Shared/ITMDynamicSceneReconstructionEngine_Shared.h"

using namespace ITMLib;

// region ========================================= PLAIN VOXEL ARRAY ==================================================
template<typename TVoxelCanonical, typename TVoxelLive>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMPlainVoxelArray>::IntegrateIntoScene(
		ITMScene<TVoxelLive, ITMPlainVoxelArray>* scene, const ITMView* view,
		const ITMTrackingState* trackingState, const ITMRenderState* renderState) {
	Vector2i rgbImgSize = view->rgb->noDims;
	Vector2i depthImgSize = view->depth->noDims;
	float voxelSize = scene->sceneParams->voxelSize;

	Matrix4f M_d, M_rgb;
	Vector4f projParams_d, projParams_rgb;

	M_d = trackingState->pose_d->GetM();
	if (TVoxelLive::hasColorInformation) M_rgb = view->calib.trafo_rgb_to_depth.calib_inv * M_d;

	projParams_d = view->calib.intrinsics_d.projectionParamsSimple.all;
	projParams_rgb = view->calib.intrinsics_rgb.projectionParamsSimple.all;

	float mu = scene->sceneParams->mu;
	int maxW = scene->sceneParams->maxW;

	float* depth = view->depth->GetData(MEMORYDEVICE_CPU);
	Vector4u* rgb = view->rgb->GetData(MEMORYDEVICE_CPU);
	TVoxelLive* voxelArray = scene->localVBA.GetVoxelBlocks();

	const ITMPlainVoxelArray::IndexData* arrayInfo = scene->index.getIndexData();

	bool stopIntegratingAtMaxW = scene->sceneParams->stopIntegratingAtMaxW;

#ifdef WITH_OPENMP
#pragma omp parallel for
#endif
	for (int locId = 0; locId < scene->index.getVolumeSize().x * scene->index.getVolumeSize().y *
	                            scene->index.getVolumeSize().z; ++locId) {
		int z = locId / (scene->index.getVolumeSize().x * scene->index.getVolumeSize().y);
		int tmp = locId - z * scene->index.getVolumeSize().x * scene->index.getVolumeSize().y;
		int y = tmp / scene->index.getVolumeSize().x;
		int x = tmp - y * scene->index.getVolumeSize().x;

		Vector4f pt_model;

		if (stopIntegratingAtMaxW) if (voxelArray[locId].w_depth == maxW) continue;

		pt_model.x = (float) (x + arrayInfo->offset.x) * voxelSize;
		pt_model.y = (float) (y + arrayInfo->offset.y) * voxelSize;
		pt_model.z = (float) (z + arrayInfo->offset.z) * voxelSize;
		pt_model.w = 1.0f;

		ComputeUpdatedLiveVoxelInfo<
				TVoxelLive::hasColorInformation,
				TVoxelLive::hasConfidenceInformation,
				TVoxelLive::hasSemanticInformation,
				TVoxelLive>::compute(voxelArray[locId], pt_model, M_d, projParams_d, M_rgb, projParams_rgb, mu, maxW,
		                             depth, depthImgSize, rgb, rgbImgSize);
	}
}

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMPlainVoxelArray>::FuseFrame(
		ITMScene<TVoxelCanonical, ITMPlainVoxelArray>* canonicalScene,
		ITMScene<TVoxelLive, ITMPlainVoxelArray>* liveScene, int liveSourceFieldIndex) {
	DIEWITHEXCEPTION_REPORTLOCATION("Not implemented");//TODO
}

template<typename TVoxelCanonical, typename TVoxelLive>
void
ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMPlainVoxelArray>::GenerateRawLiveSceneFromView(
		ITMScene<TVoxelLive, ITMPlainVoxelArray>* scene, const ITMView* view, const ITMTrackingState* trackingState,
		const ITMRenderState* renderState) {
	liveSceneManager.ResetScene(scene);
	this->IntegrateIntoScene(scene,view,trackingState,renderState);
}

template<typename TVoxelCanonical, typename TVoxelLive>
void ITMDynamicSceneReconstructionEngine_CPU<TVoxelCanonical, TVoxelLive, ITMPlainVoxelArray>::UpdateVisibleList(
		ITMScene<TVoxelLive, ITMPlainVoxelArray>* scene, const ITMView* view, const ITMTrackingState* trackingState,
		const ITMRenderState* renderState, bool resetVisibleList) {
	//do nothing
}
// endregion ===========================================================================================================