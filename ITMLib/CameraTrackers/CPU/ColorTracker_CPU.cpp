// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

//__DEBUG
#include <iostream>

#include "ColorTracker_CPU.h"
#include "../Shared/ColorTracker_Shared.h"


using namespace ITMLib;

ColorTracker_CPU::ColorTracker_CPU(Vector2i imgSize, TrackerIterationType* trackingRegime, int noHierarchyLevels,
                                   const ImageProcessingEngineInterface* lowLevelEngine)
		: ColorTracker(imgSize, trackingRegime, noHierarchyLevels, lowLevelEngine, MEMORYDEVICE_CPU) {}

ColorTracker_CPU::~ColorTracker_CPU() {}

int ColorTracker_CPU::F_oneLevel(float* f, ORUtils::SE3Pose* pose) {
	const int point_count = trackingState->point_cloud->point_count;

	Vector4f rgb_camera_projection_parameters = view->calibration_information.intrinsics_rgb.projection_params_simple.all;
	rgb_camera_projection_parameters.fx /= static_cast<float>(1u << levelId);
	rgb_camera_projection_parameters.fy /= static_cast<float>(1u << levelId);
	rgb_camera_projection_parameters.cx /= static_cast<float>(1u << levelId);
	rgb_camera_projection_parameters.cy /= static_cast<float>(1u << levelId);

	const Matrix4f M = pose->GetM();

	const Vector2i rgb_image_size = viewHierarchy->GetLevel(levelId)->rgb->dimensions;

	float scale_for_occlusions, final_f;

	Vector4f* locations = trackingState->point_cloud->locations.GetData(MEMORYDEVICE_CPU);
	Vector4f* colours = trackingState->point_cloud->colors.GetData(MEMORYDEVICE_CPU);
	Vector4u* rgb = viewHierarchy->GetLevel(levelId)->rgb->GetData(MEMORYDEVICE_CPU);

	final_f = 0;
	countedPoints_valid = 0;

#ifdef WITH_OPENMP
#pragma omp parallel for default(none) firstprivate(point_count, rgb_image_size, rgb_camera_projection_parameters, M)\
shared(locations, colours, rgb, final_f)
#endif
	for (int locId = 0; locId < point_count; locId++) {
		float color_difference_squared = getColorDifferenceSq(locations, colours, rgb, rgb_image_size, locId, rgb_camera_projection_parameters, M);
		if (color_difference_squared >= 0) {
#ifdef WITH_OPENMP
#pragma omp critical
#endif
			{
				final_f += color_difference_squared;
				countedPoints_valid++;
			}
		}
	}

	//__DEBUG
	// std::cout << "final_f: " << final_f << std::endl;
	// std::cout << "countedPoints_valid: " << countedPoints_valid << std::endl;

	if (countedPoints_valid == 0) {
		final_f = 1e10;
		scale_for_occlusions = 1.0;
	} else { scale_for_occlusions = (float) point_count / countedPoints_valid; }

	f[0] = final_f * scale_for_occlusions;

	return countedPoints_valid;
}

void ColorTracker_CPU::G_oneLevel(float* gradient, float* hessian, ORUtils::SE3Pose* pose) const {
	const int noTotalPoints = trackingState->point_cloud->point_count;

	Vector4f projParams = view->calibration_information.intrinsics_rgb.projection_params_simple.all;
	projParams.x /= 1 << levelId;
	projParams.y /= 1 << levelId;
	projParams.z /= 1 << levelId;
	projParams.w /= 1 << levelId;

	const Matrix4f M = pose->GetM();

	const Vector2i imgSize = viewHierarchy->GetLevel(levelId)->rgb->dimensions;

	float scaleForOcclusions;

	const bool rotationOnly = iterationType == TRACKER_ITERATION_ROTATION;
	const int numPara = rotationOnly ? 3 : 6, startPara = rotationOnly ? 3 : 0, numParaSQ = rotationOnly ? 3 + 2 + 1 : 6 + 5 + 4 + 3 + 2 + 1;

	float globalGradient[6], globalHessian[21];
	for (int i = 0; i < numPara; i++) globalGradient[i] = 0.0f;
	for (int i = 0; i < numParaSQ; i++) globalHessian[i] = 0.0f;

	Vector4f* locations = trackingState->point_cloud->locations.GetData(MEMORYDEVICE_CPU);
	Vector4f* colours = trackingState->point_cloud->colors.GetData(MEMORYDEVICE_CPU);
	Vector4u* rgb = viewHierarchy->GetLevel(levelId)->rgb->GetData(MEMORYDEVICE_CPU);
	Vector4s* gx = viewHierarchy->GetLevel(levelId)->gradientX_rgb->GetData(MEMORYDEVICE_CPU);
	Vector4s* gy = viewHierarchy->GetLevel(levelId)->gradientY_rgb->GetData(MEMORYDEVICE_CPU);
#ifdef WITH_OPENMP
#pragma omp parallel for default(none) firstprivate(noTotalPoints, imgSize, projParams, M, rotationOnly, numPara, startPara, numParaSQ)\
shared(locations, colours, rgb, gx, gy, globalGradient, globalHessian)
#endif
	for (int locId = 0; locId < noTotalPoints; locId++) {
		float localGradient[6], localHessian[21];

		computePerPointGH_rt_Color(localGradient, localHessian, locations, colours, rgb, imgSize, locId,
		                           projParams, M, gx, gy, 6, 0);

		bool isValidPoint = computePerPointGH_rt_Color(localGradient, localHessian, locations, colours, rgb, imgSize, locId,
		                                               projParams, M, gx, gy, numPara, startPara);

		if (isValidPoint) {
#ifdef WITH_OPENMP
#pragma omp critical
#endif
			{
				for (int i = 0; i < numPara; i++) globalGradient[i] += localGradient[i];
				for (int i = 0; i < numParaSQ; i++) globalHessian[i] += localHessian[i];
			}
		}
	}

	scaleForOcclusions = (float) noTotalPoints / countedPoints_valid;
	if (countedPoints_valid == 0) { scaleForOcclusions = 1.0f; }

	for (int para = 0, counter = 0; para < numPara; para++) {
		gradient[para] = globalGradient[para] * scaleForOcclusions;
		for (int col = 0; col <= para; col++, counter++) hessian[para + col * numPara] = globalHessian[counter] * scaleForOcclusions;
	}
	for (int row = 0; row < numPara; row++) {
		for (int col = row + 1; col < numPara; col++) hessian[row + col * numPara] = hessian[col + row * numPara];
	}
}
