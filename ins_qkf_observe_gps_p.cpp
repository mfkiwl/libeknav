/*
 * ins_qkf_observe_gps_p.cpp
 *
 *  Created on: Jun 10, 2010
 *      Author: jonathan
 */

#include "ins_qkf.hpp"
#include "assertions.hpp"
#include <Eigen/LU>
#include "timer.hpp"

#define RANK_ONE_UPDATES
using namespace Eigen;

void
basic_ins_qkf::obs_gps_p_report(const Vector3d& pos, const Vector3d& p_error)
{
	Matrix<double, 3, 1> residual = pos - avg_state.position;
	Matrix<double, 3, 3> innovation_cov = cov.block<3, 3>(6, 6);
	innovation_cov += p_error.asDiagonal();
#ifdef RANK_ONE_UPDATES
	Matrix<double, 12, 1> update = Matrix<double, 12, 1>::Zero();
	for (int i = 0; i < 3; ++i) {
		Matrix<double, 12, 1> gain = cov.block<12, 1>(0, 6+i) / innovation_cov(i, i);
		update += gain * (residual[i] - update[6+i]);
		cov -= gain * cov.block<1, 12>(6+i, 0);
	}

#else
	Matrix<double, 12, 3> kalman_gain = cov.block<12, 3>(0, 6)
		* innovation_cov.part<Eigen::SelfAdjoint>().inverse();
	Matrix<double, 12, 1> update = kalman_gain * residual;
	cov.part<Eigen::SelfAdjoint>() -= kalman_gain * cov.block<3, 12>(6, 0);
#endif
	Quaterniond rotor = avg_state.apply_kalman_vec_update(update);
	counter_rotate_cov(rotor);
	assert(is_real());
}
