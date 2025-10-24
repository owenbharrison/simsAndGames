#pragma once
#ifndef SPRING_STRUCT_H
#define SPRING_STRUCT_H

#include "point_mass.h"

struct Spring {
	PointMass* a=nullptr, * b=nullptr;
	float len_rest=0;
	float stiffness=0;
	float damping=0;
	
	Spring() {}

	Spring(PointMass* _a, PointMass* _b, float k, float d) {
		a=_a, b=_b;
		len_rest=(a->pos-b->pos).mag();
		stiffness=k;
		damping=d;
	}

	Spring(PointMass* _a, PointMass* _b) {
		a=_a, b=_b;
		len_rest=(a->pos-b->pos).mag();
		
		//coefficients proportional to avg mass
		float m=(a->mass+b->mass)/2;
		stiffness=95.67f*m;
		damping=493.3f*m;
	}

	vf2d getForce() const {
		vf2d sub=b->pos-a->pos;
		float len_curr=sub.mag();
		float force_spring=stiffness*(len_curr-len_rest);

		//thanks gonkee
		vf2d vel_a=a->pos-a->oldpos;
		vf2d vel_b=b->pos-b->oldpos;
		vf2d norm=sub/len_curr;
		float force_damp=damping*norm.dot(vel_b-vel_a);

		return (force_spring+force_damp)*norm;
	}

	void update() {
		vf2d force=getForce();
		if(!a->locked) a->applyForce(force);
		if(!b->locked) b->applyForce(-force);
	}
};
#endif//SPRING_STRUCT_H