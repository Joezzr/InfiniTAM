//  ================================================================
//  Created by Gregory Kramida on 9/4/19.
//  Copyright (c) 2019 Gregory Kramida
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
#define BOOST_TEST_MODULE DepthFusion
#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

//std
#include <iostream>

//boost
#include <boost/test/unit_test.hpp>

//ITMLib
#include "../ITMLib/GlobalTemplateDefines.h"
#include "../ITMLib/Engines/DepthFusion/DepthFusionEngine.h"
#include "../ITMLib/Engines/DepthFusion/DepthFusionEngineFactory.h"
#include "../ITMLib/Engines/ViewBuilder/Interface/ViewBuilder.h"
#include "../ITMLib/Engines/ViewBuilder/ViewBuilderFactory.h"
#include "../ITMLib/Engines/EditAndCopy/EditAndCopyEngineFactory.h"
#include "../ITMLib/Engines/Indexing/Interface/IndexingEngine.h"
#include "../ITMLib/Engines/Indexing/IndexingEngineFactory.h"
#include "../ITMLib/Engines/VolumeFileIO/VolumeFileIOEngine.h"
#include "../ITMLib/Engines/Analytics/AnalyticsEngine.h"
#include "../ITMLib/Engines/DepthFusion/DepthFusionEngineFactory.h"
#include "../ITMLib/Engines/Rendering/RenderingEngineFactory.h"
#include "../ITMLib/Utils/Configuration/Configuration.h"
#include "../ITMLib/Utils/Analytics/AlmostEqual.h"
#include "../ITMLib/Utils/Analytics/VoxelVolumeComparison/VoxelVolumeComparison.h"
#include "../ORUtils/FileUtils.h"
#ifndef COMPILE_WITHOUT_CUDA
#include "../ITMLib/Utils/Analytics/VoxelVolumeComparison/VoxelVolumeComparison_CUDA.h"
#endif

//test_utilities
#include "TestUtilities/TestUtilities.h"
#include "TestUtilities/TestDataUtilities.h"

using namespace ITMLib;
using namespace test;


template<MemoryDeviceType TMemoryDeviceType>
void GenericTestBuildSnoopyVolumeFromFrame16_PVA_vs_VBH_Near() {
	VoxelVolume<TSDFVoxel, VoxelBlockHash>* volume_VBH_16;
	BuildSdfVolumeFromImage_NearSurfaceAllocation(&volume_VBH_16,
	                                              std::string(test::snoopy::frame_16_depth_path),
	                                              std::string(test::snoopy::frame_16_color_path),
	                                              std::string(test::snoopy::frame_16_mask_path),
	                                              std::string(test::snoopy::calibration_path),
	                                              TMemoryDeviceType,
	                                              test::snoopy::InitializationParameters_Fr16andFr17<VoxelBlockHash>());

	VoxelVolume<TSDFVoxel, PlainVoxelArray>* volume_PVA_16;
	BuildSdfVolumeFromImage_NearSurfaceAllocation(&volume_PVA_16,
	                                              std::string(test::snoopy::frame_16_depth_path),
	                                              std::string(test::snoopy::frame_16_color_path),
	                                              std::string(test::snoopy::frame_16_mask_path),
	                                              std::string(test::snoopy::calibration_path),
	                                              TMemoryDeviceType,
	                                              test::snoopy::InitializationParameters_Fr16andFr17<PlainVoxelArray>());

	float absoluteTolerance = 1e-7;
	BOOST_REQUIRE(AllocatedContentAlmostEqual(volume_PVA_16, volume_VBH_16, absoluteTolerance, TMemoryDeviceType));
	BOOST_REQUIRE(ContentForFlagsAlmostEqual_Verbose(volume_PVA_16, volume_VBH_16, VoxelFlags::VOXEL_NONTRUNCATED,
	                                                     absoluteTolerance, TMemoryDeviceType));
	BOOST_REQUIRE(ContentForFlagsAlmostEqual_Verbose(volume_PVA_16, volume_VBH_16, VoxelFlags::VOXEL_TRUNCATED,
	                                                     absoluteTolerance, TMemoryDeviceType));

	delete volume_VBH_16;
	delete volume_PVA_16;
};

