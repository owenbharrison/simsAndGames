#pragma once
#ifndef POINTMASS_STRUCT_H
#define POINTMASS_STRUCT_H

#include "../geom/aabb.h"

struct PointMass {
	vf2d pos, oldpos, force;
	float mass=0;
	bool locked=false;

	PointMass() {}

	PointMass(vf2d p, float m) : pos(p), oldpos(p), mass(m) {}

	void applyForce(const vf2d& f) { force+=f; }

	void update(float dt) {
		//update pos store
		vf2d vel=pos-oldpos;
		oldpos=pos;

		//verlet integration
		vf2d acc=force/mass;
		if(!locked) pos+=vel+acc*dt*dt;

		//reset forces
		force={0, 0};
	}

	void keepIn(const AABB& a) {
		vf2d vel=pos-oldpos;
		if(pos.x<a.min.x) {
			pos.x=a.min.x;
			oldpos.x=a.min.x+vel.x;
		}
		if(pos.y<a.min.y) {
			pos.y=a.min.y;
			oldpos.y=a.min.y+vel.y;
		}
		if(pos.x>a.max.x) {
			pos.x=a.max.x;
			oldpos.x=a.max.x+vel.x;
		}
		if(pos.y>a.max.y) {
			pos.y=a.max.y;
			oldpos.y=a.max.y+vel.y;
		}
	}

	//keepout?

	static void elasticCollision(PointMass& a, PointMass& b) {
		vf2d vel_a=a.pos-a.oldpos, vel_b=b.pos-b.oldpos;
	}
};
#endif//POINTMASS_STRUCT_H