#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H

struct Particle {
	vf3d pos, old_pos, forces;
	float mass=1;
	bool locked=false;

	static const float rad;

	Particle() {}

	Particle(vf3d p) {
		pos=p;
		old_pos=p;
	}

	void accelerate(const vf3d& a) {
		forces+=mass*a;
	}

	void applyForce(const vf3d& f) {
		forces+=f;
	}

	void update(float dt) {
		//update pos store
		vf3d vel=pos-old_pos;
		old_pos=pos;

		//verlet integration
		vf3d acc=forces/mass;
		if(!locked) pos+=vel+acc*dt*dt;

		//reset forces
		forces*=0;
	}
};

const float Particle::rad=.1f;
#endif