// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

#include "CameraTracker.h"

namespace ITMLib
{
	/**
	 * \brief Tracker that reads precomputed poses from text files.
	 */
	class FileBasedTracker : public CameraTracker
	{
	private:
		std::string poseMask;
		size_t frameCount;

	public:
		bool CanKeepTracking() const;
		void TrackCamera(CameraTrackingState *trackingState, const View *view);

		bool requiresColourRendering() const { return false; }
		bool requiresDepthReliability() const { return false; }
		bool requiresPointCloudRendering() const { return false; }

		explicit FileBasedTracker(const std::string &poseMask);

	private:
		std::string GetCurrentFilename() const;
	};
}
