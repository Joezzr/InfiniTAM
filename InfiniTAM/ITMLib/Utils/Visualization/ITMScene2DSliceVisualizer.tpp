//  ================================================================
//  Created by Gregory Kramida on 12/5/17.
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
//stdlib
#include <unordered_set>
#include <iomanip>

//opencv
#include <opencv2/imgcodecs.hpp>

//boost
#include <boost/filesystem.hpp>

//local
#include "ITMScene2DSliceVisualizer.h"
#include "../../Objects/Scene/ITMRepresentationAccess.h"
#include "../Analytics/ITMSceneStatisticsCalculator.h"

using namespace ITMLib;
namespace fs = boost::filesystem;



template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
const bool ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::absFillingStrategy = false;




// region ===================================== CONSTRUCTORS / DESTRUCTORS =============================================

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::ITMScene2DSliceVisualizer(Vector3i focusCoordinates, unsigned int imageSizeVoxels, float pixelsPerVoxel,
                                                                                          Plane plane)
		:
		focusCoordinates(focusCoordinates),
		imageSizeVoxels(imageSizeVoxels),
		imageHalfSizeVoxels(imageSizeVoxels / 2),

		imgRangeStartX(focusCoordinates.x - imageHalfSizeVoxels),
		imgRangeEndX(focusCoordinates.x + imageHalfSizeVoxels),
		imgRangeStartY(focusCoordinates.y - imageHalfSizeVoxels),
		imgRangeEndY(focusCoordinates.y + imageHalfSizeVoxels),
		imgRangeStartZ(focusCoordinates.z - imageHalfSizeVoxels),
		imgRangeEndZ(focusCoordinates.z + imageHalfSizeVoxels),

		imgXSlice(focusCoordinates.x),
		imgYSlice(focusCoordinates.y),
		imgZSlice(focusCoordinates.z),

		imgVoxelRangeX(imgRangeEndX - imgRangeStartX),
		imgVoxelRangeY(imgRangeEndY - imgRangeStartY),
		imgVoxelRangeZ(imgRangeEndZ - imgRangeStartZ),

		pixelsPerVoxel(pixelsPerVoxel),

		imgPixelRangeX(static_cast<int>(pixelsPerVoxel * imgVoxelRangeX)),
		imgPixelRangeY(static_cast<int>(pixelsPerVoxel * imgVoxelRangeY)),
		imgPixelRangeZ(static_cast<int>(pixelsPerVoxel * imgVoxelRangeZ)),

		plane(plane) {

}

// endregion ===========================================================================================================

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
template<typename TVoxel>
cv::Mat ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::DrawSceneImageAroundPoint(
		ITMScene<TVoxel, TIndex>* scene) {
#ifdef IMAGE_BLACK_BACKGROUND
	//use if default voxel SDF value is -1.0
	cv::Mat img = cv::Mat::zeros(imgPixelRangeX, imgPixelRangeY, CV_32F);
#else
	//use if default voxel SDF value is 1.0
	cv::Mat img = cv::Mat::ones(imgPixelRangeX, imgPixelRangeY, CV_32F);
#endif
	TVoxel* voxelBlocks = scene->localVBA.GetVoxelBlocks();
	const ITMHashEntry* canonicalHashTable = scene->index.GetEntries();
	int noTotalEntries = scene->index.noTotalEntries;
	typename TIndex::IndexCache canonicalCache;

	std::unordered_set<float> valueSet = {};


#ifdef WITH_OPENMP
#pragma omp parallel for reduction(+:numPixelsFilled)
#endif
	for (int entryId = 0; entryId < noTotalEntries; entryId++) {
		Vector3i currentBlockPositionVoxels;
		const ITMHashEntry& currentHashEntry = canonicalHashTable[entryId];

		if (currentHashEntry.ptr < 0) continue;

		//position of the current entry in 3D space
		currentBlockPositionVoxels = currentHashEntry.pos.toInt() * SDF_BLOCK_SIZE;

		if (!IsVoxelBlockInImageRange(currentBlockPositionVoxels, plane)) continue;

		TVoxel* localVoxelBlock = &(voxelBlocks[currentHashEntry.ptr * (SDF_BLOCK_SIZE3)]);

		for (int z = 0; z < SDF_BLOCK_SIZE; z++) {
			for (int y = 0; y < SDF_BLOCK_SIZE; y++) {
				for (int x = 0; x < SDF_BLOCK_SIZE; x++) {
					Vector3i originalPosition = currentBlockPositionVoxels + Vector3i(x, y, z);
					if (!IsVoxelInImageRange(originalPosition.x, originalPosition.y, originalPosition.z,
					                         plane))
						continue;

					int locId = x + y * SDF_BLOCK_SIZE + z * SDF_BLOCK_SIZE * SDF_BLOCK_SIZE;
					TVoxel& voxel = localVoxelBlock[locId];
					Vector2i imgCoords = GetVoxelImageCoordinates(originalPosition, plane);
					const int voxelOnImageSize = static_cast<int>(pixelsPerVoxel);
					for (int row = imgCoords.y; row < imgCoords.y + voxelOnImageSize; row++) {
						for (int col = imgCoords.x; col < imgCoords.x + voxelOnImageSize; col++) {
							float value = SdfToShadeValue(TVoxel::valueToFloat(voxel.sdf));
							img.at<float>(row, col) = value;
						}
					}
				}
			}
		}
	}
	return img;
}


