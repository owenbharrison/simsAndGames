#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H

struct Particle {
	vf2d pos, vel;
	float lifespan=0, age=0;

	void update(float dt) {
		pos+=vel*dt;
		age+=dt;
	}

	//is too old?
	bool isDead() const {
		return age>lifespan;
	}

	static Particle makeRandom(const vf2d& p) {
		float speed=cmn::randFloat(1, 6);
		float angle=2*cmn::Pi*cmn::randFloat();
		float lifespan=cmn::randFloat(1.6f, 3.8f);
		vf2d vel=cmn::polar<vf2d>(speed, angle);
		return {p, vel, lifespan};
	}
};
#endif