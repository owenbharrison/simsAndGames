#pragma once
#ifndef CONSTRAINT_STRUCT_H
#define CONSTRAINT_STRUCT_H

#include "point_mass.h"

struct Constraint {
	PointMass* a=nullptr, * b=nullptr;
	float len_rest=0;

	Constraint() {}

	Constraint(PointMass* _a, PointMass* _b) :
		a(_a), b(_b) {
		len_rest=(a->pos-b->pos).mag();
	}

	vf2d getCorrection() const {
		vf2d axis=a->pos-b->pos;
		float dist=axis.mag();
		vf2d norm=axis/dist;
		float delta=len_rest-dist;
		return .5f*delta*norm;
	}

	void update() {
		vf2d correction=getCorrection();
		if(!a->locked) a->pos+=correction;
		if(!b->locked) b->pos-=correction;
	}
};
#endif//CONSTRAINT_STRUCT_H