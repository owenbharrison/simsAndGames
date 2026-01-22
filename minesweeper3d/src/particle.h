#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H

struct Particle {
	cmn::vf3d pos, vel, acc;
	float size=0;
	float age=0, lifespan=0;

	Particle() {}

	Particle(const cmn::vf3d& p, const cmn::vf3d& v, float s, float l) {
		pos=p;
		vel=v;
		size=s;
		lifespan=l;
	}

	void accelerate(const cmn::vf3d& a) {
		acc+=a;
	}

	void update(float dt) {
		vel+=acc*dt;
		pos+=vel*dt;

		acc={0, 0, 0};

		age+=dt;
	}

	bool isDead() const {
		return age>lifespan;
	}
};
#endif