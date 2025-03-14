#pragma once
#ifndef POINTMASS_STRUCT_H
#define POINTMASS_STRUCT_H

#include "common/olcPixelGameEngine.h"

#include "aabb.h"

struct PointMass {
	olc::vf2d pos, old_pos, force;
	float mass=1;
	bool locked=false;

	PointMass() {}

	PointMass(olc::vf2d p, float m) : pos(p), old_pos(p), mass(m) {}

	void applyForce(const olc::vf2d& f) {
		force+=f;
	}

	void update(float dt) {
		//update pos store
		olc::vf2d vel=pos-old_pos;
		old_pos=pos;

		//verlet integration
		olc::vf2d acc=force/mass;
		if(!locked) pos+=vel+acc*dt*dt;

		//reset forces
		force={0, 0};
	}

	void keepIn(const AABB& a) {
		olc::vf2d vel=pos-old_pos;
		if(pos.x<a.min.x) {
			pos.x=a.min.x;
			old_pos.x=a.min.x+vel.x;
		}
		if(pos.y<a.min.y) {
			pos.y=a.min.y;
			old_pos.y=a.min.y+vel.y;
		}
		if(pos.x>a.max.x) {
			pos.x=a.max.x;
			old_pos.x=a.max.x+vel.x;
		}
		if(pos.y>a.max.y) {
			pos.y=a.max.y;
			old_pos.y=a.max.y+vel.y;
		}
	}
};
#endif//POINTMASS_STRUCT_H