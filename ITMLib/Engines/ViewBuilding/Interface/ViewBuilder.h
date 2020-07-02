// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

#include "../../../Objects/Camera/RGBDCalib.h"
#include "../../../Objects/Views/ViewIMU.h"

namespace ITMLib
{
	/** \brief
	*/
	class ViewBuilder
	{
	protected:
		const RGBDCalib calibration_information;
	public:
		virtual void ConvertDisparityToDepth(FloatImage& depth_out, const ShortImage& disparity_in, const Intrinsics& depth_camera_intrinsics,
		                                     Vector2f disparity_calibration_parameters) = 0;
		virtual void ConvertDepthAffineToFloat(FloatImage& depth_out, const ShortImage& depth_in, Vector2f depth_calibration_parameters) = 0;

		/** \brief Find discontinuities in a depth image by removing all pixels for
	     * which the maximum difference between the center pixel and it's neighbours
	     * is higher than a threshold
	     *  \param[in] image_out output image
	     *  \param[in] image_in input image
	     */
		virtual void ThresholdFiltering(FloatImage& image_out, const FloatImage& image_in) = 0;
		virtual void DepthFiltering(FloatImage& image_out, const FloatImage& image_in) = 0;
		virtual void ComputeNormalAndWeights(Float4Image& normal_out, FloatImage& sigma_z_out, const FloatImage& depth_in, Vector4f depth_camera_projection_parameters) = 0;

		virtual void UpdateView(View** view, UChar4Image* rgb_image, ShortImage* raw_depth_image, bool use_threshold_filter,
		                        bool use_bilateral_filter, bool model_sensor_noise, bool store_previous_image) = 0;
		virtual void UpdateView(View** view, UChar4Image* rgb_image, ShortImage* depth_image, bool use_threshold_filter,
		                        bool use_bilateral_filter, IMUMeasurement* imu_measurement, bool model_sensor_noise,
		                        bool store_previous_image) = 0;

		ViewBuilder(const RGBDCalib& calibration_information)
		: calibration_information(calibration_information){}

		virtual ~ViewBuilder()
		{}
	};
}
