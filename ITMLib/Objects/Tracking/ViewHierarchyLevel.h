// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

#include "TrackerIterationType.h"
#include "../../Utils/Math.h"
#include "../../../ORUtils/Image.h"

namespace ITMLib
{
	class ViewHierarchyLevel
	{
	public:
		int levelId;

		TrackerIterationType iterationType;

		ORUtils::Image<Vector4u> *rgb; ORUtils::Image<float> *depth;
		ORUtils::Image<Vector4s> *gradientX_rgb, *gradientY_rgb;
		Vector4f intrinsics;

		bool manageData;

		ViewHierarchyLevel(Vector2i imgSize, int levelId, TrackerIterationType iterationType, MemoryDeviceType memoryType, bool skipAllocation)
		{
			this->manageData = !skipAllocation;
			this->levelId = levelId;
			this->iterationType = iterationType;

			if (!skipAllocation) {
				this->rgb = new ORUtils::Image<Vector4u>(imgSize, memoryType);
				this->depth = new ORUtils::Image<float>(imgSize, memoryType);
				this->gradientX_rgb = new ORUtils::Image<Vector4s>(imgSize, memoryType);
				this->gradientY_rgb = new ORUtils::Image<Vector4s>(imgSize, memoryType);
			}
		}

		void UpdateHostFromDevice()
		{ 
			this->rgb->UpdateHostFromDevice();
			this->depth->UpdateHostFromDevice();
			this->gradientX_rgb->UpdateHostFromDevice();
			this->gradientY_rgb->UpdateHostFromDevice();
		}

		void UpdateDeviceFromHost()
		{ 
			this->rgb->UpdateDeviceFromHost();
			this->depth->UpdateDeviceFromHost();
			this->gradientX_rgb->UpdateDeviceFromHost();
			this->gradientY_rgb->UpdateDeviceFromHost();
		}

		~ViewHierarchyLevel()
		{
			if (manageData) {
				delete rgb;
				delete depth;
				delete gradientX_rgb; delete gradientY_rgb;
			}
		}

		// Suppress the default copy constructor and assignment operator
		ViewHierarchyLevel(const ViewHierarchyLevel&);
		ViewHierarchyLevel& operator=(const ViewHierarchyLevel&);
	};
}
