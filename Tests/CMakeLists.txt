#####################################
# CMakeLists.txt for Tests
#####################################

option(BUILD_TESTS "Build automated tests with the project (Boost-test required)." OFF)

macro(itm_add_test)
    set(options)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ITM_ADD_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(target test_${ITM_ADD_TEST_NAME})
    set(sources ${ITM_ADD_TEST_SOURCES})

    add_executable(${target} ${sources})

    target_link_libraries(
            ${target} PRIVATE
            MiniSlamGraphLib ORUtils FernRelocLib test_utilities
            ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} InputSource ITMLib
            OpenGL::GL
    )
    if (WITH_OPENMP)
        target_link_libraries(${target} PRIVATE OpenMP::OpenMP_CXX)
    endif ()

    add_test(NAME ${ITM_ADD_TEST_NAME} COMMAND test_${ITM_ADD_TEST_NAME})
endmacro()

if (BUILD_TESTS)
    enable_testing()

    # ======== test_utilities library ========
    set(STATIC_TEST_DATA_PREFIX ${CMAKE_CURRENT_SOURCE_DIR})
    set(GENERATED_TEST_DATA_PREFIX ${CMAKE_CURRENT_BINARY_DIR})
    configure_file(TestUtilities/TestUtilitiesConfig.h.in TestUtilities/TestUtilitiesConfig.h @ONLY)

    set(
            TEST_UTILITIES_SOURCES

            TestUtilities/TestUtilities.cpp
            TestUtilities/TestUtilities.cu
            TestUtilities/TestUtilities.tpp
            TestUtilities/TestDataUtilities.cpp
            TestUtilities/LevelSetAlignment/LevelSetAlignmentTestUtilities.tpp
            TestUtilities/LevelSetAlignment/LevelSetAlignmentTestUtilities.cpp
            TestUtilities/LevelSetAlignment/GenericWarpConsistencySubtest.tpp
            TestUtilities/LevelSetAlignment/GenericWarpConsistencySubtest.cpp
            TestUtilities/LevelSetAlignment/TestCaseOrganizationBySwitches.tpp
            TestUtilities/LevelSetAlignment/TestCaseOrganizationBySwitches.cpp
            TestUtilities/LevelSetAlignment/SingleIterationTestConditions.cpp
    )

    set(
            TEST_UTILITIES_HEADERS

            ${CMAKE_CURRENT_BINARY_DIR}/TestUtilities/TestUtilitiesConfig.h
            TestUtilities/TestUtilityKernels.h
            TestUtilities/TestUtilityFunctors.h
            TestUtilities/TestUtilities.h
            TestUtilities/TestDataUtilities.h
            TestUtilities/LevelSetAlignment/LevelSetAlignmentTestUtilities.h
            TestUtilities/TestUtilitiesConfig.h.in
            TestUtilities/CameraPoseAndRenderingEngineFixture.h
            TestUtilities/LevelSetAlignment/GenericWarpConsistencySubtest.h
            TestUtilities/LevelSetAlignment/LevelSetAlignmentTestMode.h
            TestUtilities/LevelSetAlignment/TestCaseOrganizationBySwitches.h
            TestUtilities/LevelSetAlignment/SingleIterationTestConditions.h
            TestUtilities/GenericTestModuleHeader.h)

    find_package(Boost COMPONENTS unit_test_framework REQUIRED)

    add_library(test_utilities ${TEST_UTILITIES_SOURCES} ${TEST_UTILITIES_HEADERS})
    target_link_libraries(test_utilities ITMLib ORUtils
                          ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
    target_include_directories(test_utilities PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/TestUtilities)

    # ======== generator executable for derived test data library ========

    set(sources GenerateDerivedTestData.cpp)
    add_executable(generate_derived_test_data ${sources})
    target_link_libraries(generate_derived_test_data PRIVATE InputSource ITMLib MiniSlamGraphLib ORUtils FernRelocLib test_utilities)

    # ======== test suite executables =======

    # *** CPU/CUDA tests with CUDA preprocessor guards
    itm_add_test(NAME SetCopyCompare_CPU SOURCES Test_SetCopyCompare_CPU.cpp)
    itm_add_test(NAME DepthFusion SOURCES Test_DepthFusion.cpp)
    itm_add_test(NAME VolumeReduction SOURCES Test_VolumeReduction.cpp)
    itm_add_test(NAME VolumeSaveLoadCompact_CPU SOURCES Test_VolumeSaveLoadCompact_CPU.cpp)
    itm_add_test(NAME IntArrayMap3D SOURCES Test_IntArrayMap3D.cpp)
    itm_add_test(NAME ImageMaskReader SOURCES Test_ImageMaskReader.cpp)
    itm_add_test(NAME LevelSetAlignment_CPU_vs_CUDA SOURCES Test_LevelSetAlignment_CPU_vs_CUDA.cpp Test_LevelSetAlignment_CPU_vs_CUDA_Aux.h)
    itm_add_test(NAME LevelSetAlignment_PVA_vs_VBH SOURCES Test_LevelSetAlignment_PVA_vs_VBH.cpp)
    itm_add_test(NAME VolumeSlicingPVA_CPU SOURCES Test_VoxelVolumeSlicingPVA_CPU.cpp)
    itm_add_test(NAME LevelSetAlignmentAuxiliaryFunctions SOURCES Test_LevelSetAlignmentAuxiliaryFunctions.cpp)
    itm_add_test(NAME WarpVolume SOURCES Test_WarpVolume.cpp)
    itm_add_test(NAME FuseLifeIntoCanonical SOURCES Test_VolumeFusion.cpp)
    itm_add_test(NAME Configuration SOURCES Test_Configuration.cpp)
    itm_add_test(NAME GeometryUtilities SOURCES Test_GeometryUtilities.cpp)
    itm_add_test(NAME HashAllocationThreadSafety SOURCES Test_HashAllocationThreadSafety.cpp)
    itm_add_test(NAME TwoSurfaceHashAllocation SOURCES Test_TwoSurfaceHashAllocation.cpp)
    itm_add_test(NAME MeshGeneration SOURCES Test_MeshGeneration.cpp)
    itm_add_test(NAME EnumSerialization SOURCES Test_EnumSerialization.cpp)
    itm_add_test(NAME RenderingEngine SOURCES Test_RenderingEngine.cpp)
    itm_add_test(NAME ImageProcessingEngine SOURCES Test_ImageProcessingEngine.cpp)
    itm_add_test(NAME FFmpegReadWrite SOURCES Test_FFmpegReadWrite.cpp)
    itm_add_test(NAME RigidAlignment SOURCES Test_RigidAlignment.cpp)

    # *** tests that always require CUDA ***
    if (WITH_CUDA)
        itm_add_test(NAME SetCopyCompare_CUDA SOURCES Test_SetCopyCompare_CUDA.cpp)
        itm_add_test(NAME ConstructVolumeFromImage_CPU_to_CUDA SOURCES
                     Test_DepthFusion_CPU_vs_CUDA.cpp)
        itm_add_test(NAME VolumeSaveLoadCompact_CUDA SOURCES Test_VolumeSaveLoadCompact_CUDA.cpp)
        itm_add_test(NAME CUDA_Atomics SOURCES Test_CUDA_Atomics.cpp CUDAAtomicTesting.h
                     CUDAAtomicTesting.cu TemporaryCUDA_Atomics.h)
        itm_add_test(NAME SmoothEnergyGradient SOURCES Test_SmoothEnergyGradient.cpp)

    endif ()

    # *** special executable for CLI testing
    add_executable(cli_options_subtest TestUtilities/CLI_optionsSubtest.cpp)
    target_link_libraries(cli_options_subtest PRIVATE ITMLib)

endif ()