template<MemoryDeviceType TMemoryDeviceType>
void GenericTestBuildSnoopyVolumeFromFrame17_PVA_vs_VBH_Near() {

	VoxelVolume<TSDFVoxel, PlainVoxelArray>* volume_PVA_17;
	BuildSdfVolumeFromImage_NearSurfaceAllocation(&volume_PVA_17,
	                                              std::string(test::snoopy::frame_17_depth_path),
	                                              std::string(test::snoopy::frame_17_color_path),
	                                              std::string(test::snoopy::frame_17_mask_path),
	                                              std::string(test::snoopy::calibration_path),
	                                              TMemoryDeviceType,
	                                              test::snoopy::InitializationParameters_Fr16andFr17<PlainVoxelArray>());

	VoxelVolume<TSDFVoxel, VoxelBlockHash>* volume_VBH_17;
	BuildSdfVolumeFromImage_NearSurfaceAllocation(&volume_VBH_17,
	                                              std::string(test::snoopy::frame_17_depth_path),
	                                              std::string(test::snoopy::frame_17_color_path),
	                                              std::string(test::snoopy::frame_17_mask_path),
	                                              std::string(test::snoopy::calibration_path),
	                                              TMemoryDeviceType,
	                                              test::snoopy::InitializationParameters_Fr16andFr17<VoxelBlockHash>());

	float absoluteTolerance = 1e-7;
	BOOST_REQUIRE(AllocatedContentAlmostEqual(volume_PVA_17, volume_VBH_17, absoluteTolerance, TMemoryDeviceType));
	BOOST_REQUIRE(ContentForFlagsAlmostEqual_Verbose(volume_PVA_17, volume_VBH_17, VoxelFlags::VOXEL_NONTRUNCATED,
	                                             absoluteTolerance, TMemoryDeviceType));
	BOOST_REQUIRE(ContentForFlagsAlmostEqual_Verbose(volume_PVA_17, volume_VBH_17, VoxelFlags::VOXEL_TRUNCATED,
	                                                     absoluteTolerance, TMemoryDeviceType));

	delete volume_VBH_17;
	delete volume_PVA_17;
}

template<MemoryDeviceType TMemoryDeviceType>
static void SetUpTrackingState16(CameraTrackingState& tracking_state,
                                 IndexingEngine<TSDFVoxel, VoxelBlockHash, TMemoryDeviceType>& indexer_VBH,
                                 DepthFusionEngineInterface<TSDFVoxel, VoxelBlockHash>* depth_fusion_engine_VBH) {
	View* view_16 = nullptr;
	UpdateView(&view_16,
	           std::string(test::snoopy::frame_16_depth_path),
	           std::string(test::snoopy::frame_16_color_path),
	           std::string(test::snoopy::frame_16_mask_path),
	           std::string(test::snoopy::calibration_path),
	           TMemoryDeviceType);
	RenderState render_state(view_16->depth.dimensions,
	                         configuration::Get().general_voxel_volume_parameters.near_clipping_distance,
	                         configuration::Get().general_voxel_volume_parameters.far_clipping_distance,
	                         TMemoryDeviceType);
	VoxelVolume<TSDFVoxel, VoxelBlockHash> volume_VBH_16(TMemoryDeviceType,
	                                                     test::snoopy::InitializationParameters_Fr16andFr17<VoxelBlockHash>());
	volume_VBH_16.Reset();
	indexer_VBH.AllocateNearSurface(&volume_VBH_16, view_16);
	depth_fusion_engine_VBH->IntegrateDepthImageIntoTsdfVolume(&volume_VBH_16, view_16);
	RenderingEngineBase<TSDFVoxel, VoxelBlockHash>* visualization_engine =
			RenderingEngineFactory::Build<TSDFVoxel, VoxelBlockHash>(TMemoryDeviceType);
	visualization_engine->CreateICPMaps(&volume_VBH_16, view_16, &tracking_state, &render_state);
	delete visualization_engine;
	delete view_16;
}

