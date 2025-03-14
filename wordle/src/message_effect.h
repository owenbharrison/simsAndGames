#pragma once
#ifndef MESSAGE_EFFECT_STRUCT_H
#define MESSAGE_EFFECT_STRUCT_H

struct MessageEffect {
	std::string msg;
	olc::Pixel col;

	vf2d pos, vel, acc;
	float rot=0, rot_vel=0;

	float scl=1;

	float lifespan=0, age=0;

	MessageEffect() {}

	MessageEffect(const std::string& m, olc::Pixel c, vf2d p, vf2d v, float r, float s, float l) {
		msg=m;
		col=c;

		pos=p;
		vel=v;
		rot_vel=r;

		scl=s;

		lifespan=l;
	}

	void applyForce(const vf2d& f) {
		acc+=f;
	}

	void update(float dt) {
		vel+=acc*dt;
		pos+=vel*dt;
		acc={0, 0};

		rot+=rot_vel*dt;

		age+=dt;
	}

	bool isDead() const {
		return age>lifespan;
	}
};
#endif//MESSAGE_EFFECT_STRUCT_H