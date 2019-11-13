//  ================================================================
//  Created by Gregory Kramida on 11/13/19.
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

#define BOOST_TEST_MODULE Configuration
#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

//boost
#include <boost/test/unit_test.hpp>

//local
#include "../ITMLib/Utils/Configuration.h"

using namespace ITMLib;

BOOST_AUTO_TEST_CASE(ConfigurationTest) {
	Configuration default_configuration;
//	default_configuration::save_to_json_file("../../Tests/TestData/default_config.json");
	Configuration::load_from_json_file("TestData/default_config.json");
	BOOST_REQUIRE(default_configuration == Configuration::get());
	Configuration configuration1(
			ITMSceneParameters(0.05, 200, 0.005, 0.12, 4.12, true),
			ITMSurfelSceneParameters(0.4f, 0.5f, static_cast<float>(22 * M_PI / 180), 0.008f, 0.0003f, 3.4f, 26.0f, 5,
			                         1.1f, 4.5f, 21, 300, false, false),
			SlavchevaSurfaceTracker::Parameters(0.11f, 0.09f, 2.0f, 0.3f, 0.1f, 1e-6f),
			SlavchevaSurfaceTracker::Switches(false, true, false, true, false),
			Configuration::TelemetrySettings("output1", true, Vector3i(20, 23, 0)),
			true,
			false,
			MEMORYDEVICE_CPU,
			true,
			true,
			true,
			Configuration::FAILUREMODE_RELOCALIZE,
			Configuration::SWAPPINGMODE_ENABLED,
			Configuration::LIBMODE_BASIC,
			Configuration::INDEX_ARRAY,
			"type=rgb,levels=rrbb",
			300,
			0.00007f
	);
	configuration1.save_to_json_file("../../Tests/TestData/config1.json");
//	Configuration::load_from_json_file("TestData/config1.json");
//	BOOST_REQUIRE_EQUAL(configuration1,Configuration::get());

}