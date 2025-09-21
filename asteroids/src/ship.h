#pragma once
#ifndef SHIP_STRUCT_H
#define SHIP_STRUCT_H

#include "bullet.h"

#include "particle.h"

struct Ship {
	vf2d pos, vel, acc;
	float rad=0, rot=0;

	Ship() {}

	Ship(const vf2d& p, float r) {
		pos=p;
		rad=r;
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

	void accelerate(const vf2d& a) {
		acc+=a;
	}

	//toroidal space
	void checkAABB(const cmn::AABB& a) {
		if(pos.x<a.min.x) pos.x=a.max.x;
		if(pos.y<a.min.y) pos.y=a.max.y;
		if(pos.x>a.max.x) pos.x=a.min.x;
		if(pos.y>a.max.y) pos.y=a.min.y;
	}

	Bullet getBullet() const {
		vf2d dir=cmn::polar<vf2d>(1, rot);
		return {pos+rad*dir, 63.f*dir};
	}

	//move forward
	void boost(float f) {
		accelerate(cmn::polar<vf2d>(f, rot));
	}

	void turn(float f) {
		rot+=f;
	}

	//random particle at end of ship
	Particle emitParticle() const {
		float speed=cmn::randFloat(3, 6);
		float rand_angle=cmn::Pi*cmn::randFloat(-.33f, .33f);
		vf2d vel=cmn::polar<vf2d>(speed, cmn::Pi+rot+rand_angle);
		float lifespan=cmn::randFloat(1.6f, 3.8f);
		return {pos-cmn::polar<vf2d>(rad, rot+rand_angle/2), vel, lifespan};
	}

	void getOutline(vf2d& a, vf2d& b, vf2d& c) const {
		vf2d fw=cmn::polar<vf2d>(rad, rot);
		vf2d up(-fw.y, fw.x);
		a=pos+1.2f*fw;
		b=pos-fw+up;
		c=pos-fw-up;
	}

	cmn::AABB getAABB() const {
		vf2d o[3];
		getOutline(o[0], o[1], o[2]);

		cmn::AABB a;
		for(int i=0; i<3; i++) {
			a.fitToEnclose(o[i]);
		}
		return a;
	}
};
#endif