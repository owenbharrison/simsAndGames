#pragma once
#ifndef SMOKE_STRUCT_H
#define SMOKE_STRUCT_H

#include "cmn/math/v2d.h"

struct Smoke {
	cmn::vf2d pos, vel, acc;
	float rot=0, rot_vel=0;
	float size=0, size_vel=0;
	
	float life=1;

	float r=1, g=1, b=1;

	void accelerate(const cmn::vf2d& f) {
		acc+=f;
	}

	void update(float dt) {
		//euler integration
		vel+=acc*dt;
		pos+=vel*dt;

		//reset forces
		acc*=0;

		//update other things
		rot+=rot_vel*dt;
		size+=size_vel*dt;
	}

	bool isDead() const {
		return life<0;
	}
};
#endif