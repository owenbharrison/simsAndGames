#pragma once
#ifndef CONSTRAINT_STRUCT_H
#define CONSTRAINT_STRUCT_H

//didnt think i'd have to include this...
#include "common/olcPixelGameEngine.h"

#include "point_mass.h"

struct Constraint {
	PointMass* a=nullptr, * b=nullptr;
	float rest_len=0;

	Constraint() {}

	Constraint(PointMass* _a, PointMass* _b) : a(_a), b(_b) {
		rest_len=(a->pos-b->pos).mag();
	}

	olc::vf2d getCorrection() const {
		olc::vf2d axis=a->pos-b->pos;
		float dist=axis.mag();
		olc::vf2d norm=axis/dist;
		float delta=rest_len-dist;
		return .5f*delta*norm;
	}

	void update() {
		olc::vf2d correction=getCorrection();
		if(!a->locked) a->pos+=correction;
		if(!b->locked) b->pos-=correction;
	}
};
#endif//CONSTRAINT_STRUCT_H