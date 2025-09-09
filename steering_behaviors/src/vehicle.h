#pragma once
#ifndef VEHICLE_STRUCT_H
#define VEHICLE_STRUCT_H

#include <cmath>

struct Vehicle {
	vf2d pos, vel, acc;
	vf2d target;
	float max_speed=1061.7f;
	float max_force=1989.67f;

	Vehicle() {}

	Vehicle(const vf2d& p, const vf2d& t) {
		pos=p;
		target=t;
	}

	void accelerate(const vf2d& a) {
		acc+=a;
	}

	void update(float dt) {
		vel+=acc*dt;
		pos+=vel*dt;

		//clear "forces"
		acc*=0;
	}

	//behavior forces
	vf2d getSeek(const vf2d& t) const {
		vf2d desired=t-pos;

		//dont divide by zero
		vf2d desired_norm(1, 0);
		float desired_mag_sq=desired.mag2();
		if(desired_mag_sq>1e-6f) {
			desired_norm=desired/std::sqrt(desired_mag_sq);
		}

		//want to move as fast as possible
		desired=max_speed*desired_norm;

		vf2d steer=desired-vel;

		//dont divide by zero
		vf2d steer_norm(1, 0);
		float steer_mag_sq=steer.mag2();
		if(steer_mag_sq>1e-6f) {
			steer_norm=desired/std::sqrt(steer_mag_sq);
		}

		//limit steer force
		if(steer_mag_sq>max_force*max_force) {
			steer=max_force*steer_norm;
		}
		return steer;
	}

	vf2d getArrive(const vf2d& t) const {
		vf2d desired=t-pos;

		//dont divide by zero
		vf2d desired_norm(1, 0);
		float desired_mag_sq=desired.mag2();
		if(desired_mag_sq>1e-6f) {
			desired_norm=desired/std::sqrt(desired_mag_sq);
		}

		//move as fast as possible
		float speed=max_speed;

		//except when close enough
		const float slow_rad=100;
		if(desired_mag_sq<slow_rad*slow_rad) {
			speed=max_speed*std::sqrt(desired_mag_sq)/slow_rad;
		}

		desired=speed*desired_norm;

		vf2d steer=desired-vel;

		//dont divide by zero
		vf2d steer_norm(1, 0);
		float steer_mag_sq=steer.mag2();
		if(steer_mag_sq>1e-6f) {
			steer_norm=desired/std::sqrt(steer_mag_sq);
		}

		//limit steer force
		if(steer_mag_sq>max_force*max_force) {
			steer=max_force*steer_norm;
		}
		return steer;
	}

	vf2d getFlee(const vf2d& t) const {
		//move in opposite direction
		vf2d desired=pos-t;

		//dont divide by zero
		vf2d desired_norm(1, 0);
		float desired_mag_sq=desired.mag2();
		if(desired_mag_sq>1e-6f) {
			desired_norm=desired/std::sqrt(desired_mag_sq);
		}

		//want to move as fast as possible
		desired=max_speed*desired_norm;

		vf2d steer=desired-vel;

		//dont divide by zero
		vf2d steer_norm(1, 0);
		float steer_mag_sq=steer.mag2();
		if(steer_mag_sq>1e-6f) {
			steer_norm=desired/std::sqrt(steer_mag_sq);
		}

		//limit steer force
		if(steer_mag_sq>max_force*max_force) {
			steer=max_force*steer_norm;
		}
		return steer;
	}
};
#endif