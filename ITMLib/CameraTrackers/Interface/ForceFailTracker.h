// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

#include "CameraTracker.h"

namespace ITMLib
{
	/**
	 * \brief An instance of this class can be used to force tracking failure,
	 *        e.g. when testing a relocaliser.
	 */
	class ForceFailTracker : public CameraTracker
	{
		//#################### PUBLIC MEMBER FUNCTIONS ####################
	public:
		/** Override */
		virtual void TrackCamera(CameraTrackingState *trackingState, const View *view);

		/** Override */
		virtual bool requiresColourRendering() const;

		/** Override */
		virtual bool requiresDepthReliability() const;

		/** Override */
		virtual bool requiresPointCloudRendering() const;
	};
}
