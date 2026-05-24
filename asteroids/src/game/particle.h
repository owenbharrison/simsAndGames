#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H

#include "cmn/math/v2d.h"

#include "cmn/utils.h"

struct Particle {
	cmn::vf2d pos, vel;
	float lifespan=0, age=0;

	float r=1, g=1, b=1;

	void update(float dt) {
		pos+=vel*dt;

		age+=dt;
	}

	//is too old?
	bool isDead() const {
		return age>lifespan;
	}

	static Particle makeRandom(const cmn::vf2d& pos, float r, float g, float b) {
		Particle p;
		p.pos=pos;
		float speed=cmn::randFloat(1, 6);
		float angle=cmn::randFloat(2*cmn::Pi);
		p.vel=cmn::vf2d::polar({speed, angle});
		p.lifespan=cmn::randFloat(1.6f, 3.8f);
		//randomize color
		p.r=cmn::clamp(r+.15f*cmn::randFloat(-1, 1), 0.f, 1.f);
		p.g=cmn::clamp(g+.15f*cmn::randFloat(-1, 1), 0.f, 1.f);
		p.b=cmn::clamp(b+.15f*cmn::randFloat(-1, 1), 0.f, 1.f);
		return p;
	}
};
#endif