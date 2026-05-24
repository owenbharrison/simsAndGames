#pragma once
#ifndef SHIP_STRUCT_H
#define SHIP_STRUCT_H

#include "bullet.h"

#include "particle.h"

#include "cmn/geom/aabb2.h"

#include "cmn/utils.h"

struct Ship {
	cmn::vf2d pos, vel, acc;
	float rad=0, rot=0;

	Ship() {}

	Ship(const cmn::vf2d& p, float r) {
		pos=p;
		rad=r;
	}

	Bullet getBullet() const {
		cmn::vf2d dir=cmn::vf2d::polar({1.f, rot});
		return {pos+rad*dir, 63.f*dir};
	}

	//random particle at end of ship
	Particle emitParticle() const {
		Particle p;
		float noise=.33f*cmn::Pi*cmn::randFloat(-1, 1);
		p.pos=pos-cmn::vf2d::polar({rad, rot+noise/2});
		float speed=cmn::randFloat(3, 6);
		p.vel=-cmn::vf2d::polar({speed, rot+noise});
		p.lifespan=cmn::randFloat(1.6f, 3.8f);
		float r=1, g=.35f, b=0;//orangeish
		//randomize color
		p.r=cmn::clamp(r+.25f*cmn::randFloat(-1, 1), 0.f, 1.f);
		p.g=cmn::clamp(g+.25f*cmn::randFloat(-1, 1), 0.f, 1.f);
		p.b=cmn::clamp(b+.25f*cmn::randFloat(-1, 1), 0.f, 1.f);
		return p;
	}

	void getOutline(cmn::vf2d& a, cmn::vf2d& b, cmn::vf2d& c) const {
		cmn::vf2d fw=cmn::vf2d::polar({rad, rot});
		cmn::vf2d up(-fw.y, fw.x);
		a=pos+1.2f*fw;
		b=pos-fw+up;
		c=pos-fw-up;
	}

	cmn::AABBf2 getAABB() const {
		cmn::vf2d a, b, c;
		getOutline(a, b, c);

		const cmn::vf2d inf(1e300, 1e300);
		cmn::AABBf2 box{inf, -inf};
		box.fitToEnclose(a);
		box.fitToEnclose(b);
		box.fitToEnclose(c);
		return box;
	}

	void accelerate(const cmn::vf2d& a) {
		acc+=a;
	}

	//move forward
	void boost(float f) {
		accelerate(cmn::vf2d::polar({f, rot}));
	}

	void turn(float f) {
		rot+=f;
	}

	void update(float dt) {
		//drag
		accelerate(-.46f*vel);

		//kinematics
		vel+=acc*dt;
		pos+=vel*dt;

		//reset "forces"
		acc*=0;
	}

	//toroidal space
	void checkAABB(const cmn::AABBf2& a) {
		if(pos.x<a.min.x) pos.x=a.max.x;
		if(pos.y<a.min.y) pos.y=a.max.y;
		if(pos.x>a.max.x) pos.x=a.min.x;
		if(pos.y>a.max.y) pos.y=a.min.y;
	}
};
#endif