template<MemoryDeviceType TMemoryDeviceType>
void GenericTestBuildVolumeFromImage_VBH_Near_vs_Span() {
	IndexingEngine<TSDFVoxel, VoxelBlockHash, TMemoryDeviceType>& indexer =
			IndexingEngine<TSDFVoxel, VoxelBlockHash, TMemoryDeviceType>::Instance();
	DepthFusionEngineInterface<TSDFVoxel, VoxelBlockHash>* depth_fusion_engine_VBH =
			DepthFusionEngineFactory::Build<TSDFVoxel, VoxelBlockHash>(TMemoryDeviceType);
	Vector2i resolution(640, 480);
	CameraTrackingState tracking_state(resolution, TMemoryDeviceType);

	SetUpTrackingState16<TMemoryDeviceType>(tracking_state, indexer, depth_fusion_engine_VBH);

	View* view_17 = nullptr;
	UpdateView(&view_17,
	           std::string(test::snoopy::frame_17_depth_path),
	           std::string(test::snoopy::frame_17_color_path),
	           std::string(test::snoopy::frame_17_mask_path),
	           std::string(test::snoopy::calibration_path),
	           TMemoryDeviceType);

	VoxelVolume<TSDFVoxel, VoxelBlockHash> volume_VBH_17_Span(TMemoryDeviceType,
	                                                          test::snoopy::InitializationParameters_Fr16andFr17<VoxelBlockHash>());
	volume_VBH_17_Span.Reset();
	// CPU
	VoxelVolume<TSDFVoxel, VoxelBlockHash> volume_VBH_17_Near(TMemoryDeviceType,
	                                                          test::snoopy::InitializationParameters_Fr16andFr17<VoxelBlockHash>());
	volume_VBH_17_Near.Reset();

	// *** allocate hash blocks ***
	indexer.AllocateNearSurface(&volume_VBH_17_Near, view_17);
	indexer.AllocateNearAndBetweenTwoSurfaces(&volume_VBH_17_Span, view_17, &tracking_state);

	// *** integrate depth ***
	depth_fusion_engine_VBH->IntegrateDepthImageIntoTsdfVolume(&volume_VBH_17_Span, view_17);
	depth_fusion_engine_VBH->IntegrateDepthImageIntoTsdfVolume(&volume_VBH_17_Near, view_17);

	int span_nontruncated_voxel_count =
			AnalyticsEngine<TSDFVoxel, VoxelBlockHash, TMemoryDeviceType>::Instance().CountNonTruncatedVoxels(&volume_VBH_17_Span);
	int near_nontruncated_voxel_count =
			AnalyticsEngine<TSDFVoxel, VoxelBlockHash, TMemoryDeviceType>::Instance().CountNonTruncatedVoxels(&volume_VBH_17_Near);

	BOOST_REQUIRE_EQUAL(span_nontruncated_voxel_count, near_nontruncated_voxel_count);

	float absolute_tolerance = 1e-7;
	BOOST_REQUIRE(ContentForFlagsAlmostEqual_Verbose(&volume_VBH_17_Span, &volume_VBH_17_Near,
	                                                     VoxelFlags::VOXEL_NONTRUNCATED, absolute_tolerance,
	                                                     TMemoryDeviceType));

	delete view_17;
	delete depth_fusion_engine_VBH;
}

