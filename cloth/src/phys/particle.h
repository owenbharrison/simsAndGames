#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H

struct Particle {
	vf3d pos, old_pos, acc;
	bool locked=false;
	vf3d uv;

	Particle() {}

	Particle(vf3d p) {
		pos=p;
		old_pos=p;
	}

	void accelerate(const vf3d& f) {
		acc+=f;
	}

	void update(float dt) {
		//update pos store
		vf3d vel=pos-old_pos;
		old_pos=pos;

		//verlet integration
		if(!locked) pos+=vel+acc*dt*dt;

		//reset forces
		acc*=0;
	}
};
#endif