#pragma once
#ifndef CONSTRAINT_STRUCT_H
#define CONSTRAINT_STRUCT_H

#include "particle.h"

struct Constraint {
	Particle* a=nullptr, * b=nullptr;
	float rest_len=0;

	Constraint() {}

	Constraint(Particle* _a, Particle* _b) :
		a(_a), b(_b) {
		rest_len=(a->pos-b->pos).mag();
	}

	vf3d getCorrection() const {
		vf3d axis=a->pos-b->pos;
		float dist=axis.mag();
		vf3d norm=axis/dist;
		float delta=rest_len-dist;
		return .5f*delta*norm;
	}

	void update() {
		vf3d correction=getCorrection();
		if(!a->locked) a->pos+=correction;
		if(!b->locked) b->pos-=correction;
	}
};
#endif