template<MemoryDeviceType TMemoryDeviceType>
void GenericTestBuildSnoopyVolumeFromFrame17_PVA_vs_VBH_Span() {
	IndexingEngine<TSDFVoxel, VoxelBlockHash, TMemoryDeviceType>& indexer_VBH =
			IndexingEngine<TSDFVoxel, VoxelBlockHash, TMemoryDeviceType>::Instance();
	DepthFusionEngineInterface<TSDFVoxel, VoxelBlockHash>* depth_fusion_engine_VBH =
			DepthFusionEngineFactory::Build<TSDFVoxel, VoxelBlockHash>(TMemoryDeviceType);
	DepthFusionEngineInterface<TSDFVoxel, PlainVoxelArray>* depth_fusion_engine_PVA =
			DepthFusionEngineFactory::Build<TSDFVoxel, PlainVoxelArray>(TMemoryDeviceType);
	Vector2i resolution(640, 480);
	CameraTrackingState tracking_state(resolution, TMemoryDeviceType);
	SetUpTrackingState16<TMemoryDeviceType>(tracking_state, indexer_VBH, depth_fusion_engine_VBH);

	View* view_17 = nullptr;
	UpdateView(&view_17,
	           std::string(test::snoopy::frame_17_depth_path),
	           std::string(test::snoopy::frame_17_color_path),
	           std::string(test::snoopy::frame_17_mask_path),
	           std::string(test::snoopy::calibration_path),
	           TMemoryDeviceType);

	VoxelVolume<TSDFVoxel, VoxelBlockHash> volume_VBH_17_Span(TMemoryDeviceType,
	                                                          test::snoopy::InitializationParameters_Fr16andFr17<VoxelBlockHash>());
	volume_VBH_17_Span.Reset();
	// CPU
	VoxelVolume<TSDFVoxel, PlainVoxelArray> volume_PVA_17(TMemoryDeviceType,
	                                                      test::snoopy::InitializationParameters_Fr16andFr17<PlainVoxelArray>());
	volume_PVA_17.Reset();

	// *** allocate hash blocks ***
	indexer_VBH.AllocateNearAndBetweenTwoSurfaces(&volume_VBH_17_Span, view_17, &tracking_state);

	// *** integrate depth ***
	depth_fusion_engine_VBH->IntegrateDepthImageIntoTsdfVolume(&volume_VBH_17_Span, view_17);
	depth_fusion_engine_PVA->IntegrateDepthImageIntoTsdfVolume(&volume_PVA_17, view_17);

	std::vector<Vector3s> difference_set = AnalyticsEngine<TSDFVoxel, VoxelBlockHash, TMemoryDeviceType>::Instance()
			.GetDifferenceBetweenAllocatedAndUtilizedHashBlockPositionSets(&volume_VBH_17_Span);

	BOOST_REQUIRE(difference_set.empty());

	int vbh_span_nontruncated_voxel_count =
			AnalyticsEngine<TSDFVoxel, VoxelBlockHash, TMemoryDeviceType>::Instance().CountNonTruncatedVoxels(&volume_VBH_17_Span);
	int pva_nontruncated_voxel_count =
			AnalyticsEngine<TSDFVoxel, PlainVoxelArray, TMemoryDeviceType>::Instance().CountNonTruncatedVoxels(&volume_PVA_17);
	BOOST_REQUIRE_EQUAL(vbh_span_nontruncated_voxel_count, pva_nontruncated_voxel_count);

	float absolute_tolerance = 1e-7;
	BOOST_REQUIRE(AllocatedContentAlmostEqual(&volume_PVA_17, &volume_VBH_17_Span, absolute_tolerance, TMemoryDeviceType));
	BOOST_REQUIRE(ContentForFlagsAlmostEqual_Verbose(&volume_PVA_17, &volume_VBH_17_Span, VoxelFlags::VOXEL_NONTRUNCATED,
	                                                     absolute_tolerance, TMemoryDeviceType));
}

