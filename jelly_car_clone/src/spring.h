#pragma once
#ifndef SPRING_CLASS_H
#define SPRING_CLASS_H

//didnt think i'd have to include this...
#include "common/olcPixelGameEngine.h"

#include "constraint.h"

struct Spring : Constraint {
	float stiffness=0, damping=0;

	Spring() {}

	Spring(PointMass* a, PointMass* b, float k, float d) : Constraint(a, b), stiffness(k), damping(d) {}

	olc::vf2d calcForce() const {
		//seperation axis
		olc::vf2d sub=b->pos-a->pos;
		float curr_len=sub.mag();

		//hookes law: F=kx
		float spring_force=stiffness*(curr_len-rest_len);
		olc::vf2d norm=sub/curr_len;
		olc::vf2d a_vel=a->pos-a->old_pos;
		olc::vf2d b_vel=b->pos-b->old_pos;

		//drag
		float damp_force=damping*norm.dot(b_vel-a_vel);
		return norm*(spring_force+damp_force);
	}

	void update() {
		olc::vf2d force=calcForce();
		if(!a->locked) a->applyForce(force);
		if(!b->locked) b->applyForce(-force);
	}
};
#endif//SPRING_CLASS_H