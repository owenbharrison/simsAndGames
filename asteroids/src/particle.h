#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H

struct Particle {
	vf2d pos, vel;
	float lifespan=0, age=0;

	olc::Pixel col=olc::WHITE;

	Particle() {}

	Particle(const vf2d& p, const vf2d& v, float l, const olc::Pixel& c) {
		pos=p;
		vel=v;
		lifespan=l;
		col=c;
	}

	//is too old?
	bool isDead() const {
		return age>lifespan;
	}

	void update(float dt) {
		pos+=vel*dt;

		age+=dt;
	}

	static Particle makeRandom(const vf2d& p, const olc::Pixel& col) {
		float speed=cmn::randFloat(1, 6);
		float angle=2*cmn::Pi*cmn::randFloat();
		float lifespan=cmn::randFloat(1.6f, 3.8f);
		vf2d vel=cmn::polar<vf2d>(speed, angle);
		return Particle(p, vel, lifespan, col);
	}
};
#endif