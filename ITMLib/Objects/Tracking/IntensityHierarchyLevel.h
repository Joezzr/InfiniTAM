// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

#include "TrackerIterationType.h"
#include "../../Utils/Math.h"
#include "../../../ORUtils/Image.h"

namespace ITMLib
{
	class IntensityHierarchyLevel
	{
	public:
		int levelId;

		TrackerIterationType iterationType;

		ORUtils::Image<float> *intensity_current;
		ORUtils::Image<float> *intensity_prev;
		ORUtils::Image<Vector2f> *gradients;
		Vector4f intrinsics;

		bool manageData;

		IntensityHierarchyLevel(Vector2i imgSize, int levelId, TrackerIterationType iterationType,
		                        MemoryDeviceType memoryType, bool skipAllocation = false)
		{
			this->manageData = !skipAllocation;
			this->levelId = levelId;
			this->iterationType = iterationType;

			if (!skipAllocation)
			{
				this->intensity_current = new ORUtils::Image<float>(imgSize, memoryType);
				this->intensity_prev = new ORUtils::Image<float>(imgSize, memoryType);
			}
			else
			{
				this->intensity_current = nullptr;
				this->intensity_prev = nullptr;
			}

			this->gradients = new ORUtils::Image<Vector2f>(imgSize, memoryType);
		}

		void UpdateHostFromDevice()
		{ 
			if (!this->intensity_current || !this->intensity_prev)
				throw std::runtime_error("IntensityHierarchyLevel: did not set intensity images.");

			this->intensity_current->UpdateHostFromDevice();
			this->intensity_prev->UpdateHostFromDevice();
			this->gradients->UpdateHostFromDevice();
		}

		void UpdateDeviceFromHost()
		{ 
			if (!this->intensity_current || !this->intensity_prev)
				throw std::runtime_error("IntensityHierarchyLevel: did not set intensity images.");

			this->intensity_current->UpdateDeviceFromHost();
			this->intensity_prev->UpdateDeviceFromHost();
			this->gradients->UpdateDeviceFromHost();
		}

		~IntensityHierarchyLevel()
		{
			if (manageData)
			{
				delete intensity_current;
				delete intensity_prev;
			}

			delete gradients;
		}

		// Suppress the default copy constructor and assignment operator
		IntensityHierarchyLevel(const IntensityHierarchyLevel&);
		IntensityHierarchyLevel& operator=(const IntensityHierarchyLevel&);
	};
}
