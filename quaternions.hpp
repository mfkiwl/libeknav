#ifndef AHRS_QUATERNIONS_HPP
#define AHRS_QUATERNIONS_HPP

/*
 * quaternions.cpp
 *
 *      Author: Jonathan Brandmeyer
 *
 *          This file is part of libeknav.
 *
 *  Libeknav is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  Libeknav is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with libeknav.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Eigen/Core>
#include <Eigen/Geometry>

using Eigen::Quaternion;

template<typename FloatT>
Quaternion<FloatT> operator-(const Quaternion<FloatT>& q)
{
	return Quaternion<FloatT>(-q.w(), -q.x(), -q.y(), -q.z());
}

/**
 * Convert a rotation from modified Rodrigues parameters to a quaternion.
 * @param v The multiplication of the rotation angle by the tangent of the angle/4
 * @return The quaternion corresponding to that rotation.
 */
template<typename FloatT>
Quaternion<FloatT> exp_r(const Eigen::Matrix<FloatT, 3, 1>& v)
{
	// a2 = tan^2(theta/4)
	FloatT a2 = v.squaredNorm();
	Quaternion<FloatT> ret;
	// sin(theta/2) = 2*tan(theta/4) / (1 + tan^2(theta/4))
	// v == v.normalized() * tan^2(theta/4)
	ret.vec() = v*(2/(1+a2));
	// cos(theta/2) = (1 - tan^2(theta/4)) / (1 + tan^2(theta/4))
	ret.w() = (1-a2)/(1+a2);
	return ret;
}

template<typename FloatT>
Eigen::Matrix<FloatT, 3, 1> log_r(const Quaternion<FloatT>& q)
{
	// Note: This algorithm is reasonably safe when using double
	// precision (to within 1e-10 of precision), but not float.
	/*
	 * q.w() == cos(theta/2)
	 * q.vec() == sin(theta/2)*v_hat
	 *
	 *
	 * Normal rodrigues params:
	 * v*tan(theta/2) = v*sin(theta/2) / cos(theta/2)
	 * v*tan(theta/2) = q.vec() / q.w()
	 *
	 * Modified rodrigues params (pulls away from infinity at theta=pi)
	 * tan(theta/2) == sin(theta) / (1 + cos(theta))
	 * therefore, tan(theta/4)*v_hat = sin(theta/2)*v_hat / (1 + cos(theta/2))
	 */
	return q.vec() / (1.0+q.w());
}

/**
 * Convert an angle/axis 3-vector to a unit quaternion
 * @param v A 3-vector whose length is between 0 and 2*pi
 * @return The quaternion that represents the same rotation.
 */
template<typename FloatT>
Quaternion<FloatT> exp(Eigen::Matrix<FloatT, 3, 1> v);

template<typename FloatT>
Quaternion<FloatT> exp(Eigen::Matrix<FloatT, 3, 1> v)
{
	FloatT angle = v.norm();
	if (angle <= Eigen::machine_epsilon<FloatT>()) {
		// std::cerr << "Warning: tiny quaternion flushed to zero\n";
		return Quaternion<FloatT>::Identity();
	}
	else {
		Quaternion<FloatT> ret;
#if 0
		if (angle > 1.999*M_PI) {
			// TODO: I really, really don't like this hack. It should
			// be impossible to compute an angular measurement update
			// with a rotation angle greater than this number...
			v *= 1.999*M_PI / angle;
			angle = 1.999*M_PI;
		}
#endif
		assert(angle <= FloatT(2.0*M_PI));
#if 0
		// Oddly enough, this attempt to make the formula faster by reducing
		// the number of trig calls actually runs slower.
		FloatT tan_x = std::tan(angle * 0.25);
		FloatT cos_angle = (1 - tan_x*tan_x)/(1+tan_x*tan_x);
		FloatT sin_angle = 2*tan_x/(1+tan_x*tan_x);
		ret.w() = cos_angle;
		ret.vec() = (sin_angle/angle)*v;
#else
		ret.w() = std::cos(angle*0.5);
		ret.vec() = (std::sin(angle*0.5)/angle)*v;
#endif
		return ret;
		// return Quaternion<FloatT>(Eigen::AngleAxis<FloatT>(angle, v / angle));
	}
}

/**
 * Convert a unit quaternion to multiplied angle/axis form.
 * @param q A unit quaternion.  The quaternion's norm should be close to unity,
 * but may be slightly too large or small.
 * @return The 3-vector in the tangent space of the quaternion q.
 */
template<typename FloatT>
Eigen::Matrix<FloatT, 3, 1> log(const Quaternion<FloatT>& q);

template<typename FloatT>
Eigen::Matrix<FloatT, 3, 1> log(const Quaternion<FloatT>& q)
{
	FloatT mag = q.vec().norm();
	if (mag <= Eigen::machine_epsilon<FloatT>()) {
		// Flush to zero for very small angles.  This avoids division by zero.
		return Eigen::Matrix<FloatT, 3, 1>::Zero();
	}
	FloatT angle = 2.0*std::atan2(mag, q.w());
	return q.vec() * (angle/mag);
	// Eigen::AngleAxis<FloatT> res(q /*mag <= 1.0) ? q : q.normalized() */);
	// return res.axis() * res.angle();
}

/**
 * Compute the cross-product matrix of a 3-vector.
 * @param v A vector in R3
 * @return a cross product matrix following the identity
 * 	cross(v)*x == v.cross(x)
 */
inline Matrix<double, 3, 3> cross(const Matrix<double, 3, 1>& v)
{
	return (Matrix<double, 3, 3>() <<
		0, -v[2], v[1],
		v[2], 0, -v[0],
		-v[1], v[0], 0).finished();
}

template<typename FloatT>
Quaternion<FloatT>
incremental_normalized(const Quaternion<FloatT>& q);

/**
 * Incrementally normalize a quaternion q.
 * @precondition 1 - q.norm() < sqrt(machine_epsilon)
 * @postcondition 1 - q.norm() <= machine_epsilon
 * @param q A nearly normalized quaternion to be completely normalized
 * @return The completely normalized quaternion
 */
template<typename FloatT>
Quaternion<FloatT>
incremental_normalized(const Quaternion<FloatT>& q)
{
    FloatT norm2 = q.coeffs().squaredNorm();
    // 1 Newton iteration of 1/sqrt(x), given a definition of refine like
    // est*0.5*(3 - x*est*est)
    // with an initial estimate of 1.0.  If the true norm is less than
    // sqrt(eps) away from 1.0, then this completely normalizes the quaternion.
    FloatT invSqrtMag = 0.5*(3 - norm2);
    // Grumble, grumble, cannot construct from array of coefficients, grumble.
    return Quaternion<FloatT>(
            q.w()*invSqrtMag,
            q.x()*invSqrtMag,
            q.y()*invSqrtMag,
            q.z()*invSqrtMag
    );
}

#endif