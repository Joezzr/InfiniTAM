// Copyright 2014 Isis Innovation Limited and the authors of InfiniTAM

#ifdef COMPILE_WITH_METAL

#import "MetalContext.h"

#include "VisualizationEngine_Metal.h"
#include "../Shared/VisualizationEngine_Shared.h"

#include <vector>

using namespace ITMLib;

struct VisualizationEngine_MetalBits
{
    id<MTLFunction> f_genericRaycastVH_device;
    id<MTLComputePipelineState> p_genericRaycastVH_device;

    id<MTLFunction> f_renderICP_device;
    id<MTLComputePipelineState> p_renderICP_device;

    id<MTLBuffer> paramsBuffer;
};

static VisualizationEngine_MetalBits vis_metalBits;

template<class TVoxel>
VisualizationEngine_Metal<TVoxel, ITMVoxelBlockHash>::VisualizationEngine_Metal()
: VisualizationEngine_CPU<TVoxel, ITMVoxelBlockHash>()
{
    NSError *errors;

    vis_metalBits.f_genericRaycastVH_device = [[[MetalContext instance]library]newFunctionWithName:@"genericRaycastVH_device"];
    vis_metalBits.p_genericRaycastVH_device = [[[MetalContext instance]device]newComputePipelineStateWithFunction:vis_metalBits.f_genericRaycastVH_device error:&errors];

    vis_metalBits.f_renderICP_device = [[[MetalContext instance]library]newFunctionWithName:@"renderICP_device"];
    vis_metalBits.p_renderICP_device = [[[MetalContext instance]device]newComputePipelineStateWithFunction:vis_metalBits.f_renderICP_device error:&errors];

    vis_metalBits.paramsBuffer = BUFFEREMPTY(16384);
}

template<class TVoxel, class TIndex>
static void CreateICPMaps_common_metal(const ITMScene<TVoxel,TIndex> *scene, const View *view, ITMTrackingState *trackingState, ITMRenderState *renderState)
{
    const void *entriesVisibleType = NULL;
    if ((dynamic_cast<const ITMRenderState_VH*>(renderState)!=NULL))
    {
        entriesVisibleType = ((ITMRenderState_VH*)renderState)->GetEntriesVisibleType_MB();
    }

    id<MTLCommandBuffer> commandBuffer = [[[MetalContext instance]commandQueue]commandBuffer];
    id<MTLComputeCommandEncoder> commandEncoder = [commandBuffer computeCommandEncoder];

    trackingState->pose_pointCloud->SetFrom(trackingState->pose_d);

    CreateICPMaps_Params *params = (CreateICPMaps_Params*)[vis_metalBits.paramsBuffer contents];
    params->imgSize.x = view->depth->noDims.x; params->imgSize.y = view->depth->noDims.y; params->imgSize.z = 0; params->imgSize.w = 1;
    params->voxelSizes.x = scene->GetParameters().voxelSize;
    params->voxelSizes.y = 1.0f / scene->GetParameters().voxelSize;
    params->invM = trackingState->pose_d->GetInvM();
    params->invProjParams = InvertProjectionParams(view->calib.intrinsics_d.projectionParamsSimple.all);
    params->lightSource.x = -Vector3f(params->invM.getColumn(2)).x;
    params->lightSource.y = -Vector3f(params->invM.getColumn(2)).y;
    params->lightSource.z = -Vector3f(params->invM.getColumn(2)).z;
    params->lightSource.w = scene->GetParameters().mu;

    [commandEncoder setComputePipelineState:vis_metalBits.p_genericRaycastVH_device];
    [commandEncoder setBuffer:(__bridge id<MTLBuffer>) renderState->raycastResult->GetMetalBuffer()             offset:0 atIndex:0];
    [commandEncoder setBuffer:(__bridge id<MTLBuffer>) entriesVisibleType                                       offset:0 atIndex:1];
    [commandEncoder setBuffer:(__bridge id<MTLBuffer>) scene->voxels.GetVoxelBlocks_MB()                      offset:0 atIndex:2];
    [commandEncoder setBuffer:(__bridge id<MTLBuffer>) scene->index.getIndexData_MB()                           offset:0 atIndex:3];
    [commandEncoder setBuffer:(__bridge id<MTLBuffer>) renderState->renderingRangeImage->GetMetalBuffer()       offset:0 atIndex:4];
    [commandEncoder setBuffer:vis_metalBits.paramsBuffer                                                        offset:0 atIndex:5];

    MTLSize blockSize = {16, 16, 1};
    MTLSize gridSize = {(NSUInteger)ceil((float)view->depth->noDims.x / (float)blockSize.width),
        (NSUInteger)ceil((float)view->depth->noDims.y / (float)blockSize.height), 1};

    [commandEncoder dispatchThreadgroups:gridSize threadsPerThreadgroup:blockSize];
    [commandEncoder endEncoding];

    commandEncoder = [commandBuffer computeCommandEncoder];

    [commandEncoder setComputePipelineState:vis_metalBits.p_renderICP_device];
    [commandEncoder setBuffer:(__bridge id<MTLBuffer>) renderState->raycastResult->GetMetalBuffer()             offset:0 atIndex:0];
    [commandEncoder setBuffer:(__bridge id<MTLBuffer>) trackingState->pointCloud->locations->GetMetalBuffer()   offset:0 atIndex:1];
    [commandEncoder setBuffer:(__bridge id<MTLBuffer>) trackingState->pointCloud->colours->GetMetalBuffer()     offset:0 atIndex:2];
    [commandEncoder setBuffer:(__bridge id<MTLBuffer>) renderState->raycastImage->GetMetalBuffer()              offset:0 atIndex:3];
    [commandEncoder setBuffer:vis_metalBits.paramsBuffer                                                        offset:0 atIndex:4];

    [commandEncoder dispatchThreadgroups:gridSize threadsPerThreadgroup:blockSize];
    [commandEncoder endEncoding];

    [commandBuffer commit];

    [commandBuffer waitUntilCompleted];
}

