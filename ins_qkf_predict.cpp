/*
 * ins_qkf_predict.cpp
 *
 *  Created on: Sep 2, 2009
 *      Author: Jonathan Brandmeyer

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

#include "ins_qkf.hpp"
#include "assertions.hpp"
#include "timer.hpp"

#ifdef TIME_OPS
# include <iostream>
#endif

using namespace Eigen;

namespace {

void
linear_predict(basic_ins_qkf& _this,
		const Vector3d& gyro_meas,
		const Vector3d& accel_meas,
		double dt)
{
	// The two components of rotation that do not spin about the gravity vector
	// have an influence on the position and velocity of the vehicle.
	// Let r be an error axis of rotation, and z be the gravity vector.
	// Increasing r creates increasing error in the direction _|_ to r and z.
	// By the small angle theorem, the amount of error is ~ abs(r)*abs(z).
	// Increasing r also creates increasing error in the direction || to -z.
	// By the small angle theorem, the amount of error is ~ zero.
	// Therefore, rotate the error block about the z axis by -90 degrees, and
	// zero out the error vector in the z direction.
	// accel_cov is the relationship between error vectors in the tangent space
	// of the vehicle orientation and the translational reference frame.
	Vector3d accel_body = _this.avg_state.orientation.conjugate()*accel_meas;
	// The acceleration due to gravity as observed by the sensor. (a force
	// away from the earth).
	Vector3d accel_gravity = _this.avg_state.position.normalized()*9.81;
	// The acceleration acting on the body of the vehicle in the ECEF frame.
	Vector3d accel = accel_body - accel_gravity;

	Matrix<double, 3, 3> accel_cov = cross<double>(-accel_body);

	// The linearized Kalman state projection matrix.
#if 0
	Matrix<double, 12, 12> A;
	     // gyro bias row
	A << Matrix<double, 3, 3>::Identity(), Matrix<double, 3, 9>::Zero(),
		 // Orientation row
		 _this.avg_state.orientation.conjugate().toRotationMatrix()*-dt,
			 Matrix<double, 3, 3>::Identity(), Matrix<double, 3, 6>::Zero(),
		 // Position row
		 Matrix<double, 3, 3>::Zero(), -accel_cov*0.5*dt*dt,
			 Matrix<double, 3, 3>::Identity(), Matrix<double, 3, 3>::Identity()*dt,
		 // Velocity row
		 Matrix<double, 3, 3>::Zero(), -accel_cov * dt,
			 Matrix<double, 3, 3>::Zero(), Matrix<double, 3, 3>::Identity();

	// 800x realtime, with vectorization
	_this.cov.part<Eigen::SelfAdjoint>() = A * _this.cov * A.transpose();
#else
	// 1500x realtime, without vectorization, on 2.2 GHz Athlon X2
	const Matrix<double, 12, 12> cov = _this.cov;
	const Matrix3d dtR = dt * _this.avg_state.orientation.conjugate().toRotationMatrix();
	const Matrix3d dtQ = accel_cov * dt;

	_this.cov.block<3, 3>(0, 3) -= cov.block<3,3>(0, 0)*dtR.transpose();
	_this.cov.block<3, 3>(0, 6) += dt * cov.block<3, 3>(0, 9);
	_this.cov.block<3, 3>(0, 9) -= cov.block<3, 3>(0, 3) * dtQ.transpose();
	_this.cov.block<3, 3>(3, 3) += dtR*cov.block<3, 3>(0, 0)*dtR.transpose()
			- dtR*cov.block<3, 3>(0, 3) - cov.block<3, 3>(3, 0)*dtR.transpose();
	_this.cov.block<3, 3>(3, 6) += -dtR * (cov.block<3, 3>(0, 6) + dt*cov.block<3, 3>(0, 9))
			+ dt*cov.block<3, 3>(3, 9);
	_this.cov.block<3, 3>(3, 9) += -dtR*( -cov.block<3, 3>(0, 3)*dtQ.transpose() + cov.block<3, 3>(0, 9))
			- cov.block<3, 3>(3, 3)*dtQ.transpose();
	_this.cov.block<3, 3>(6, 6) += dt*cov.block<3, 3>(6, 9) + dt*dt*cov.block<3, 3>(9, 9)
			+ dt*cov.block<3, 3>(9, 6);
	_this.cov.block<3, 3>(6, 9) += -cov.block<3, 3>(6, 3)*dtQ.transpose() + dt*cov.block<3, 3>(9, 9)
			- dt*cov.block<3, 3>(9, 3)*dtQ.transpose();
	_this.cov.block<3, 3>(9, 9) += dtQ*cov.block<3, 3>(3, 3)*dtQ.transpose()
			- dtQ*cov.block<3, 3>(3, 9) - cov.block<3, 3>(9, 3)*dtQ.transpose();

	_this.cov.block<3, 3>(3, 0) = _this.cov.block<3, 3>(0, 3).transpose();
	_this.cov.block<3, 3>(6, 0) = _this.cov.block<3, 3>(0, 6).transpose();
	_this.cov.block<3, 3>(6, 3) = _this.cov.block<3, 3>(3, 6).transpose();
	_this.cov.block<3, 3>(9, 0) = _this.cov.block<3, 3>(0, 9).transpose();
	_this.cov.block<3, 3>(9, 3) = _this.cov.block<3, 3>(3, 9).transpose();
	_this.cov.block<3, 3>(9, 6) = _this.cov.block<3, 3>(6, 9).transpose();
#endif

	_this.cov.block<3, 3>(0, 0) += _this.gyro_stability_noise.asDiagonal() * dt;
	_this.cov.block<3, 3>(3, 3) += _this.gyro_white_noise.asDiagonal() * dt;
	_this.cov.block<3, 3>(6, 6) += _this.accel_white_noise.asDiagonal() * 0.5*dt*dt;
	_this.cov.block<3, 3>(9, 9) += _this.accel_white_noise.asDiagonal() * dt;

	Quaterniond orientation = exp<double>((gyro_meas - _this.avg_state.gyro_bias) * dt)
			* _this.avg_state.orientation;
	Vector3d position = _this.avg_state.position + _this.avg_state.velocity * dt + 0.5*accel*dt*dt;
	Vector3d velocity = _this.avg_state.velocity + accel*dt;

	_this.avg_state.position = position;
	_this.avg_state.velocity = velocity;
	// Note: Renormalization occurs during all measurement updates.
	_this.avg_state.orientation = orientation;
}

} // !namespace (anon)

void
basic_ins_qkf::predict(const Vector3d& gyro_meas,
		const Vector3d& accel_meas,
		double dt)
{
#ifdef TIME_OPS
	timer clock;
	clock.start();
#endif

	// Always use linearized prediction
	linear_predict(*this, gyro_meas, accel_meas, dt);

	assert(invariants_met());

#ifdef TIME_OPS
	double time = clock.stop()*1e6;
	std::cout << "unscented predict time: " << time << "\n";
#endif
}

