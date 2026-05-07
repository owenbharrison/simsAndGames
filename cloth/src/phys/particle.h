#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H

#include "cmn/math/v3d.h"

struct Particle {
	cmn::vf3d pos, old_pos, forces;
	float mass=1;
	bool locked=false;
	float tex_u=0, tex_v=0;

	Particle() {}

	Particle(cmn::vf3d p, float m) {
		pos=p;
		old_pos=p;
		mass=m;
	}

	void applyForce(const cmn::vf3d& f) {
		forces+=f;
	}

	void update(float dt) {
		//update pos store
		cmn::vf3d vel=pos-old_pos;
		old_pos=pos;

		//verlet integration
		cmn::vf3d acc=forces/mass;
		if(!locked) pos+=vel+acc*dt*dt;

		//reset forces
		forces*=0;
	}
};
#endif