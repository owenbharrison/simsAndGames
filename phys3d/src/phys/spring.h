#pragma once
#ifndef SPRING_STRUCT_H
#define SPRING_STRUCT_H

#include "particle.h"

struct Spring {
	Particle* a=nullptr, * b=nullptr;
	float rest_len=0;
	float stiffness=0;
	float damping=0;

	Spring() {}

	Spring(Particle* _a, Particle* _b, float k, float d) {
		a=_a, b=_b;
		rest_len=length(a->pos-b->pos);
		stiffness=k;
		damping=d;
	}

	cmn::vf3d getForce() const {
		cmn::vf3d sub=b->pos-a->pos;
		float curr_len=length(sub);
		float spring=stiffness*(curr_len-rest_len);

		cmn::vf3d a_vel=a->pos-a->pos_old;
		cmn::vf3d b_vel=b->pos-b->pos_old;
		cmn::vf3d norm=sub/curr_len;
		float damp=damping*dot(norm, b_vel-a_vel);

		return (spring+damp)*norm;
	}

	void update() {
		cmn::vf3d force=getForce();
		a->applyForce(force);
		b->applyForce(-force);
	}
};
#endif