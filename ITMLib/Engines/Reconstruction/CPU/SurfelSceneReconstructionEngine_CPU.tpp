// InfiniTAM: Surffuse. Copyright (c) Torr Vision Group and the authors of InfiniTAM, 2016.

#include "SurfelSceneReconstructionEngine_CPU.h"

#include "../Shared/SurfelSceneReconstructionEngine_Shared.h"

namespace ITMLib
{

//#################### CONSTRUCTORS ####################

template <typename TSurfel>
SurfelSceneReconstructionEngine_CPU<TSurfel>::SurfelSceneReconstructionEngine_CPU(const Vector2i& depthImageSize)
: SurfelSceneReconstructionEngine<TSurfel>(depthImageSize)
{}

//#################### PRIVATE MEMBER FUNCTIONS ####################

template <typename TSurfel>
void SurfelSceneReconstructionEngine_CPU<TSurfel>::AddNewSurfels(SurfelScene<TSurfel> *scene, const View *view, const CameraTrackingState *trackingState) const
{
  // Calculate the prefix sum of the new points mask.
  const unsigned short *newPointsMask = this->m_newPointsMaskMB->GetData(MEMORYDEVICE_CPU);
  unsigned int *newPointsPrefixSum = this->m_newPointsPrefixSumMB->GetData(MEMORYDEVICE_CPU);
  const int pixelCount = static_cast<int>(this->m_newPointsMaskMB->size() - 1);

  newPointsPrefixSum[0] = 0;
  for(int i = 1; i <= pixelCount; ++i)
  {
    newPointsPrefixSum[i] = newPointsPrefixSum[i-1] + newPointsMask[i-1];
  }

  // Add the new surfels to the scene.
  const size_t newSurfelCount = static_cast<size_t>(newPointsPrefixSum[pixelCount]);
  TSurfel *newSurfels = scene->AllocateSurfels(newSurfelCount);
  if(newSurfels == nullptr) return;

  const Vector4u *colourMap = view->rgb.GetData(MEMORYDEVICE_CPU);
  const Matrix4f& depthToRGB = view->calibration_information.trafo_rgb_to_depth.calib_inv;
  const Vector3f *normalMap = this->m_normalMapMB->GetData(MEMORYDEVICE_CPU);
  const Vector4f& projParamsRGB = view->calibration_information.intrinsics_rgb.projectionParamsSimple.all;
  const float *radiusMap = this->m_radiusMapMB->GetData(MEMORYDEVICE_CPU);
  const SurfelVolumeParameters& sceneParams = scene->GetParams();
  const Matrix4f T = trackingState->pose_d->GetInvM();
  const Vector4f *vertexMap = this->m_vertexMapMB->GetData(MEMORYDEVICE_CPU);

#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int locId = 0; locId < pixelCount; ++locId)
  {
    add_new_surfel(
		    locId, T, this->m_timestamp, newPointsMask, newPointsPrefixSum, vertexMap, normalMap, radiusMap, colourMap,
		    view->depth.dimensions.x, view->depth.dimensions.y, view->rgb.dimensions.x, view->rgb.dimensions.y,
		    depthToRGB, projParamsRGB, sceneParams.use_gaussian_sample_confidence, sceneParams.gaussian_confidence_sigma,
		    sceneParams.max_surfel_radius, newSurfels
    );
  }
}

template <typename TSurfel>
void SurfelSceneReconstructionEngine_CPU<TSurfel>::FindCorrespondingSurfels(const SurfelScene<TSurfel> *scene, const View *view, const CameraTrackingState *trackingState,
                                                                            const SurfelRenderState *renderState) const
{
  unsigned int *correspondenceMap = this->m_correspondenceMapMB->GetData(MEMORYDEVICE_CPU);
  const float *depthMap = view->depth.GetData(MEMORYDEVICE_CPU);
  const int depthMapWidth = view->depth.dimensions.x;
  const unsigned int *indexImageSuper = renderState->GetIndexImageSuper()->GetData(MEMORYDEVICE_CPU);
  const Matrix4f& invT = trackingState->pose_d->GetM();
  unsigned short *newPointsMask = this->m_newPointsMaskMB->GetData(MEMORYDEVICE_CPU);
  const Vector3f *normalMap = this->m_normalMapMB->GetData(MEMORYDEVICE_CPU);
  const int pixelCount = static_cast<int>(view->depth.size());
  const SurfelVolumeParameters& sceneParams = scene->GetParams();
  const TSurfel *surfels = scene->GetSurfels()->GetData(MEMORYDEVICE_CPU);

#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int locId = 0; locId < pixelCount; ++locId)
  {
    find_corresponding_surfel(locId, invT, depthMap, depthMapWidth, normalMap, indexImageSuper, sceneParams.supersampling_factor, surfels, correspondenceMap, newPointsMask);
  }
}

template <typename TSurfel>
void SurfelSceneReconstructionEngine_CPU<TSurfel>::FuseMatchedPoints(SurfelScene<TSurfel> *scene, const View *view, const CameraTrackingState *trackingState) const
{
  const Vector4u *colourMap = view->rgb.GetData(MEMORYDEVICE_CPU);
  const int colourMapHeight = view->rgb.dimensions.y;
  const int colourMapWidth = view->rgb.dimensions.x;
  const int depthMapHeight = view->depth.dimensions.y;
  const int depthMapWidth = view->depth.dimensions.x;
  const unsigned int *correspondenceMap = this->m_correspondenceMapMB->GetData(MEMORYDEVICE_CPU);
  const Matrix4f& depthToRGB = view->calibration_information.trafo_rgb_to_depth.calib_inv;
  const Vector3f *normalMap = this->m_normalMapMB->GetData(MEMORYDEVICE_CPU);
  const int pixelCount = static_cast<int>(view->depth.size());
  const Vector4f& projParamsRGB = view->calibration_information.intrinsics_rgb.projectionParamsSimple.all;
  const float *radiusMap = this->m_radiusMapMB->GetData(MEMORYDEVICE_CPU);
  const SurfelVolumeParameters& sceneParams = scene->GetParams();
  TSurfel *surfels = scene->GetSurfels()->GetData(MEMORYDEVICE_CPU);
  const Matrix4f T = trackingState->pose_d->GetInvM();
  const Vector4f *vertexMap = this->m_vertexMapMB->GetData(MEMORYDEVICE_CPU);

#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int locId = 0; locId < pixelCount; ++locId)
  {
    fuse_matched_point(
      locId, correspondenceMap, T, this->m_timestamp, vertexMap, normalMap, radiusMap, colourMap, depthMapWidth, depthMapHeight, colourMapWidth, colourMapHeight,
      depthToRGB, projParamsRGB, sceneParams.delta_radius, sceneParams.use_gaussian_sample_confidence, sceneParams.gaussian_confidence_sigma, sceneParams.max_surfel_radius,
      surfels
    );
  }
}

template <typename TSurfel>
void SurfelSceneReconstructionEngine_CPU<TSurfel>::MarkBadSurfels(SurfelScene<TSurfel> *scene) const
{
  const int surfelCount = static_cast<int>(scene->GetSurfelCount());

  // If the scene is empty, early out.
  if(surfelCount == 0) return;

  const SurfelVolumeParameters& sceneParams = scene->GetParams();
  unsigned int *surfelRemovalMask = this->m_surfelRemovalMaskMB->GetData(MEMORYDEVICE_CPU);
  const TSurfel *surfels = scene->GetSurfels()->GetData(MEMORYDEVICE_CPU);

  // Clear the surfel removal mask.
#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int surfelId = 0; surfelId < surfelCount; ++surfelId)
  {
    clear_removal_mask_entry(surfelId, surfelRemovalMask);
  }

  // Mark long-term unstable surfels for removal.
#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int surfelId = 0; surfelId < surfelCount; ++surfelId)
  {
    mark_for_removal_if_unstable(
      surfelId,
      surfels,
      this->m_timestamp,
      sceneParams.stable_surfel_confidence,
      sceneParams.unstable_surfel_period,
      surfelRemovalMask
    );
  }
}

template <typename TSurfel>
void SurfelSceneReconstructionEngine_CPU<TSurfel>::MergeSimilarSurfels(SurfelScene<TSurfel> *scene, const SurfelRenderState *renderState) const
{
  const unsigned int *correspondenceMap = this->m_correspondenceMapMB->GetData(MEMORYDEVICE_CPU);
  const unsigned int *indexImage = renderState->GetIndexImage()->GetData(MEMORYDEVICE_CPU);
  const int indexImageHeight = renderState->GetIndexImage()->dimensions.y;
  const int indexImageWidth = renderState->GetIndexImage()->dimensions.x;
  unsigned int *mergeTargetMap = this->m_mergeTargetMapMB->GetData(MEMORYDEVICE_CPU);
  const int pixelCount = static_cast<int>(renderState->GetIndexImage()->size());
  const SurfelVolumeParameters& sceneParams = scene->GetParams();
  TSurfel *surfels = scene->GetSurfels()->GetData(MEMORYDEVICE_CPU);
  unsigned int *surfelRemovalMask = this->m_surfelRemovalMaskMB->GetData(MEMORYDEVICE_CPU);

  // Clear the merge target map.
#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int locId = 0; locId < pixelCount; ++locId)
  {
    clear_merge_target(locId, mergeTargetMap);
  }

  // Find pairs of surfels that can be merged.
#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int locId = 0; locId < pixelCount; ++locId)
  {
    find_mergeable_surfel(
      locId, indexImage, indexImageWidth, indexImageHeight, correspondenceMap, surfels,
      sceneParams.stable_surfel_confidence, sceneParams.max_merge_dist, sceneParams.max_merge_angle,
      sceneParams.min_radius_overlap_factor, mergeTargetMap
    );
  }

  // Prevent any merge chains.
#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int locId = 0; locId < pixelCount; ++locId)
  {
    prevent_merge_chain(locId, mergeTargetMap);
  }

  // Merge the relevant surfels.
#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int locId = 0; locId < pixelCount; ++locId)
  {
    perform_surfel_merge(locId, mergeTargetMap, surfels, surfelRemovalMask, indexImage, sceneParams.max_surfel_radius);
  }
}

template <typename TSurfel>
void SurfelSceneReconstructionEngine_CPU<TSurfel>::PreprocessDepthMap(const View *view, const SurfelVolumeParameters& sceneParams) const
{
  const float *depthMap = view->depth.GetData(MEMORYDEVICE_CPU);
  const int height = view->depth.dimensions.y;
  const Intrinsics& intrinsics = view->calibration_information.intrinsics_d;
  Vector3f *normalMap = this->m_normalMapMB->GetData(MEMORYDEVICE_CPU);
  const int pixelCount = static_cast<int>(view->depth.size());
  float *radiusMap = this->m_radiusMapMB->GetData(MEMORYDEVICE_CPU);
  Vector4f *vertexMap = this->m_vertexMapMB->GetData(MEMORYDEVICE_CPU);
  const int width = view->depth.dimensions.x;

  // Calculate the vertex map.
#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int locId = 0; locId < pixelCount; ++locId)
  {
    calculate_vertex_position(locId, width, intrinsics, depthMap, vertexMap);
  }

  // Calculate the normal map.
#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int locId = 0; locId < pixelCount; ++locId)
  {
    calculate_normal(locId, vertexMap, width, height, normalMap);
  }

  // Calculate the radius map.
#ifdef WITH_OPENMP
  #pragma omp parallel for
#endif
  for(int locId = 0; locId < pixelCount; ++locId)
  {
    calculate_radius(locId, depthMap, normalMap, intrinsics, radiusMap);
  }
}

template <typename TSurfel>
void SurfelSceneReconstructionEngine_CPU<TSurfel>::RemoveMarkedSurfels(SurfelScene<TSurfel> *scene) const
{
  // Remove marked surfels from the scene.
  // TODO: This is is currently unimplemented on CPU. It's not worth implementing it at the moment,
  // because we're going to need to change the scene representation to something better anyway.
}

}
