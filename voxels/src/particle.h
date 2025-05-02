#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H
struct Particle {
	vf3d pos, oldpos, acc;
	olc::Pixel col=olc::WHITE;
	float rad=.1f;

	Particle() {}

	Particle(vf3d p) {
		pos=p;
		oldpos=p;
	}

	void accelerate(const vf3d& f) {
		acc+=f;
	}

	void update(float dt) {
		//position store
		vf3d vel=pos-oldpos;
		oldpos=pos;

		//verlet integration
		pos+=vel+acc*dt*dt;

		//reset forces
		acc*=0;
	}

	void keepIn(const AABB3& a) {
		vf3d vel=pos-oldpos;
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
		if(pos.z>a.max.z) {
			pos.z=a.max.z;
			oldpos.z=a.max.z+vel.z;
		}
		if(pos.z>a.max.y) {
			pos.z=a.max.z;
			oldpos.z=a.max.z+vel.z;
		}
	}
};
#endif