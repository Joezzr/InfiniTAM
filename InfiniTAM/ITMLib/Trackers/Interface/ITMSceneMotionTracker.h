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

#include "../../TempDebugDefines.h"

//local
#include "../../Objects/Scene/ITMScene.h"
#include "../../Engines/Reconstruction/CPU/ITMSceneReconstructionEngine_CPU.h"
#include "../../Utils/ITMSceneSliceRasterizer.h"

namespace ITMLib {

template<typename TVoxelCanonical1, typename TVoxelLive1, typename TIndex1>
class ITMDenseDynamicMapper;

//TODO: write documentation block -Greg (Github: Algomorph)
template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
class ITMSceneMotionTracker {
	template<typename TVoxelCanonical1, typename TVoxelLive1, typename TIndex1>
	friend
	class ITMDenseDynamicMapper;
public:
//============================= CONSTRUCTORS / DESTRUCTORS =============================================================
//TODO: write documentation block -Greg (Github: Algomorph)
//TODO: simplify constructor to just accept the ITMLibSettings object and set the parameters from it

	ITMSceneMotionTracker(const ITMSceneParams& params, std::string scenePath,
	                      unsigned int maxIterationCount = 200,
	                      float maxVectorUpdateThresholdMeters = 0.0001f,
	                      float gradientDescentLearningRate = 0.0f,
	                      float rigidityEnforcementFactor = 0.1f,
	                      float weightSmoothnessTerm = 0.2f,
	                      float weightLevelSetTerm = 0.2f,
	                      float epsilon = FLT_EPSILON);
	ITMSceneMotionTracker(const ITMSceneParams& params, std::string scenePath, Vector3i focusCoordinates,
	                      unsigned int maxIterationCount = 200,
	                      float maxVectorUpdateThresholdMeters = 0.0001f,
	                      float gradientDescentLearningRate = 0.0f,
	                      float rigidityEnforcementFactor = 0.1f,
	                      float weightSmoothnessTerm = 0.2f,
	                      float weightLevelSetTerm = 0.2f,
	                      float epsilon = FLT_EPSILON);
	virtual ~ITMSceneMotionTracker();

//============================= MEMBER FUNCTIONS =======================================================================
	/**
	 * \brief Fuses the live scene into the canonical scene based on the motion warp of the canonical scene
	 * \details Typically called after TrackMotion is called
	 * \param canonicalScene the canonical voxel grid, representing the state at the beginning of the sequence
	 * \param liveScene the live voxel grid, a TSDF generated from a single recent depth image
	 */
	virtual void
	FuseFrame(ITMScene<TVoxelCanonical, TIndex>* canonicalScene, ITMScene<TVoxelLive, TIndex>* liveScene) = 0;

	/**
	 * \brief Warp canonical back to live
	 * \param canonicalScene
	 * \param liveScene
	 */
	virtual void
	WarpCanonicalToLive(ITMScene<TVoxelCanonical, TIndex>* canonicalScene, ITMScene<TVoxelLive, TIndex>* liveScene) = 0;

	void TrackMotion(
			ITMScene<TVoxelCanonical, TIndex>* canonicalScene, ITMScene<TVoxelLive, TIndex>*& liveScene,
			bool recordWarpUpdates);

	void SetUpStepByStepTracking(
			ITMScene<TVoxelCanonical, TIndex>* canonicalScene, ITMScene<TVoxelLive, TIndex>*& sourceLiveScene);
	bool UpdateTrackingSingleStep(
			ITMScene<TVoxelCanonical, TIndex>* canonicalScene, ITMScene<TVoxelLive, TIndex>*& sourceLiveScene);


	std::string GenerateCurrentFrameOutputPath() const;

	int GetFrameIndex() const { return currentFrameIx; }

protected:

	virtual float CalculateWarpUpdate(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                                  ITMScene<TVoxelLive, TIndex>* liveScene) = 0;//TODO: refactor to "CalculateWarpGradient"

	virtual void ApplySmoothingToGradient(
			ITMScene<TVoxelCanonical, TIndex>* canonicalScene, ITMScene<TVoxelLive, TIndex>* liveScene) = 0;
	virtual float ApplyWarpUpdateToWarp(
			ITMScene<TVoxelCanonical, TIndex>* canonicalScene, ITMScene<TVoxelLive, TIndex>* liveScene) = 0;

	virtual void ApplyWarpFieldToLive(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                                  ITMScene<TVoxelLive, TIndex>* sourceLiveScene)= 0;
	virtual void ApplyWarpUpdateToLive(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                                   ITMScene<TVoxelLive, TIndex>* sourceLiveScene) = 0;


	virtual void AllocateNewCanonicalHashBlocks(ITMScene<TVoxelCanonical, TIndex>* canonicalScene,
	                                            ITMScene<TVoxelLive, TIndex>* liveScene) = 0;

	virtual void ClearOutFrameWarps(
			ITMScene <TVoxelCanonical, TIndex>* canonicalScene) = 0;
	virtual void ApplyFrameWarpsToWarps(ITMScene<TVoxelCanonical, TIndex>* canonicalScene) = 0;


//============================= MEMBER VARIABLES =======================================================================
	//TODO -- make all of these parameters
	const unsigned int maxIterationCount = 200;
	const float maxVectorUpdateThresholdMeters = 0.0001f;//m //original for KillingFusion
	const float gradientDescentLearningRate = 0.1f;
	const float rigidityEnforcementFactor = 0.1f;
	const float weightSmoothnessTerm = 0.2f; //original for SobolevFusion
	const float weightLevelSetTerm = 0.2f;
	const float epsilon = FLT_EPSILON;

	float maxVectorUpdateThresholdVoxels;

	unsigned int iteration = 0;
	unsigned int currentFrameIx = 0;

	ITMSceneLogger<TVoxelCanonical, TVoxelLive, TIndex>* sceneLogger;
	std::string baseOutputDirectory;
	std::ofstream energy_stat_file;

	// variables for extra logging/analysis
	bool hasFocusCoordinates = false;
	Vector3i focusCoordinates;

	//TODO: these should be CLI parameters -Greg (GitHub:Algomorph)
	bool rasterizeLive = false;
	bool rasterizeCanonical = false;
	bool rasterizeUpdates = false;
	unsigned int rasterizationFrame = 1;

private:

	void InitializeUpdate2DImageLogging(
			ITMScene <TVoxelCanonical, TIndex>* canonicalScene, ITMScene <TVoxelLive, TIndex>* liveScene, cv::Mat& blank,
			cv::Mat& liveImgTemplate, ITMLib::ITMSceneSliceRasterizer<TVoxelCanonical, TVoxelLive, TIndex>& rasterizer);
	void LogWarpUpdateAs2DImage(
			ITMScene <TVoxelCanonical, TIndex>* canonicalScene, ITMScene <TVoxelLive, TIndex>* liveScene,
			const cv::Mat& blank, const cv::Mat& liveImgTemplate,
			ITMSceneSliceRasterizer <TVoxelCanonical, TVoxelLive, TIndex>& rasterizer);
	float maxVectorUpdate;
	bool inStepByStepProcessingMode = false;
};


}//namespace ITMLib



