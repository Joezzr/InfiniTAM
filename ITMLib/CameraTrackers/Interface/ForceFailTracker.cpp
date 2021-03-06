// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#include "ForceFailTracker.h"

namespace ITMLib
{

//#################### PUBLIC MEMBER FUNCTIONS ####################

void ForceFailTracker::TrackCamera(CameraTrackingState *trackingState, const View *view)
{
	trackingState->trackerResult = CameraTrackingState::TRACKING_FAILED;
}

bool ForceFailTracker::requiresColourRendering() const
{
	return false;
}

bool ForceFailTracker::requiresDepthReliability() const
{
	return false;
}

bool ForceFailTracker::requiresPointCloudRendering() const
{
	return false;
}

}
