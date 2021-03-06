//  ================================================================
//  Created by Gregory Kramida (https://github.com/Algomorph) on 5/25/20.
//  Copyright (c) 2020 Gregory Kramida
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

#include "RawArrayComparison.h"

namespace ITMLib {
// *** exact comparisons ***
// primitive specializations
template bool
RawArraysEqual<bool>(const bool* l, MemoryDeviceType memory_device_type_l, const bool* r,
                           MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual<short>(const short* l, MemoryDeviceType memory_device_type_l, const short* r,
                            MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual<int>(const int* l, MemoryDeviceType memory_device_type_l, const int* r,
                          MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

// Vector specializations
template bool
RawArraysEqual<Vector3u>(const Vector3u* l, MemoryDeviceType memory_device_type_l, const Vector3u* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Vector4u>(const Vector4u* l, MemoryDeviceType memory_device_type_l, const Vector4u* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual<Vector2s>(const Vector2s* l, MemoryDeviceType memory_device_type_l, const Vector2s* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Vector3s>(const Vector3s* l, MemoryDeviceType memory_device_type_l, const Vector3s* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Vector4s>(const Vector4s* l, MemoryDeviceType memory_device_type_l, const Vector4s* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual<Vector2i>(const Vector2i* l, MemoryDeviceType memory_device_type_l, const Vector2i* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Vector3i>(const Vector3i* l, MemoryDeviceType memory_device_type_l, const Vector3i* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Vector4i>(const Vector4i* l, MemoryDeviceType memory_device_type_l, const Vector4i* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Vector6i>(const Vector6i* l, MemoryDeviceType memory_device_type_l, const Vector6i* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual<Vector2f>(const Vector2f* l, MemoryDeviceType memory_device_type_l, const Vector2f* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Vector3f>(const Vector3f* l, MemoryDeviceType memory_device_type_l, const Vector3f* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Vector4f>(const Vector4f* l, MemoryDeviceType memory_device_type_l, const Vector4f* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Vector6f>(const Vector6f* l, MemoryDeviceType memory_device_type_l, const Vector6f* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual<Vector2d>(const Vector2d* l, MemoryDeviceType memory_device_type_l, const Vector2d* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Vector3d>(const Vector3d* l, MemoryDeviceType memory_device_type_l, const Vector3d* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Vector4d>(const Vector4d* l, MemoryDeviceType memory_device_type_l, const Vector4d* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

// Matrix specializations
template bool
RawArraysEqual<Matrix3f>(const Matrix3f* l, MemoryDeviceType memory_device_type_l, const Matrix3f* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual<Matrix4f>(const Matrix4f* l, MemoryDeviceType memory_device_type_l, const Matrix4f* r,
                               MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

// *** verbose exact comparisons ***
// primitive specializations
template bool
RawArraysEqual_Verbose<bool>(const bool* l, MemoryDeviceType memory_device_type_l, const bool* r,
                                   MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual_Verbose<short>(const short* l, MemoryDeviceType memory_device_type_l, const short* r,
                                    MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual_Verbose<int>(const int* l, MemoryDeviceType memory_device_type_l, const int* r,
                                  MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

// Vector specializations
template bool
RawArraysEqual_Verbose<Vector3u>(const Vector3u* l, MemoryDeviceType memory_device_type_l, const Vector3u* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Vector4u>(const Vector4u* l, MemoryDeviceType memory_device_type_l, const Vector4u* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual_Verbose<Vector2s>(const Vector2s* l, MemoryDeviceType memory_device_type_l, const Vector2s* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Vector3s>(const Vector3s* l, MemoryDeviceType memory_device_type_l, const Vector3s* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Vector4s>(const Vector4s* l, MemoryDeviceType memory_device_type_l, const Vector4s* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual_Verbose<Vector2i>(const Vector2i* l, MemoryDeviceType memory_device_type_l, const Vector2i* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Vector3i>(const Vector3i* l, MemoryDeviceType memory_device_type_l, const Vector3i* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Vector4i>(const Vector4i* l, MemoryDeviceType memory_device_type_l, const Vector4i* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Vector6i>(const Vector6i* l, MemoryDeviceType memory_device_type_l, const Vector6i* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual_Verbose<Vector2f>(const Vector2f* l, MemoryDeviceType memory_device_type_l, const Vector2f* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Vector3f>(const Vector3f* l, MemoryDeviceType memory_device_type_l, const Vector3f* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Vector4f>(const Vector4f* l, MemoryDeviceType memory_device_type_l, const Vector4f* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Vector6f>(const Vector6f* l, MemoryDeviceType memory_device_type_l, const Vector6f* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

template bool
RawArraysEqual_Verbose<Vector2d>(const Vector2d* l, MemoryDeviceType memory_device_type_l, const Vector2d* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Vector3d>(const Vector3d* l, MemoryDeviceType memory_device_type_l, const Vector3d* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Vector4d>(const Vector4d* l, MemoryDeviceType memory_device_type_l, const Vector4d* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

// Matrix specializations
template bool
RawArraysEqual_Verbose<Matrix3f>(const Matrix3f* l, MemoryDeviceType memory_device_type_l, const Matrix3f* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);
template bool
RawArraysEqual_Verbose<Matrix4f>(const Matrix4f* l, MemoryDeviceType memory_device_type_l, const Matrix4f* r,
                                       MemoryDeviceType memory_device_type_r, const int element_count, bool presort);

// *** approximate comparisons ***
// Vector specializations
template bool
RawArraysAlmostEqual<Vector3u>(const Vector3u* l, MemoryDeviceType memory_device_type_l, const Vector3u* r,
                                     MemoryDeviceType memory_device_type_r, const int element_count,
                                     const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual<Vector4u>(const Vector4u* l, MemoryDeviceType memory_device_type_l, const Vector4u* r,
                                     MemoryDeviceType memory_device_type_r, const int element_count,
                                     const float absolute_tolerance, bool presort);

template bool
RawArraysAlmostEqual<Vector2f>(const Vector2f* l, MemoryDeviceType memory_device_type_l, const Vector2f* r,
                                     MemoryDeviceType memory_device_type_r, const int element_count,
                                     const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual<Vector3f>(const Vector3f* l, MemoryDeviceType memory_device_type_l, const Vector3f* r,
                                     MemoryDeviceType memory_device_type_r, const int element_count,
                                     const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual<Vector4f>(const Vector4f* l, MemoryDeviceType memory_device_type_l, const Vector4f* r,
                                     MemoryDeviceType memory_device_type_r, const int element_count,
                                     const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual<Vector6f>(const Vector6f* l, MemoryDeviceType memory_device_type_l, const Vector6f* r,
                                     MemoryDeviceType memory_device_type_r, const int element_count,
                                     const float absolute_tolerance, bool presort);

template bool
RawArraysAlmostEqual<Vector2d>(const Vector2d* l, MemoryDeviceType memory_device_type_l, const Vector2d* r,
                                     MemoryDeviceType memory_device_type_r, const int element_count,
                                     const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual<Vector3d>(const Vector3d* l, MemoryDeviceType memory_device_type_l, const Vector3d* r,
                                     MemoryDeviceType memory_device_type_r, const int element_count,
                                     const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual<Vector4d>(const Vector4d* l, MemoryDeviceType memory_device_type_l, const Vector4d* r,
                                     MemoryDeviceType memory_device_type_r, const int element_count,
                                     const float absolute_tolerance, bool presort);

// Matrix specializations
template bool
RawArraysAlmostEqual<Matrix3f>(const Matrix3f* l, MemoryDeviceType memory_device_type_l, const Matrix3f* r,
                                     MemoryDeviceType memory_device_type_r, const int element_count,
                                     const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual<Matrix4f>(const Matrix4f* l, MemoryDeviceType memory_device_type_l, const Matrix4f* r,
                                     MemoryDeviceType memory_device_type_r, const int element_count,
                                     const float absolute_tolerance, bool presort);


// *** verbose approximate comparisons ***
// Vector specializations
template bool
RawArraysAlmostEqual_Verbose<Vector3u>(const Vector3u* l, MemoryDeviceType memory_device_type_l, const Vector3u* r,
                                             MemoryDeviceType memory_device_type_r, const int element_count,
                                             const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual_Verbose<Vector4u>(const Vector4u* l, MemoryDeviceType memory_device_type_l, const Vector4u* r,
                                             MemoryDeviceType memory_device_type_r, const int element_count,
                                             const float absolute_tolerance, bool presort);

template bool
RawArraysAlmostEqual_Verbose<Vector2f>(const Vector2f* l, MemoryDeviceType memory_device_type_l, const Vector2f* r,
                                             MemoryDeviceType memory_device_type_r, const int element_count,
                                             const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual_Verbose<Vector3f>(const Vector3f* l, MemoryDeviceType memory_device_type_l, const Vector3f* r,
                                             MemoryDeviceType memory_device_type_r, const int element_count,
                                             const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual_Verbose<Vector4f>(const Vector4f* l, MemoryDeviceType memory_device_type_l, const Vector4f* r,
                                             MemoryDeviceType memory_device_type_r, const int element_count,
                                             const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual_Verbose<Vector6f>(const Vector6f* l, MemoryDeviceType memory_device_type_l, const Vector6f* r,
                                             MemoryDeviceType memory_device_type_r, const int element_count,
                                             const float absolute_tolerance, bool presort);

template bool
RawArraysAlmostEqual_Verbose<Vector2d>(const Vector2d* l, MemoryDeviceType memory_device_type_l, const Vector2d* r,
                                             MemoryDeviceType memory_device_type_r, const int element_count,
                                             const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual_Verbose<Vector3d>(const Vector3d* l, MemoryDeviceType memory_device_type_l, const Vector3d* r,
                                             MemoryDeviceType memory_device_type_r, const int element_count,
                                             const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual_Verbose<Vector4d>(const Vector4d* l, MemoryDeviceType memory_device_type_l, const Vector4d* r,
                                             MemoryDeviceType memory_device_type_r, const int element_count,
                                             const float absolute_tolerance, bool presort);

// Matrix specializations
template bool
RawArraysAlmostEqual_Verbose<Matrix3f>(const Matrix3f* l, MemoryDeviceType memory_device_type_l, const Matrix3f* r,
                                             MemoryDeviceType memory_device_type_r, const int element_count,
                                             const float absolute_tolerance, bool presort);
template bool
RawArraysAlmostEqual_Verbose<Matrix4f>(const Matrix4f* l, MemoryDeviceType memory_device_type_l, const Matrix4f* r,
                                             MemoryDeviceType memory_device_type_r, const int element_count,
                                             const float absolute_tolerance, bool presort);


} // namespace ITMLib
