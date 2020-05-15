// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#include "RealSenseEngine.h"

#include "../ORUtils/FileUtils.h"

#include <cstdio>
#include <stdexcept>

#ifdef COMPILE_WITH_RealSense

using namespace InputSource;
using namespace ITMLib;

class RealSenseEngine::PrivateData
{
	public:
	PrivateData() : dev(NULL){}
	rs::device *dev = NULL;
	rs::context ctx;
};

RealSenseEngine::RealSenseEngine(const char *calibFilename, bool alignColourWithDepth,
								 Vector2i requested_imageSize_rgb, Vector2i requested_imageSize_d)
: BaseImageSourceEngine(calibFilename),
  colourStream(alignColourWithDepth ? rs::stream::color_aligned_to_depth : rs::stream::color)
{
	this->calib.disparityCalib.SetStandard();
	this->calib.trafo_rgb_to_depth = ITMExtrinsics();
	this->calib.intrinsics_d = this->calib.intrinsics_rgb;

	this->imageSize_d = requested_imageSize_d;
	this->imageSize_rgb = requested_imageSize_rgb;

	data = new RealSenseEngine::PrivateData();
	printf("There are %d connected RealSense devices.\n", data->ctx.get_device_count());
	if (data->ctx.get_device_count() == 0) {
		dataAvailable = false;
		delete data;
		data = NULL;
		return;
	}

	data->dev = data->ctx.get_device(0);

	data->dev->enable_stream(rs::stream::depth, imageSize_d.x, imageSize_d.y, rs::format::z16, 60);
	data->dev->enable_stream(rs::stream::color, imageSize_rgb.x, imageSize_rgb.y, rs::format::rgb8, 60);

	rs::intrinsics intrinsics_depth = data->dev->get_stream_intrinsics(rs::stream::depth);
	rs::intrinsics intrinsics_rgb = data->dev->get_stream_intrinsics(colourStream);

	this->calib.intrinsics_d.projectionParamsSimple.fx = intrinsics_depth.fx;
	this->calib.intrinsics_d.projectionParamsSimple.fy = intrinsics_depth.fy;
	this->calib.intrinsics_d.projectionParamsSimple.px = intrinsics_depth.ppx;
	this->calib.intrinsics_d.projectionParamsSimple.py = intrinsics_depth.ppy;

	this->calib.intrinsics_rgb.projectionParamsSimple.fx = intrinsics_rgb.fx;
	this->calib.intrinsics_rgb.projectionParamsSimple.fy = intrinsics_rgb.fy;
	this->calib.intrinsics_rgb.projectionParamsSimple.px = intrinsics_rgb.ppx;
	this->calib.intrinsics_rgb.projectionParamsSimple.py = intrinsics_rgb.ppy;

	rs::extrinsics rs_extrinsics = data->dev->get_extrinsics(colourStream, rs::stream::depth);

	Matrix4f extrinsics;
	extrinsics.m00 = rs_extrinsics.rotation[0]; extrinsics.m10 = rs_extrinsics.rotation[1]; extrinsics.m20 = rs_extrinsics.rotation[2];
	extrinsics.m01 = rs_extrinsics.rotation[3]; extrinsics.m11 = rs_extrinsics.rotation[4]; extrinsics.m21 = rs_extrinsics.rotation[5];
	extrinsics.m02 = rs_extrinsics.rotation[6]; extrinsics.m12 = rs_extrinsics.rotation[7]; extrinsics.m22 = rs_extrinsics.rotation[8];
	extrinsics.m30 = rs_extrinsics.translation[0];
	extrinsics.m31 = rs_extrinsics.translation[1];
	extrinsics.m32 = rs_extrinsics.translation[2];

	extrinsics.m33 = 1.0f;
	extrinsics.m03 = 0.0f; extrinsics.m13 = 0.0f; extrinsics.m23 = 0.0f;

	this->calib.trafo_rgb_to_depth.SetFrom(extrinsics);

	this->calib.disparityCalib.SetFrom(data->dev->get_depth_scale(), 0.0f,
		ITMDisparityCalib::TRAFO_AFFINE);

	data->dev->start();
}

RealSenseEngine::~RealSenseEngine()
{
	if (data != NULL)
	{
		data->dev->stop();
		delete data;
	}
}


void RealSenseEngine::GetImages(UChar4Image *rgbImage, ShortImage *rawDepthImage)
{
	dataAvailable = false;

	// get frames
	data->dev->wait_for_frames();
	const uint16_t * depth_frame = reinterpret_cast<const uint16_t *>(data->dev->get_frame_data(rs::stream::depth));
	const uint8_t * color_frame = reinterpret_cast<const uint8_t*>(data->dev->get_frame_data(colourStream));

	// setup infinitam frames
	short *rawDepth = rawDepthImage->GetData(MEMORYDEVICE_CPU);
	Vector4u *rgb = rgbImage->GetData(MEMORYDEVICE_CPU);

	Vector2i dimensions = rawDepthImage->dimensions;
	rawDepthImage->Clear();
	rgbImage->Clear();

	for (int y = 0; y < dimensions.y; y++) for (int x = 0; x < dimensions.x; x++) rawDepth[x + y * dimensions.x] = *depth_frame++;

	for (int i = 0; i < rgbImage->dimensions.x * 3 * rgbImage->dimensions.y ; i+=3) {
		Vector4u newPix;
		newPix.x = color_frame[i]; newPix.y = color_frame[i+1]; newPix.z = color_frame[i+2];
		newPix.w = 255;
		rgb[i/3] = newPix;
	}

	dataAvailable = true;
}

bool RealSenseEngine::HasMoreImages() const { return (data!=NULL); }
Vector2i RealSenseEngine::GetDepthImageSize() const { return (data!=NULL)?imageSize_d:Vector2i(0,0); }
Vector2i RealSenseEngine::GetRGBImageSize() const { return (data!=NULL)?imageSize_rgb:Vector2i(0,0); }

#else

using namespace InputSource;

RealSenseEngine::RealSenseEngine(const char* calibFilename, bool alignColourWithDepth,
                                 Vector2i requested_imageSize_rgb, Vector2i requested_imageSize_d)
		: BaseImageSourceEngine(calibFilename) {
	printf("compiled without RealSense Windows support\n");
}

RealSenseEngine::~RealSenseEngine() {}

void RealSenseEngine::GetImages(UChar4Image& rgbImage, ShortImage& rawDepthImage) { }

bool RealSenseEngine::HasMoreImages() const { return false; }

Vector2i RealSenseEngine::GetDepthImageSize() const { return Vector2i(0, 0); }

Vector2i RealSenseEngine::GetRGBImageSize() const { return Vector2i(0, 0); }

#endif

