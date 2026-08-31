//thanks sebastian lague for inspo!
//https://www.youtube.com/watch?v=bqtqltqcQhw
#pragma once
#ifndef BOID_STRUCT_H
#define BOID_STRUCT_H

#include "common/math/v3d.h"

struct Boid {
	static float min_speed;
	static float max_speed;
	static float max_force;

	static float flock_rad;
	float avoid_rad=.13f;

	static float alignment_wgt;
	static float cohesion_wgt;
	static float separation_wgt;

	cmn::vf3d pos, vel;
	cmn::vf3d dir;

	//updated externally...
	bool flock_valid=false;
	cmn::vf3d flock_pos;
	cmn::vf3d flock_dir;
	cmn::vf3d flock_sep;

	void update(float dt) {
		cmn::vf3d acc;

		if(flock_valid) {
			acc+=steerToward(flock_dir)*alignment_wgt;
			cmn::vf3d cohesion_vec=flock_pos-pos;
			acc+=steerToward(cohesion_vec)*cohesion_wgt;
			acc+=steerToward(flock_sep)*separation_wgt;
		}

		//update vel
		vel+=dt*acc;

		//get dir & clamp speed
		float speed=length(vel);
		dir=speed==0?cmn::vf3d(1, 0, 0):vel/speed;
		if(speed<min_speed) speed=min_speed;
		if(speed>max_speed) speed=max_speed;
		vel=speed*dir;

		//update pos
		pos+=dt*vel;
	}

	//force to steer curr vel towards given v
	cmn::vf3d steerToward(cmn::vf3d v) const {
		v=max_speed*normalize(v)-vel;
		float curr=length(v);
		if(curr>max_force) v*=max_force/curr;
		return v;
	}
};

float Boid::min_speed=0;
float Boid::max_speed=1.7f;
float Boid::max_force=100;

float Boid::flock_rad=.5f;

float Boid::alignment_wgt=.3f;
float Boid::cohesion_wgt=.5f;
float Boid::separation_wgt=1;
#endif