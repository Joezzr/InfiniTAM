// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

//boost
#include <boost/program_options.hpp>
namespace po = boost::program_options;

namespace ITMLib
{
	/** \brief
	    Stores parameters of a scene like voxel size
	*/
	class ITMSceneParameters
	{
	public:
		/// Size of a voxel, usually given in meters.
		const float voxelSize;

		/** @{ */
		/** \brief
		    Fallback parameters: consider only parts of the
		    scene from @p viewFrustum_min in front of the camera
		    to a distance of @p viewFrustum_max. Usually the
		    actual depth range should be determined
		    automatically by a ITMLib::Engine::ITMVisualisationEngine.
		*/
		const float viewFrustum_min, viewFrustum_max;

		/** @} */
		/** \brief
		    Encodes the width of the band of the truncated
		    signed distance transform that is actually stored
		    in the volume. This is again usually specified in
		    meters. The resulting width in voxels is @ref mu
		    divided by @ref voxelSize.
		*/
		const float mu;

		/** \brief
		    Up to @ref maxW observations per voxel are averaged.
		    Beyond that a sliding average is computed.
		*/
		const int maxW;

		/** Stop integration/fusion once maxW has been reached. */
		const bool stopIntegratingAtMaxW;

		ITMSceneParameters();
		explicit ITMSceneParameters(const po::variables_map& vm);

		/**
		 * \brief standard constructor setting all elements to passed-in values
		 * \param mu width of the band (in meters) of the truncated signed distance transform
		 * \param maxW maximum number of observations per voxel which are averaged, after this a sliding average is computed
		 * \param voxelSize voxelSize (in meters)
		 * \param viewFrustum_min distance (in meters) to near clipping plane of the view frustum, closer than which nothing is considered
		 * \param viewFrustum_max distance (in meters) to far clipping plane of the view frustum, farther than which nothing is considered
		 * \param stopIntegratingAtMaxW defines behavior after maxW observations have been gathered for a specific point
		 */
		ITMSceneParameters(float mu, int maxW, float voxelSize,
		                   float viewFrustum_min, float viewFrustum_max, bool stopIntegratingAtMaxW);

	};
}