template<class TVoxel>
static void RenderImage_common_metal(const ITMScene<TVoxel,ITMVoxelBlockHash> *scene, const ORUtils::SE3Pose *pose, const ITMIntrinsics *intrinsics,
                               const ITMRenderState *renderState, ITMUChar4Image *outputImage, IVisualizationEngine::RenderImageType type, IVisualizationEngine::RenderRaycastSelection raycastType)
{
    Vector2i imgSize = outputImage->noDims;
    Matrix4f invM = pose->GetInvM();

    Vector4f *pointsRay;
    if (raycastType == IVisualizationEngine::RENDER_FROM_OLD_RAYCAST)
        pointsRay = renderState->raycastResult->GetData(MEMORYDEVICE_CPU);
    else
    {
        if (raycastType == IVisualizationEngine::RENDER_FROM_OLD_FORWARDPROJ)
            pointsRay = renderState->forwardProjection->GetData(MEMORYDEVICE_CPU);
        else
        {
            // this one is generally done for freeview Rendering, so
            // no, do not update the list of visible blocks

            const void *entriesVisibleType = NULL;
            if ((dynamic_cast<const ITMRenderState_VH*>(renderState)!=NULL))
            {
                entriesVisibleType = ((ITMRenderState_VH*)renderState)->GetEntriesVisibleType_MB();
            }

            id<MTLCommandBuffer> commandBuffer = [[[MetalContext instance]commandQueue]commandBuffer];
            id<MTLComputeCommandEncoder> commandEncoder = [commandBuffer computeCommandEncoder];

            CreateICPMaps_Params *params = (CreateICPMaps_Params*)[vis_metalBits.paramsBuffer contents];
            params->imgSize.x = outputImage->noDims.x; params->imgSize.y = outputImage->noDims.y; params->imgSize.z = 0; params->imgSize.w = 0;
            params->voxelSizes.x = scene->GetParameters().voxelSize;
            params->voxelSizes.y = 1.0f / scene->GetParameters().voxelSize;
            params->invM = pose->GetInvM();
            params->invProjParams = InvertProjectionParams(intrinsics->projectionParamsSimple.all);
            params->lightSource.x = -Vector3f(params->invM.getColumn(2)).x;
            params->lightSource.y = -Vector3f(params->invM.getColumn(2)).y;
            params->lightSource.z = -Vector3f(params->invM.getColumn(2)).z;
            params->lightSource.w = scene->GetParameters().mu;

            [commandEncoder setComputePipelineState:vis_metalBits.p_genericRaycastVH_device];
            [commandEncoder setBuffer:(__bridge id<MTLBuffer>) renderState->raycastResult->GetMetalBuffer()             offset:0 atIndex:0];
            [commandEncoder setBuffer:(__bridge id<MTLBuffer>) entriesVisibleType                                       offset:0 atIndex:1];
            [commandEncoder setBuffer:(__bridge id<MTLBuffer>) scene->voxels.GetVoxelBlocks_MB()                      offset:0 atIndex:2];
            [commandEncoder setBuffer:(__bridge id<MTLBuffer>) scene->index.getIndexData_MB()                           offset:0 atIndex:3];
            [commandEncoder setBuffer:(__bridge id<MTLBuffer>) renderState->renderingRangeImage->GetMetalBuffer()       offset:0 atIndex:4];
            [commandEncoder setBuffer:vis_metalBits.paramsBuffer                                                        offset:0 atIndex:5];

            MTLSize blockSize = {16, 16, 1};
            MTLSize gridSize = {(NSUInteger)ceil((float)outputImage->noDims.x / (float)blockSize.width),
                (NSUInteger)ceil((float)outputImage->noDims.y / (float)blockSize.height), 1};

            [commandEncoder dispatchThreadgroups:gridSize threadsPerThreadgroup:blockSize];
            [commandEncoder endEncoding];

            [commandBuffer commit];

            [commandBuffer waitUntilCompleted];
            pointsRay = renderState->raycastResult->GetData(MEMORYDEVICE_CPU);
        }
    }

    Vector3f lightSource = -Vector3f(invM.getColumn(2));
    Vector4u *outRendering = outputImage->GetData(MEMORYDEVICE_CPU);
    const TVoxel *voxelData = scene->GetVoxels();
    const typename ITMVoxelBlockHash::IndexData *voxelIndex = scene->index.GetIndexData();

    if ((type == IVisualizationEngine::RENDER_COLOUR_FROM_VOLUME)&&
        (!TVoxel::hasColorInformation)) type = IVisualizationEngine::RENDER_SHADED_GREYSCALE;

    switch (type) {
        case IVisualizationEngine::RENDER_COLOUR_FROM_VOLUME:
            for (int locId = 0; locId < imgSize.x * imgSize.y; locId++)
            {
                Vector4f ptRay = pointsRay[locId];
                processPixelColour<TVoxel, ITMVoxelBlockHash>(outRendering[locId], ptRay.toVector3(), ptRay.w > 0, voxelData, voxelIndex, lightSource);
            }
            break;
        case IVisualizationEngine::RENDER_COLOUR_FROM_NORMAL:
            for (int locId = 0; locId < imgSize.x * imgSize.y; locId++)
            {
                Vector4f ptRay = pointsRay[locId];
                processPixelNormal<TVoxel, ITMVoxelBlockHash>(outRendering[locId], ptRay.toVector3(), ptRay.w > 0, voxelData, voxelIndex, lightSource);
            }
            break;
        case IVisualizationEngine::RENDER_COLOUR_FROM_CONFIDENCE:
            for (int locId = 0; locId < imgSize.x * imgSize.y; locId++)
            {
                Vector4f ptRay = pointsRay[locId];
                processPixelConfidence<TVoxel, ITMVoxelBlockHash>(outRendering[locId], ptRay, ptRay.w > 0, voxelData, voxelIndex, lightSource);
            }
            break;
        case IVisualizationEngine::RENDER_SHADED_GREYSCALE_IMAGENORMALS:
            for (int locId = 0; locId < imgSize.x * imgSize.y; locId++)
            {
                int y = locId/imgSize.x;
                int x = locId - y*imgSize.x;
                processPixelGrey_ImageNormals<true, false>(outRendering, pointsRay, imgSize, x, y, scene->GetParameters().voxelSize, lightSource);
            }
            break;
        case IVisualizationEngine::RENDER_SHADED_GREYSCALE:
        default:
            for (int locId = 0; locId < imgSize.x * imgSize.y; locId++)
            {
                Vector4f ptRay = pointsRay[locId];
                processPixelGrey<TVoxel, ITMVoxelBlockHash>(outRendering[locId], ptRay.toVector3(), ptRay.w > 0, voxelData, voxelIndex, lightSource);
            }
    }
}

template<class TVoxel>
void VisualizationEngine_Metal<TVoxel, ITMVoxelBlockHash>::CreateICPMaps(const ITMScene<TVoxel,ITMVoxelBlockHash> *scene, const View *view, ITMTrackingState *trackingState, ITMRenderState *renderState) const
{
    CreateICPMaps_common_metal(scene, view, trackingState, renderState);
}

template<class TVoxel>
void VisualizationEngine_Metal<TVoxel,ITMVoxelBlockHash>::RenderImage(const ITMScene<TVoxel,ITMVoxelBlockHash> *scene, const ORUtils::SE3Pose *pose, const ITMIntrinsics *intrinsics,
                                                            const ITMRenderState *renderState, ITMUChar4Image *outputImage, IVisualizationEngine::RenderImageType type,
                                                            IVisualizationEngine::RenderRaycastSelection raycastType) const
{
    RenderImage_common_metal(scene, pose, intrinsics, renderState, outputImage, type, raycastType);
}

#endif