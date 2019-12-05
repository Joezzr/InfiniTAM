// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#include "../../ITMLibDefines.h"
//Note: ".tpp" files have to be included for all explicit instantiations in order to link properly

#include "../../Core/ITMBasicEngine.tpp"
#include "../../Core/ITMBasicSurfelEngine.tpp"
#include "../../Core/ITMMultiEngine.tpp"
#include "../../Core/ITMDenseMapper.tpp"
#include "../../Core/ITMDenseSurfelMapper.tpp"
#include "../../Engines/Meshing/CPU/ITMMeshingEngine_CPU.tpp"
#include "../../Engines/Meshing/CPU/ITMMultiMeshingEngine_CPU.tpp"
#include "../../Engines/MultiScene/ITMMapGraphManager.tpp"
#include "../../Engines/Visualization/CPU/ITMMultiVisualizationEngine_CPU.tpp"
#include "../../Engines/Reconstruction/ITMSurfelSceneReconstructionEngineFactory.tpp"
#include "../../Engines/Reconstruction/CPU/ITMSceneReconstructionEngine_CPU.tpp"
#include "../../Engines/Reconstruction/CPU/ITMSurfelSceneReconstructionEngine_CPU.tpp"
#include "../../Engines/Reconstruction/Interface/ITMSurfelSceneReconstructionEngine.tpp"
#include "../../Engines/Swapping/CPU/ITMSwappingEngine_CPU.tpp"
#include "../../Engines/Visualization/ITMSurfelVisualizationEngineFactory.tpp"
#include "../../Engines/Visualization/CPU/ITMSurfelVisualisationEngine_CPU.tpp"
#include "../../Engines/Visualization/CPU/ITMVisualisationEngine_CPU.tpp"
#include "../../Engines/Visualization/Interface/ITMSurfelVisualisationEngine.tpp"
#include "../../Engines/Visualization/Interface/ITMVisualisationEngine.h"
#include "../../CameraTrackers/ITMCameraTrackerFactory.h"


namespace ITMLib
{
	//voxel fusion
	template class ITMBasicEngine<ITMVoxel, ITMVoxelBlockHash>;
	template class ITMBasicSurfelEngine<ITMSurfel_grey>;
	template class ITMBasicSurfelEngine<ITMSurfel_rgb>;
	template class ITMMultiEngine<ITMVoxel, ITMVoxelBlockHash>;
	template class ITMDenseMapper<ITMVoxel, ITMVoxelBlockHash>;
	template class ITMVoxelMapGraphManager<ITMVoxel, ITMVoxelBlockHash>;
	template class ITMVisualizationEngine_CPU<ITMVoxel, ITMVoxelBlockHash>;
	template class ITMMeshingEngine_CPU<ITMVoxel, ITMVoxelBlockHash>;
	template class ITMMultiMeshingEngine_CPU<ITMVoxel, ITMVoxelBlockHash>;
	template class ITMSwappingEngine_CPU<ITMVoxel, ITMVoxelBlockHash>;

	//surfel fusion
	template class ITMDenseSurfelMapper<ITMSurfel_grey>;
	template class ITMDenseSurfelMapper<ITMSurfel_rgb>;
	template class ITMSurfelSceneReconstructionEngine<ITMSurfel_grey>;
	template class ITMSurfelSceneReconstructionEngine<ITMSurfel_rgb>;
	template class ITMSurfelSceneReconstructionEngine_CPU<ITMSurfel_grey>;
	template class ITMSurfelSceneReconstructionEngine_CPU<ITMSurfel_rgb>;
	template struct ITMSurfelSceneReconstructionEngineFactory<ITMSurfel_grey>;
	template struct ITMSurfelSceneReconstructionEngineFactory<ITMSurfel_rgb>;
	template class ITMSurfelVisualisationEngine<ITMSurfel_grey>;
	template class ITMSurfelVisualisationEngine<ITMSurfel_rgb>;
	template class ITMSurfelVisualizationEngine_CPU<ITMSurfel_grey>;
	template class ITMSurfelVisualizationEngine_CPU<ITMSurfel_rgb>;
	template struct ITMSurfelVisualizationEngineFactory<ITMSurfel_grey>;
	template struct ITMSurfelVisualizationEngineFactory<ITMSurfel_rgb>;
}
