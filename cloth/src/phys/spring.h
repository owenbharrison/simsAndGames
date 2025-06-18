#pragma once
#ifndef SPRING_STRUCT_H
#define SPRING STRUCT_H

#include "particle.h"

struct Spring {
	Particle* a=nullptr, * b=nullptr;
	float rest_len=0;
	float stiffness=0;
	float damping=0;
	float strain=0;

	Spring() {}

	Spring(Particle* _a, Particle* _b, float k, float d) {
		a=_a, b=_b;
		rest_len=(a->pos-b->pos).mag();
		stiffness=k;
		damping=d;
	}

	vf3d getAccel() const {
		vf3d sub=b->pos-a->pos;
		float curr_len=sub.mag();
		float spring=stiffness*(curr_len-rest_len);

		//thanks gonkee
		vf3d a_vel=a->pos-a->old_pos;
		vf3d b_vel=b->pos-b->old_pos;
		vf3d norm=sub/curr_len;
		float damp=damping*norm.dot(b_vel-a_vel);

		return (spring+damp)*norm;
	}

	void update() {
		vf3d force=getAccel();
		if(!a->locked) a->accelerate(force);
		if(!b->locked) b->accelerate(-force);

		//update strain
		float curr=(a->pos-b->pos).mag();
		strain=rest_len-curr/rest_len;
	}
};
#endif