template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
template<typename TVoxel>
cv::Mat ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::DrawSceneImageAroundPointIndexedFields(
		ITMScene<TVoxel, TIndex>* scene, int fieldIndex) {
#ifdef IMAGE_BLACK_BACKGROUND
	//use if default voxel SDF value is -1.0
	cv::Mat img = cv::Mat::zeros(imgPixelRangeX, imgPixelRangeY, CV_32F);
#else
	//use if default voxel SDF value is 1.0
	cv::Mat img = cv::Mat::ones(imgPixelRangeX, imgPixelRangeY, CV_32F);
#endif
	TVoxel* voxelBlocks = scene->localVBA.GetVoxelBlocks();
	const ITMHashEntry* canonicalHashTable = scene->index.GetEntries();
	int noTotalEntries = scene->index.noTotalEntries;
	typename TIndex::IndexCache canonicalCache;

	std::unordered_set<float> valueSet = {};
#ifdef WITH_OPENMP
#pragma omp parallel for reduction(+:numPixelsFilled)
#endif
	for (int entryId = 0; entryId < noTotalEntries; entryId++) {
		Vector3i currentBlockPositionVoxels;
		const ITMHashEntry& currentHashEntry = canonicalHashTable[entryId];

		if (currentHashEntry.ptr < 0) continue;

		//position of the current entry in 3D space
		currentBlockPositionVoxels = currentHashEntry.pos.toInt() * SDF_BLOCK_SIZE;

		if (!IsVoxelBlockInImageRange(currentBlockPositionVoxels, plane)) continue;

		TVoxel* localVoxelBlock = &(voxelBlocks[currentHashEntry.ptr * (SDF_BLOCK_SIZE3)]);

		for (int z = 0; z < SDF_BLOCK_SIZE; z++) {
			for (int y = 0; y < SDF_BLOCK_SIZE; y++) {
				for (int x = 0; x < SDF_BLOCK_SIZE; x++) {
					Vector3i originalPosition = currentBlockPositionVoxels + Vector3i(x, y, z);
					if (!IsVoxelInImageRange(originalPosition.x, originalPosition.y, originalPosition.z,
					                         plane))
						continue;

					int locId = x + y * SDF_BLOCK_SIZE + z * SDF_BLOCK_SIZE * SDF_BLOCK_SIZE;
					TVoxel& voxel = localVoxelBlock[locId];
					Vector2i imgCoords = GetVoxelImageCoordinates(originalPosition, plane);
					const int voxelOnImageSize = static_cast<int>(pixelsPerVoxel);
					float value = SdfToShadeValue(TVoxel::valueToFloat(voxel.sdf_values[fieldIndex]));
					for (int row = imgCoords.y; row < imgCoords.y + voxelOnImageSize; row++) {
						for (int col = imgCoords.x; col < imgCoords.x + voxelOnImageSize; col++) {
							img.at<float>(row, col) = value;
						}
					}
				}
			}
		}
	}
	return img;
}


