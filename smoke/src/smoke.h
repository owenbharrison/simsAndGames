#pragma once
#ifndef SMOKE_STRUCT_H
#define SMOKE_STRUCT_H

struct Smoke {
	vf2d pos, vel, acc;
	float rot=0, rot_vel=0;
	float size=0, size_vel=0;
	olc::Pixel col;
	float life=1;

	Smoke() {}

	Smoke(vf2d p, vf2d v, float r, float rv, float s, float sv, olc::Pixel c) {
		pos=p;
		vel=v;
		rot=r;
		rot_vel=rv;
		size=s;
		size_vel=sv;
		col=c;
	}

	void accelerate(const vf2d& f) {
		acc+=f;
	}

	void update(const float& dt) {
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
#endif SMOKE_STRUCT_H