template<MemoryDeviceType TMemoryDeviceType>
void GenericTestSimpleVolumeFromStripeImage() {
	// region ================================= CONSTRUCT VIEW =========================================================

	RGBD_CalibrationInformation calibration_data;
	readRGBDCalib(std::string(test::snoopy::calibration_path).c_str(), calibration_data);

	ViewBuilder* view_builder = ViewBuilderFactory::Build(calibration_data, TMemoryDeviceType);
	Vector2i image_size(640, 480);
	View* view = nullptr;

	UChar4Image rgb(true, false);
	ShortImage depth(true, false);
	BOOST_REQUIRE(ReadImageFromFile(rgb, STATIC_TEST_DATA_PREFIX "TestData/frames/stripes_color.png"));
	BOOST_REQUIRE(ReadImageFromFile(depth, STATIC_TEST_DATA_PREFIX "TestData/frames/stripes_depth.png"));

	view_builder->UpdateView(&view, &rgb, &depth, false, false, false, true);

	// endregion =======================================================================================================

	Vector3i volume_size(1024, 32, 1024), volume_offset(-volume_size.x / 2, -volume_size.y / 2, 0);

	VoxelVolume<TSDFVoxel, PlainVoxelArray> volume1(TMemoryDeviceType, {volume_size, volume_offset});
	volume1.Reset();
	CameraTrackingState tracking_state(image_size, TMemoryDeviceType);

	DepthFusionSettings settings;
	settings.use_surface_thickness_cutoff = false;

	DepthFusionEngineInterface<TSDFVoxel, PlainVoxelArray>* depth_fusion_engine_PVA =
			DepthFusionEngineFactory::Build<TSDFVoxel, PlainVoxelArray>(TMemoryDeviceType, settings);

	IndexingEngineInterface<TSDFVoxel, PlainVoxelArray>& indexer_PVA
			= IndexingEngineFactory::GetDefault<TSDFVoxel, PlainVoxelArray>(TMemoryDeviceType);
	indexer_PVA.AllocateNearAndBetweenTwoSurfaces(&volume1, view, &tracking_state);
	depth_fusion_engine_PVA->IntegrateDepthImageIntoTsdfVolume(&volume1, view, &tracking_state);

	const int num_stripes = 62;

	//These hardcoded values were precomputed mathematically based on the generated stripe images
	int zero_level_set_voxel_x_coords_mm[] = {-142, -160, -176, -191, -205, -217, -227, -236, -244, -250, -254,
	                                          -256, -258, -257, -255, -252, -247, -240, -232, -222, -211, -198,
	                                          -184, -168, -151, -132, -111, -89, -66, -41, -14, 14, 44,
	                                          75, 108, 143, 179, 216, 255, 296, 338, 382, 427, 474,
	                                          522, 572, 624, 677, 731, 787, 845, 904, 965, 1027, 1091,
	                                          1156, 1223, 1292, 1362, 1433, 1506, 1581};
	int zero_level_set_voxel_z_coords_mm[] = {240, 280, 320, 360, 400, 440, 480, 520, 560, 600, 640,
	                                          680, 720, 760, 800, 840, 880, 920, 960, 1000, 1040, 1080,
	                                          1120, 1160, 1200, 1240, 1280, 1320, 1360, 1400, 1440, 1480, 1520,
	                                          1560, 1600, 1640, 1680, 1720, 1760, 1800, 1840, 1880, 1920, 1960,
	                                          2000, 2040, 2080, 2120, 2160, 2200, 2240, 2280, 2320, 2360, 2400,
	                                          2440, 2480, 2520, 2560, 2600, 2640, 2680};

	std::vector<Vector3i> zero_level_set_coordinates;
	auto getVoxelCoord = [](Vector3f coordinateMeters, float voxelSize) {
		return TO_INT_ROUND3(coordinateMeters / voxelSize);
	};
	for (int iVoxel = 0; iVoxel < num_stripes; iVoxel++) {
		Vector3f coordinateMeters(
				static_cast<float>(zero_level_set_voxel_x_coords_mm[iVoxel]) / 1000.0f,
				0.0f,
				static_cast<float>(zero_level_set_voxel_z_coords_mm[iVoxel]) / 1000.0f
		);
		zero_level_set_coordinates.push_back(
				getVoxelCoord(coordinateMeters, configuration::Get().general_voxel_volume_parameters.voxel_size));
	}

	float tolerance = 1e-4;
	int truncation_distance_voxels = static_cast<int>(std::round(
			volume1.GetParameters().truncation_distance / volume1.GetParameters().voxel_size));
	float max_SDF_step = 1.0f / truncation_distance_voxels;

	// check constructed volume integrity
	Vector3i bad_coordinate;
	float bad_sdf, bad_expected_sdf;
	bool unexpected_sdf_at_level_set = false;
	int i_bad_level_set;

	for (int i_coordinate = 0; i_coordinate < zero_level_set_coordinates.size(); i_coordinate++) {
		if (unexpected_sdf_at_level_set) break;
		Vector3i coord = zero_level_set_coordinates[i_coordinate];
		TSDFVoxel voxel = volume1.GetValueAt(coord);
		float sdf = TSDFVoxel::valueToFloat(voxel.sdf);
		if (!AlmostEqual(sdf, 0.0f, tolerance)) {
			unexpected_sdf_at_level_set = true;
			i_bad_level_set = 0;
			bad_sdf = sdf;
			bad_expected_sdf = 0.0f;
			bad_coordinate = coord;
			break;
		}

		// for extremely lateral points Close to the camera, the rays are highly skewed,
		// so the value progression won't hold. Skip those.
		if (i_coordinate > 17) {
			// don't go into low negative sdf values, since those will be overwritten by positive values
			// during sdf construction in certain cases
			for (int i_level_set = -truncation_distance_voxels;
			     i_level_set < (truncation_distance_voxels / 2); i_level_set++) {
				Vector3i augmented_coord(coord.x, coord.y, coord.z + i_level_set);
				float expected_sdf = -static_cast<float>(i_level_set) * max_SDF_step;
				voxel = volume1.GetValueAt(augmented_coord);
				sdf = TSDFVoxel::valueToFloat(voxel.sdf);
				if (!AlmostEqual(sdf, expected_sdf, tolerance)) {
					unexpected_sdf_at_level_set = true;
					i_bad_level_set = i_level_set;
					bad_coordinate = augmented_coord;
					bad_sdf = sdf;
					bad_expected_sdf = expected_sdf;
					break;
				}
			}
		}
	}
	BOOST_REQUIRE_MESSAGE(!unexpected_sdf_at_level_set, "Expected sdf " << bad_expected_sdf << " for voxel " \
 << bad_coordinate << " at level set " << i_bad_level_set << ", got: " << bad_sdf);

	VoxelVolume<TSDFVoxel, VoxelBlockHash> volume2(TMemoryDeviceType);
	volume2.Reset();

	DepthFusionEngineInterface<TSDFVoxel, VoxelBlockHash>* depth_fusion_engine_VBH =
			DepthFusionEngineFactory::Build<TSDFVoxel, VoxelBlockHash>(TMemoryDeviceType, settings);

	IndexingEngineInterface<TSDFVoxel, VoxelBlockHash>& indexer_VBH
			= IndexingEngineFactory::GetDefault<TSDFVoxel, VoxelBlockHash>(TMemoryDeviceType);
	indexer_VBH.AllocateNearAndBetweenTwoSurfaces(&volume2, view, &tracking_state);
	depth_fusion_engine_VBH->IntegrateDepthImageIntoTsdfVolume(&volume2, view, &tracking_state);

	tolerance = 1e-5;
	BOOST_REQUIRE(AllocatedContentAlmostEqual(&volume1, &volume2, tolerance, TMemoryDeviceType));
	VoxelVolume<TSDFVoxel, PlainVoxelArray> volume3(TMemoryDeviceType, {volume_size, volume_offset});
	volume3.Reset();
	indexer_PVA.AllocateNearAndBetweenTwoSurfaces(&volume3, view, &tracking_state);
	depth_fusion_engine_PVA->IntegrateDepthImageIntoTsdfVolume(&volume3, view, &tracking_state);
	BOOST_REQUIRE(ContentAlmostEqual(&volume1, &volume3, tolerance, TMemoryDeviceType));
	VoxelVolume<TSDFVoxel, VoxelBlockHash> volume4(TMemoryDeviceType, {0x800, 0x20000});
	volume4.Reset();
	indexer_VBH.AllocateNearAndBetweenTwoSurfaces(&volume4, view, &tracking_state);
	depth_fusion_engine_VBH->IntegrateDepthImageIntoTsdfVolume(&volume4, view, &tracking_state);
	BOOST_REQUIRE(ContentAlmostEqual(&volume2, &volume4, tolerance, TMemoryDeviceType));

	Vector3i coordinate = zero_level_set_coordinates[0];
	TSDFVoxel voxel = volume3.GetValueAt(coordinate);
	voxel.sdf = TSDFVoxel::floatToValue(TSDFVoxel::valueToFloat(voxel.sdf) + 0.05f);
	volume3.SetValueAt(coordinate, voxel);
	volume2.SetValueAt(coordinate, voxel);

	BOOST_REQUIRE(!ContentAlmostEqual(&volume1, &volume3, tolerance, TMemoryDeviceType));
	BOOST_REQUIRE(!ContentAlmostEqual(&volume2, &volume4, tolerance, TMemoryDeviceType));
	BOOST_REQUIRE(!AllocatedContentAlmostEqual(&volume1, &volume2, tolerance, TMemoryDeviceType));

	VoxelVolume<TSDFVoxel, VoxelBlockHash> volume5(TMemoryDeviceType);
	volume5.Reset();
	std::string path = GENERATED_TEST_DATA_PREFIX "TestData/volumes/VBH/stripes_CPU.dat";
	volume4.SaveToDisk(path);
	volume5.LoadFromDisk(path);
	BOOST_REQUIRE(AllocatedContentAlmostEqual(&volume1, &volume5, tolerance, TMemoryDeviceType));
	BOOST_REQUIRE(ContentAlmostEqual(&volume4, &volume5, tolerance, TMemoryDeviceType));

	delete view;
	delete depth_fusion_engine_PVA;
	delete depth_fusion_engine_VBH;
	delete view_builder;
}