template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
template<typename TVoxel>
cv::Mat ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::DrawWarpedSceneImageTemplated(
		ITMScene<TVoxel, TIndex>* scene) {
#ifdef IMAGE_BLACK_BACKGROUND
	//use if default voxel SDF value is -1.0
	cv::Mat img = cv::Mat::zeros(imgPixelRangeX, imgPixelRangeY, CV_32F);
#else
	//use if default voxel SDF value is 1.0
	cv::Mat img = cv::Mat::ones(imgPixelRangeX, imgPixelRangeY, CV_32F);
#endif

	TVoxel* voxelBlocks = scene->localVBA.GetVoxelBlocks();
	const ITMHashEntry* canonicalHashTable = scene->index.GetEntries();
	int noTotalEntries = scene->index.noTotalEntries;
	typename TIndex::IndexCache canonicalCache;

#ifdef WITH_OPENMP
#pragma omp parallel for
#endif
	for (int entryId = 0; entryId < noTotalEntries; entryId++) {
		Vector3i currentBlockPositionVoxels;
		const ITMHashEntry& currentHashEntry = canonicalHashTable[entryId];

		if (currentHashEntry.ptr < 0) continue;

		//position of the current entry in 3D space
		currentBlockPositionVoxels = currentHashEntry.pos.toInt() * SDF_BLOCK_SIZE;

		if (!IsVoxelBlockInImageRangeTolerance(currentBlockPositionVoxels, 5, plane)) continue;

		TVoxel* localVoxelBlock = &(voxelBlocks[currentHashEntry.ptr * (SDF_BLOCK_SIZE3)]);

		for (int z = 0; z < SDF_BLOCK_SIZE; z++) {
			for (int y = 0; y < SDF_BLOCK_SIZE; y++) {
				for (int x = 0; x < SDF_BLOCK_SIZE; x++) {
					Vector3i originalPosition = currentBlockPositionVoxels + Vector3i(x, y, z);
					int locId = x + y * SDF_BLOCK_SIZE + z * SDF_BLOCK_SIZE * SDF_BLOCK_SIZE;
					TVoxel& voxel = localVoxelBlock[locId];
					Vector3f projectedPosition = originalPosition.toFloat() + voxel.framewise_warp;
					Vector3i projectedPositionFloored = projectedPosition.toIntFloor();
					switch (plane){
						case PLANE_XY:
							projectedPositionFloored.z = originalPosition.z;
							break;
						case PLANE_YZ:
							projectedPositionFloored.x = originalPosition.x;
							break;
						case PLANE_XZ:
							projectedPosition.y = originalPosition.y;
							break;
					}
					if (!IsVoxelInImageRange(projectedPositionFloored.x, projectedPositionFloored.y,
					                         projectedPositionFloored.z, plane))
						continue;

					Vector2i imgCoords = GetVoxelImageCoordinates(projectedPosition, plane);
					const int voxelOnImageSize = static_cast<int>(pixelsPerVoxel);
					float value = 0.0;//SdfToShadeValue(TVoxel::valueToFloat(voxel.sdf));
					//fill a pixel block with the source scene value
					for (int row = imgCoords.y; row < imgCoords.y + voxelOnImageSize / 2; row++) {
						for (int col = imgCoords.x; col < imgCoords.x + voxelOnImageSize / 2; col++) {
							img.at<float>(row, col) = value;
						}
					}
				}
			}
		}
	}
	return img;
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
cv::Mat ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::DrawCanonicalSceneImageAroundPoint(
		ITMScene<TVoxelCanonical, TIndex>* scene) {
	return DrawSceneImageAroundPoint(scene);
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
cv::Mat ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::DrawLiveSceneImageAroundPoint(
		ITMScene<TVoxelLive, TIndex>* scene, int fieldIndex) {
	return DrawSceneImageAroundPointIndexedFields(scene, fieldIndex);
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
cv::Mat ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::DrawWarpedSceneImageAroundPoint(
		ITMScene<TVoxelCanonical, TIndex>* scene) {
	return DrawWarpedSceneImageTemplated<TVoxelCanonical>(scene);
}

/**
 * \brief Mark/highlight the (warped) voxel on an image generated from the given scene's voxels
 * around a specific (potentially, different) point
 * \tparam TVoxelCanonical canonical voxel type
 * \tparam TVoxelLive live voxel type
 * \tparam TIndex index structure type
 * \param scene the voxel grid the image was generated from
 * \param imageToMarkOn the image that was generated from the warped vosel positions that should be marked
 * \param positionOfVoxelToMark voxel position (before warp application) of the voxel to highlight/mark
 */
template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
void ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::MarkWarpedSceneImageAroundPoint(
		ITMScene<TVoxelCanonical, TIndex>* scene, cv::Mat& imageToMarkOn,
		Vector3i positionOfVoxelToMark) {
	bool vmIndex;
	TVoxelCanonical voxel = readVoxel(scene->localVBA.GetVoxelBlocks(), scene->index.GetEntries(),
	                                  positionOfVoxelToMark, vmIndex);
	Vector3f projectedPosition = positionOfVoxelToMark.toFloat() + voxel.framewise_warp;
	Vector3i projectedPositionFloored = projectedPosition.toIntFloor();
	switch (plane){
		case PLANE_XY:
			projectedPositionFloored.z = positionOfVoxelToMark.z;
			break;
		case PLANE_YZ:
			projectedPositionFloored.x = positionOfVoxelToMark.x;
			break;
		case PLANE_XZ:
			projectedPosition.y = positionOfVoxelToMark.y;
			break;
	}
	if (!IsVoxelInImageRange(projectedPositionFloored.x, projectedPositionFloored.y, positionOfVoxelToMark.z, plane))
		return;

	Vector2i imageCoords = GetVoxelImageCoordinates(projectedPosition, this->plane);
	const int voxelOnImageSize = static_cast<int>(pixelsPerVoxel);

	//fill a pixel block with the source scene value
	for (int row = imageCoords.y; row < imageCoords.y + voxelOnImageSize / 2; row++) {
		for (int col = imageCoords.x; col < imageCoords.x + voxelOnImageSize / 2; col++) {
			imageToMarkOn.at<uchar>(row, col) = static_cast<uchar>(255.0f);
		}
	}

}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
bool ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::IsVoxelInImageRange(
		int x, int y, int z,
		Plane TPlane) const {
	switch (TPlane) {
		case PLANE_XY:
			return (z == imgZSlice && x >= imgRangeStartX && x < imgRangeEndX && y >= imgRangeStartY &&
			        y < imgRangeEndY);
		case PLANE_YZ:
			return (z >= imgRangeStartZ && z < imgRangeEndZ && x == imgXSlice && y >= imgRangeStartY &&
			        y < imgRangeEndY);
		case PLANE_XZ:
			return (z >= imgRangeStartZ && z < imgRangeEndZ && x >= imgRangeStartX && x < imgRangeEndX &&
			        y == imgYSlice);
		default:
			DIEWITHEXCEPTION_REPORTLOCATION("Plane value support not implemented");
	}
}



template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
Vector2i
ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::GetVoxelImageCoordinates(
		Vector3i coordinates,
		Plane plane) {
	switch (plane) {
		case PLANE_XY:
			return Vector2i(static_cast<int>(pixelsPerVoxel * (coordinates.x - imgRangeStartX)),
			                imgPixelRangeY - static_cast<int>(pixelsPerVoxel * (coordinates.y - imgRangeStartY)));
		case PLANE_YZ:
			return Vector2i(static_cast<int>(pixelsPerVoxel * (coordinates.y - imgRangeStartY)),
			                imgPixelRangeZ - static_cast<int>(pixelsPerVoxel * (coordinates.z - imgRangeStartZ)));
		case PLANE_XZ:
			return Vector2i(static_cast<int>(pixelsPerVoxel * (coordinates.x - imgRangeStartX)),
			                imgPixelRangeZ - static_cast<int>(pixelsPerVoxel * (coordinates.z - imgRangeStartZ)));
		default:
			DIEWITHEXCEPTION_REPORTLOCATION("Plane value support not implemented");
	}
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
Vector2i
ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::GetVoxelImageCoordinates(
		Vector3f coordinates,
		Plane plane) {
	switch (plane) {
		case PLANE_XY:
			return Vector2i(static_cast<int>(pixelsPerVoxel * (coordinates.x - imgRangeStartX)),
			                imgPixelRangeY - static_cast<int>(pixelsPerVoxel * (coordinates.y - imgRangeStartY)));
		case PLANE_YZ:
			return Vector2i(static_cast<int>(pixelsPerVoxel * (coordinates.y - imgRangeStartY)),
			                imgPixelRangeZ - static_cast<int>(pixelsPerVoxel * (coordinates.z - imgRangeStartZ)));
		case PLANE_XZ:
			return Vector2i(static_cast<int>(pixelsPerVoxel * (coordinates.x - imgRangeStartX)),
			                imgPixelRangeZ - static_cast<int>(pixelsPerVoxel * (coordinates.z - imgRangeStartZ)));
		default:
			DIEWITHEXCEPTION_REPORTLOCATION("Plane value support not implemented");
	}
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
bool ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::IsVoxelBlockInImageRange(
		Vector3i blockVoxelCoords,
		Plane TPlane) const {
	Vector3i& bvc0 = blockVoxelCoords;
	Vector3i bvc1 = blockVoxelCoords + Vector3i(SDF_BLOCK_SIZE);
	switch (TPlane) {
		case PLANE_XY:
			return !(imgZSlice >= bvc1.z || imgZSlice < bvc0.z) &&
			       !(imgRangeStartX >= bvc1.x || imgRangeEndX < bvc0.x) &&
			       !(imgRangeStartY >= bvc1.y || imgRangeEndY < bvc0.y);
		case PLANE_YZ:
			return !(imgXSlice >= bvc1.x || imgXSlice < bvc0.x) &&
			       !(imgRangeStartZ >= bvc1.z || imgRangeEndZ < bvc0.z) &&
			       !(imgRangeStartY >= bvc1.y || imgRangeEndY < bvc0.y);
		case PLANE_XZ:
			return !(imgYSlice >= bvc1.y || imgYSlice < bvc0.y) &&
			       !(imgRangeStartX >= bvc1.x || imgRangeEndX < bvc0.x) &&
			       !(imgRangeStartZ >= bvc1.z || imgRangeEndZ < bvc0.z);
		default:
			DIEWITHEXCEPTION_REPORTLOCATION("Plane value support not implemented");
	}
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
bool
ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::IsVoxelBlockInImageRangeTolerance(
		Vector3i blockVoxelCoords, int tolerance,
		Plane plane) const {
	Vector3i& bvc0 = blockVoxelCoords;
	Vector3i bvc1 = blockVoxelCoords + Vector3i(SDF_BLOCK_SIZE);
	switch (plane) {
		case PLANE_XY:
			return !(imgZSlice >= bvc1.z || imgZSlice < bvc0.z) &&
			       !(imgRangeStartX - tolerance >= bvc1.x || imgRangeEndX + tolerance < bvc0.x) &&
			       !(imgRangeStartY - tolerance >= bvc1.y || imgRangeEndY + tolerance < bvc0.y);
		case PLANE_YZ:
			return !(imgXSlice >= bvc1.x || imgXSlice < bvc0.x) &&
			       !(imgRangeStartZ - tolerance >= bvc1.z || imgRangeEndZ + tolerance < bvc0.z) &&
			       !(imgRangeStartY - tolerance >= bvc1.y || imgRangeEndY + tolerance < bvc0.y);
		case PLANE_XZ:
			return !(imgYSlice >= bvc1.y || imgYSlice < bvc0.y) &&
			       !(imgRangeStartX - tolerance >= bvc1.x || imgRangeEndX + tolerance < bvc0.x) &&
			       !(imgRangeStartZ - tolerance >= bvc1.z || imgRangeEndZ + tolerance < bvc0.z);
	}

}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
template<typename TVoxel>
void
ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::RenderSceneSlices(ITMScene<TVoxel, TIndex>* scene,
                                                                              Axis axis,
                                                                              const std::string& outputFolder,
                                                                              bool verbose) {

	Vector3i minPoint, maxPoint;

	ITMSceneStatisticsCalculator<TVoxel, TIndex> calculator;
	calculator.ComputeVoxelBounds(scene, minPoint, maxPoint);
	std::cout << "Voxel ranges ( min x,y,z; max x,y,z): " << minPoint << "; " << maxPoint << std::endl;

	int imageSizeX, imageSizeY, imageSizeZ;
	imageSizeX = maxPoint.x - minPoint.x;
	imageSizeY = maxPoint.y - minPoint.y;
	imageSizeZ = maxPoint.z - minPoint.z;

	cv::Size imSize;
	int imageCount = 0;
	int indexOffset = 0;
	switch (axis) {
		case AXIS_X:
			imSize = cv::Size(imageSizeZ, imageSizeY);
			imageCount = imageSizeX;
			indexOffset = minPoint.x;
			break;
		case AXIS_Y:
			imSize = cv::Size(imageSizeX, imageSizeZ);
			imageCount = imageSizeY;
			indexOffset = minPoint.y;
			break;
		case AXIS_Z:
			imSize = cv::Size(imageSizeX, imageSizeY);
			imageCount = imageSizeZ;
			indexOffset = minPoint.z;
			break;
	}
	std::vector<cv::Mat> images;
	images.reserve(static_cast<unsigned long>(imageCount));

	//generate image stack
	for (int iImage = 0; iImage < imageCount; iImage++) {
		images.push_back(cv::Mat::zeros(imSize, CV_8UC3));
	}

	TVoxel* voxelBlocks = scene->localVBA.GetVoxelBlocks();
	const ITMHashEntry* hashTable = scene->index.GetEntries();
	int noTotalEntries = scene->index.noTotalEntries;

	//fill the images with awesomeness
#ifdef WITH_OPENMP
#pragma omp parallel for
#endif
	for (int entryId = 0; entryId < noTotalEntries; entryId++) {

		const ITMHashEntry& currentHashEntry = hashTable[entryId];

		if (currentHashEntry.ptr < 0) continue;

		//position of the current entry in 3D space
		Vector3i currentHashBlockPositionVoxels = currentHashEntry.pos.toInt() * SDF_BLOCK_SIZE;

		TVoxel* localVoxelBlock = &(voxelBlocks[currentHashEntry.ptr * (SDF_BLOCK_SIZE3)]);
		for (int z = 0; z < SDF_BLOCK_SIZE; z++) {
			for (int y = 0; y < SDF_BLOCK_SIZE; y++) {
				for (int x = 0; x < SDF_BLOCK_SIZE; x++) {
					Vector3i voxelPosition = currentHashBlockPositionVoxels + Vector3i(x, y, z);
					Vector3i absoluteCoordinate = voxelPosition - minPoint; // move origin to minPoint
					int locId = x + y * SDF_BLOCK_SIZE + z * SDF_BLOCK_SIZE * SDF_BLOCK_SIZE;
					TVoxel& voxel = localVoxelBlock[locId];
					//determine pixel coordinate
					Vector2i pixelCoordinate;
					int iImage;
					switch (axis) {
						case AXIS_X:
							//Y-Z plane
							pixelCoordinate.x = absoluteCoordinate.z;
							pixelCoordinate.y = absoluteCoordinate.y;
							iImage = absoluteCoordinate.x;
							break;
						case AXIS_Y:
							//Z-X plane
							pixelCoordinate.x = absoluteCoordinate.x;
							pixelCoordinate.y = absoluteCoordinate.z;
							iImage = absoluteCoordinate.y;
							break;
						case AXIS_Z:
							//X-Y plane
							pixelCoordinate.x = absoluteCoordinate.x;
							pixelCoordinate.y = absoluteCoordinate.y;
							iImage = absoluteCoordinate.z;
							break;
					}
					float value = SdfToShadeValue(TVoxel::valueToFloat(voxel.sdf));
					uchar colorChar = static_cast<uchar>(value * 255.0);
					cv::Vec3b color = cv::Vec3b::all(colorChar);
					if (voxelPosition == focusCoordinates) {
						color.val[0] = 0;
						color.val[1] = 0;
						color.val[2] = static_cast<uchar>(255.0);
					}
					images[iImage].at<cv::Vec3b>(pixelCoordinate.y, pixelCoordinate.x) = color;

				}
			}
		}
	}
	for (int iImage = 0; iImage < imageCount; iImage++) {
		std::stringstream ss;
		ss << outputFolder << "/slice" << std::setfill('0') << std::setw(5) << iImage + indexOffset << ".png";
		cv::imwrite(ss.str(), images[iImage]);
		if (verbose) {
			std::cout << "Writing " << ss.str() << std::endl;
		}
	}
}


template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
void ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::SaveLiveSceneSlicesAs2DImages_AllDirections(
		ITMScene<TVoxelLive, TIndex>* scene, std::string pathWithoutPostfix) {
	ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::SaveLiveSceneSlicesAs2DImages(
			scene, AXIS_X, pathWithoutPostfix + "_X");
	ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::SaveLiveSceneSlicesAs2DImages(
			scene, AXIS_Y, pathWithoutPostfix + "_Y");
	ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::SaveLiveSceneSlicesAs2DImages(
			scene, AXIS_Z, pathWithoutPostfix + "_Z");
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
void ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::SaveCanonicalSceneSlicesAs2DImages_AllDirections(
		ITMScene<TVoxelCanonical, TIndex>* scene, std::string pathWithoutPostfix) {
	ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::SaveCanonicalSceneSlicesAs2DImages(
			scene, AXIS_X, pathWithoutPostfix + "_X");
	ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::SaveCanonicalSceneSlicesAs2DImages(
			scene, AXIS_Y, pathWithoutPostfix + "_Y");
	ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::SaveCanonicalSceneSlicesAs2DImages(
			scene, AXIS_Z, pathWithoutPostfix + "_Z");
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
void ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::SaveCanonicalSceneSlicesAs2DImages(
		ITMScene<TVoxelCanonical, TIndex>* scene, Axis axis, std::string path) {
	RenderSceneSlices<TVoxelCanonical>(scene, axis, path, false);
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
void
ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::SaveLiveSceneSlicesAs2DImages(
		ITMScene<TVoxelLive, TIndex>* scene, Axis axis, std::string path) {
	RenderSceneSlices<TVoxelLive>(scene, axis, path, false);
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
void ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::MarkWarpedSceneImageAroundFocusPoint(
		ITMScene<TVoxelCanonical, TIndex>* scene, cv::Mat& imageToMarkOn) {
	MarkWarpedSceneImageAroundPoint(scene, imageToMarkOn, focusCoordinates);

}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
float ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::SdfToShadeValue(float sdf) {
	return absFillingStrategy ? std::abs(sdf) : (sdf + 1.0f) / 2.0f;
}

template<typename TVoxelCanonical, typename TVoxelLive, typename TIndex>
Plane ITMScene2DSliceVisualizer<TVoxelCanonical, TVoxelLive, TIndex>::GetPlane() const {
	return this->plane;
}





