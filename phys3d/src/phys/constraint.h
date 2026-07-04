#pragma once
#ifndef CONSTRAINT_STRUCT_H
#define CONSTRAINT_STRUCT_H

#include "particle.h"

struct Constraint {
	Particle* a=nullptr, * b=nullptr;
	float rest_len=0;

	Constraint() {}

	Constraint(Particle& a_, Particle& b_) {
		a=&a_;
		b=&b_;
		rest_len=(a->pos-b->pos).mag();
	}

	cmn::vf3d getCorrection() const {
		cmn::vf3d ab=b->pos-a->pos;
		float len=ab.mag();
		cmn::vf3d norm=ab/len;
		float delta=len-rest_len;
		return .5f*delta*norm;
	}

	void update() {
		cmn::vf3d corr=getCorrection();
		a->pos+=corr;
		b->pos-=corr;
	}
};
#endif