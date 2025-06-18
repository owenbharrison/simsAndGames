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

	Spring(Particle* _a, Particle* _b, float dt) {
		a=_a, b=_b;
		rest_len=(a->pos-b->pos).mag();

		//constants proportional to avg mass
		float m=(a->mass+b->mass)/2;
		stiffness=1130.75f*m;
		//"*dt" cancels out later
		damping=375.9f*dt*m;
	}

	vf3d getForce(float dt) const {
		//estimate particle velocities
		vf3d vel_a=(a->pos-a->old_pos)/dt;
		vf3d vel_b=(b->pos-b->old_pos)/dt;
		
		//get separation
		vf3d sub=b->pos-a->pos;
		float mag=sub.mag();
		vf3d norm(1, 0, 0);
		if(mag>1e-6f) norm=sub/mag;

		//hookes law
		float x=mag-rest_len;
		float spring=stiffness*x;

		//damping w/ relative velocity
		float v_rel=norm.dot(vel_b-vel_a);
		float damp=damping*v_rel;

		return (spring+damp)*norm;
	}

	void update(float dt) {
		vf3d force=getForce(dt);
		if(!a->locked) a->applyForce(force);
		if(!b->locked) b->applyForce(-force);
	}
};
#endif