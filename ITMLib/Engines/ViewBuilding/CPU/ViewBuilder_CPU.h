// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

#include "../Interface/ViewBuilder.h"

namespace ITMLib
{
	class ViewBuilder_CPU : public ViewBuilder
	{
	public:
		void ConvertDisparityToDepth(ITMFloatImage *depth_out, const ITMShortImage *disp_in, const Intrinsics *depthIntrinsics,
			Vector2f disparityCalibParams);
		void ConvertDepthAffineToFloat(ITMFloatImage *depth_out, const ITMShortImage *depth_in, Vector2f depthCalibParams);

		void ThresholdFiltering(ITMFloatImage *image_out, const ITMFloatImage *image_in) override;
		void DepthFiltering(ITMFloatImage *image_out, const ITMFloatImage *image_in);
		void ComputeNormalAndWeights(ITMFloat4Image *normal_out, ITMFloatImage *sigmaZ_out, const ITMFloatImage *depth_in, Vector4f intrinsic);

		void UpdateView(View** view, ITMUChar4Image* rgbImage, ITMShortImage* rawDepthImage, bool useThresholdFilter,
		                bool useBilateralFilter, bool modelSensorNoise, bool storePreviousImage);
		void UpdateView(View** view, ITMUChar4Image* rgbImage, ITMShortImage* depthImage, bool useThresholdFilter,
		                bool useBilateralFilter, IMUMeasurement* imuMeasurement, bool modelSensorNoise,
		                bool storePreviousImage);

		ViewBuilder_CPU(const RGBDCalib& calib);
		~ViewBuilder_CPU(void);
	};
}
