// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

#include "../../Utils/Math.h"

namespace ITMLib
{
	/** \brief
	    Represents the calibration information to compute_allocated a depth
	    image from the disparity image typically received from a
	    Kinect.
	*/
	class DisparityCalib
	{
		//#################### ENUMERATIONS ####################
	public:
		/** Type of transformation required to get from raw values to depths. */
		enum TrafoType
		{
			/// Raw values are transformed according to \f$\frac{8c_2f_x}{c_1 - d}\f$
			TRAFO_KINECT,
			/// Raw values are transformed according to \f$c_1 d + c_2\f$
			TRAFO_AFFINE
		};

		//#################### PRIVATE VARIABLES ####################
	private:
		TrafoType type;

		/** These are the actual parameters. */
		Vector2f params;

		//#################### CONSTRUCTORS ####################
	public:
		DisparityCalib()
		{
			SetStandard();
		}

		//#################### PUBLIC MEMBER FUNCTIONS ####################
	public:
		const Vector2f& GetParams() const
		{
			return params;
		}

		TrafoType GetType() const
		{
			return type;
		}

		/** Setup from given arguments. */
		void SetFrom(float a, float b, TrafoType _type)
		{
			if(a != 0.0f || b != 0.0f)
			{
				params.x = a;
				params.y = b;
				type = _type;
			}
			else SetStandard();
		}

		/** Setup from standard arguments. */
		void SetStandard()
		{
			// standard calibration parameters - converts mm to metres by dividing by 1000
			SetFrom(1.0f / 1000.0f, 0.0f, TRAFO_AFFINE);
		}
	};
}
