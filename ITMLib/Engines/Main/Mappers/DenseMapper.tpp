// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#include "DenseMapper.h"

#include "../../Reconstruction/SceneReconstructionEngineFactory.h"
#include "../../Swapping/SwappingEngineFactory.h"

using namespace ITMLib;

template<class TVoxel, class TIndex>
DenseMapper<TVoxel, TIndex>::DenseMapper()
{
	auto& settings = configuration::Get();
	sceneRecoEngine = SceneReconstructionEngineFactory::MakeSceneReconstructionEngine<TVoxel,TIndex>(settings.device_type);
	swappingEngine = settings.swapping_mode != configuration::SWAPPINGMODE_DISABLED ?
	                 SwappingEngineFactory::Build<TVoxel, TIndex>(settings.device_type,
	                                                              configuration::ForVolumeRole<TIndex>(configuration::VOLUME_CANONICAL)) : nullptr;

	swappingMode = settings.swapping_mode;
}

template<class TVoxel, class TIndex>
DenseMapper<TVoxel,TIndex>::~DenseMapper()
{
	delete sceneRecoEngine;
	delete swappingEngine;
}

template<class TVoxel, class TIndex>
void DenseMapper<TVoxel,TIndex>::ProcessFrame(const View *view, const CameraTrackingState *trackingState, VoxelVolume<TVoxel,TIndex> *scene, RenderState *renderState)
{
	// allocation
	sceneRecoEngine->AllocateSceneFromDepth(scene, view, trackingState, renderState);

	// integration
	sceneRecoEngine->IntegrateIntoScene(scene, view, trackingState, renderState);

	if (swappingEngine != nullptr) {
		// swapping: CPU -> CUDA
		if (swappingMode == configuration::SWAPPINGMODE_ENABLED) swappingEngine->IntegrateGlobalIntoLocal(scene, renderState);

		// swapping: CUDA -> CPU
		switch (swappingMode)
		{
		case configuration::SWAPPINGMODE_ENABLED:
			swappingEngine->SaveToGlobalMemory(scene, renderState);
			break;
		case configuration::SWAPPINGMODE_DELETE:
			swappingEngine->CleanLocalMemory(scene, renderState);
			break;
		case configuration::SWAPPINGMODE_DISABLED:
			break;
		} 
	}
}

template<class TVoxel, class TIndex>
void DenseMapper<TVoxel,TIndex>::UpdateVisibleList(const View *view, const CameraTrackingState *trackingState, VoxelVolume<TVoxel,TIndex> *scene, RenderState *renderState, bool resetVisibleList)
{
	sceneRecoEngine->AllocateSceneFromDepth(scene, view, trackingState, renderState, true, resetVisibleList);
}