template<MemoryDeviceType TMemoryDeviceType>
void GenericTestBuildSnoopyVolumeFromFrame00_PVA_vs_VBH() {
	// region ================================= CONSTRUCT VIEW =========================================================

	RGBD_CalibrationInformation calibration_data;
	readRGBDCalib(std::string(test::snoopy::calibration_path).c_str(), calibration_data);

	ViewBuilder* view_builder = ViewBuilderFactory::Build(calibration_data, TMemoryDeviceType);
	Vector2i image_size(640, 480);
	View* view = nullptr;

	UChar4Image rgb(true, false);
	ShortImage depth(true, false);
	BOOST_REQUIRE(ReadImageFromFile(rgb, std::string(test::snoopy::frame_00_color_path).c_str()));
	BOOST_REQUIRE(ReadImageFromFile(depth, std::string(test::snoopy::frame_00_depth_path).c_str()));

	view_builder->UpdateView(&view, &rgb, &depth, false, false, false, true);

	// endregion =======================================================================================================

	VoxelVolume<TSDFVoxel, PlainVoxelArray> generated_volume(TMemoryDeviceType,
	                                                         test::snoopy::InitializationParameters_Fr00<PlainVoxelArray>());

	generated_volume.Reset();
	CameraTrackingState tracking_state(image_size, TMemoryDeviceType);

	DepthFusionEngineInterface<TSDFVoxel, PlainVoxelArray>* depth_fusion_engine_PVA =
			DepthFusionEngineFactory::Build<TSDFVoxel, PlainVoxelArray>(TMemoryDeviceType);
	IndexingEngineInterface<TSDFVoxel, PlainVoxelArray>& indexer_PVA
			= IndexingEngineFactory::GetDefault<TSDFVoxel, PlainVoxelArray>(TMemoryDeviceType);
	indexer_PVA.AllocateNearAndBetweenTwoSurfaces(&generated_volume, view, &tracking_state);
	depth_fusion_engine_PVA->IntegrateDepthImageIntoTsdfVolume(&generated_volume, view, &tracking_state);


	VoxelVolume<TSDFVoxel, PlainVoxelArray> loaded_volume(TMemoryDeviceType, test::snoopy::InitializationParameters_Fr00<PlainVoxelArray>());

	std::string path = test::snoopy::PartialVolume00Path<PlainVoxelArray>();
	loaded_volume.Reset();
	loaded_volume.LoadFromDisk(path);

	float tolerance = 1e-5;
	BOOST_REQUIRE(ContentAlmostEqual_Verbose(&generated_volume, &loaded_volume, tolerance, TMemoryDeviceType));

	VoxelVolume<TSDFVoxel, VoxelBlockHash> volume3(TMemoryDeviceType,
	                                               test::snoopy::InitializationParameters_Fr00<VoxelBlockHash>());
	volume3.Reset();

	DepthFusionEngineInterface<TSDFVoxel, VoxelBlockHash>* depth_fusion_engine_VBH =
			DepthFusionEngineFactory::Build<TSDFVoxel, VoxelBlockHash>(TMemoryDeviceType);
	IndexingEngineInterface<TSDFVoxel, VoxelBlockHash>& indexer_VBH
			= IndexingEngineFactory::GetDefault<TSDFVoxel, VoxelBlockHash>(TMemoryDeviceType);


	indexer_VBH.AllocateNearAndBetweenTwoSurfaces(&volume3, view, &tracking_state);
	depth_fusion_engine_VBH->IntegrateDepthImageIntoTsdfVolume(&volume3, view, &tracking_state);

	BOOST_REQUIRE(AllocatedContentAlmostEqual(&loaded_volume, &volume3, tolerance, TMemoryDeviceType));
	delete depth_fusion_engine_VBH;
	delete depth_fusion_engine_PVA;
}

