#pragma once
#ifndef SPRING_STRUCT_H
#define SPRING_STRUCT_H

#include "point_mass.h"

struct Spring {
	PointMass* a=nullptr, * b=nullptr;
	float rest_len=0;
	float stiffness=0;
	float damping=0;
	
	Spring() {}

	Spring(PointMass* _a, PointMass* _b, float k, float d) {
		a=_a, b=_b;
		rest_len=(a->pos-b->pos).mag();
		stiffness=k;
		damping=d;
	}

	Spring(PointMass* _a, PointMass* _b) {
		a=_a, b=_b;
		rest_len=(a->pos-b->pos).mag();
		
		//coefficients proportional to avg mass
		float m=(a->mass+b->mass)/2;
		stiffness=95.67f*m;
		damping=493.3f*m;
	}

	vf2d getForce() const {
		vf2d sub=b->pos-a->pos;
		float curr_len=sub.mag();
		float spring_force=stiffness*(curr_len-rest_len);

		//thanks gonkee
		vf2d a_vel=a->pos-a->oldpos;
		vf2d b_vel=b->pos-b->oldpos;
		vf2d norm=sub/curr_len;
		float damp_force=damping*norm.dot(b_vel-a_vel);

		return (spring_force+damp_force)*norm;
	}

	void update() {
		vf2d force=getForce();
		a->applyForce(force);
		b->applyForce(-force);
	}
};
#endif//SPRING_STRUCT_H