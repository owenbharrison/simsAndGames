#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H

struct Particle {
	vf3d pos, oldpos, acc;
	bool locked=false;

	Particle() {}

	Particle(vf3d p) {
		pos=p;
		oldpos=p;
	}

	void applyForce(const vf3d& f) {
		acc+=f;
	}

	void update(float dt) {
		//update pos store
		vf3d vel=pos-oldpos;
		oldpos=pos;

		//verlet integration
		if(!locked) pos+=vel+acc*dt*dt;

		//reset forces
		acc*=0;
	}
};
#endif