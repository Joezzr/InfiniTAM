//  ================================================================
//  Created by Gregory Kramida on 12/17/19.
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
#pragma once

//std
#include <string>
#include <unordered_map>

//ITMLib
#include "../../../ITMLib/Engines/LevelSetAlignment/Interface/LevelSetAlignmentEngine.h"

//local
#include "../TestUtilities.h"
#include "LevelSetAlignmentTestMode.h"


using namespace ITMLib;

namespace test {




template<MemoryDeviceType TMemoryDeviceType>
void PVA_to_VBH_WarpComparisonSubtest(int iteration, LevelSetAlignmentSwitches tracker_switches, float absolute_tolerance = 1e-7);


template<MemoryDeviceType TMemoryDeviceType>
void
GenericMultiIterationAlignmentTest(const LevelSetAlignmentSwitches& switches, int iteration_limit, LevelSetAlignmentTestMode mode,
                                   float absolute_tolerance, float tolerance_divergence_factor = 1.00f);


} // namespace test_utilities