BOOST_AUTO_TEST_CASE(TestBuildSnoopyVolumeFromFrame16_PVA_vs_VBH_Near_CPU) {
	GenericTestBuildSnoopyVolumeFromFrame16_PVA_vs_VBH_Near<MEMORYDEVICE_CPU>();
}

BOOST_AUTO_TEST_CASE(TestBuildSnoopyVolumeFromFrame17_PVA_vs_VBH_Near_CPU) {
	GenericTestBuildSnoopyVolumeFromFrame17_PVA_vs_VBH_Near<MEMORYDEVICE_CPU>();
}

BOOST_AUTO_TEST_CASE(TestBuildVolumeFromImage_VBH_Near_vs_Span_CPU) {
	GenericTestBuildVolumeFromImage_VBH_Near_vs_Span<MEMORYDEVICE_CPU>();
}

BOOST_AUTO_TEST_CASE(TestBuildSnoopyVolumeFromFrame17_PVA_vs_VBH_Span_CPU) {
	GenericTestBuildSnoopyVolumeFromFrame17_PVA_vs_VBH_Span<MEMORYDEVICE_CPU>();
}


BOOST_AUTO_TEST_CASE(TestSimpleVolumeFromStripeImage_CPU) {
	GenericTestSimpleVolumeFromStripeImage<MEMORYDEVICE_CPU>();
}

