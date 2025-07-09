#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H

struct Particle {
	vf3d pos, vel, acc;
	float rot=0, rot_vel=0;
	float size=0, size_vel=0;
	float life=1;

	Particle() {}

	Particle(const vf3d& p, const vf3d& v, float r, float rv, float s, float sv) {
		pos=p;
		vel=v;
		rot=r;
		rot_vel=rv;
		size=s;
		size_vel=sv;
	}

	void accelerate(const vf3d& f) {
		acc+=f;
	}

	void update(float dt) {
		//euler integration
		vel+=acc*dt;
		pos+=vel*dt;

		//reset forces
		acc*=0;

		//update other things
		rot+=rot_vel*dt;
		size+=size_vel*dt;
	}

	bool isDead() const {
		return life<0;
	}
};
#endif