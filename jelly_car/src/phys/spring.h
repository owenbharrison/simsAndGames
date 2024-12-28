#pragma once
#ifndef SPRING_STRUCT_H
#define SPRING_STRUCT_H

#include "point_mass.h"

struct Spring {
	PointMass* a=nullptr, * b=nullptr;
	float rest_len=0;
	float stiffness=0;
	float damping=0;

	Spring(PointMass* _a, PointMass* _b, float k=215.3f, float d=6.7f) :
		a(_a), b(_b),
		stiffness(k),
		damping(d) {
		rest_len=(a->pos-b->pos).mag();
	}

	vf2d getForce(float dt) const {
		vf2d sub=b->pos-a->pos;
		float curr_len=sub.mag();
		float spring_force=stiffness*(curr_len-rest_len);

		//thanks gonkee
		vf2d a_vel=(a->pos-a->oldpos)/dt;
		vf2d b_vel=(b->pos-b->oldpos)/dt;
		vf2d norm=sub/curr_len;
		float damp_force=damping*norm.dot(b_vel-a_vel);

		return (spring_force+damp_force)*norm;
	}

	void update(float dt) {
		vf2d force=getForce(dt);
		a->applyForce(force);
		b->applyForce(-force);
	}
};
#endif//SPRING_STRUCT_H