BOOST_AUTO_TEST_CASE(TestBuildSnoopyVolumeFromFrame00_PVA_vs_VBH_CPU) {
	GenericTestBuildSnoopyVolumeFromFrame00_PVA_vs_VBH<MEMORYDEVICE_CPU>();
}

#ifndef COMPILE_WITHOUT_CUDA

BOOST_AUTO_TEST_CASE(TestBuildSnoopyVolumeFromFrame16_PVA_vs_VBH_Near_CUDA) {
	GenericTestBuildSnoopyVolumeFromFrame16_PVA_vs_VBH_Near<MEMORYDEVICE_CUDA>();
}

BOOST_AUTO_TEST_CASE(TestBuildSnoopyVolumeFromFrame17_PVA_vs_VBH_Near_CUDA) {
	GenericTestBuildSnoopyVolumeFromFrame17_PVA_vs_VBH_Near<MEMORYDEVICE_CUDA>();
}

BOOST_AUTO_TEST_CASE(TestBuildVolumeFromImage_VBH_Near_vs_Span_CUDA) {
	GenericTestBuildVolumeFromImage_VBH_Near_vs_Span<MEMORYDEVICE_CUDA>();
}

BOOST_AUTO_TEST_CASE(TestBuildSnoopyVolumeFromFrame17_PVA_vs_VBH_Span_CUDA) {
	GenericTestBuildSnoopyVolumeFromFrame17_PVA_vs_VBH_Span<MEMORYDEVICE_CUDA>();
}


BOOST_AUTO_TEST_CASE(TestSimpleVolumeFromStripeImage_CUDA) {
	GenericTestSimpleVolumeFromStripeImage<MEMORYDEVICE_CUDA>();
}

BOOST_AUTO_TEST_CASE(TestBuildSnoopyVolumeFromFrame00_PVA_vs_VBH_CUDA) {
	GenericTestBuildSnoopyVolumeFromFrame00_PVA_vs_VBH<MEMORYDEVICE_CUDA>();
}
#endif
