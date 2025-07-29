#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H

struct Particle {
	vf3d pos, vel, acc;
	float rot=0, rot_vel=0;
	float size=0, size_vel=0;
	float lifespan=0, age=0;
	enum Type {
		SMOKE=0,
		LIGHTNING,
		NUM_TYPES
	} type=SMOKE;
	unsigned short id=0;
	static unsigned short total_id;

	Particle() {}

	Particle(Type t, const vf3d& p, const vf3d& v, float r, float rv, float s, float sv, float l) {
		type=t;
		pos=p;
		vel=v;
		rot=r;
		rot_vel=rv;
		size=s;
		size_vel=sv;
		lifespan=l;
		id=total_id++;
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

		//aging
		age+=dt;
	}

	bool isDead() const {
		return age>lifespan;
	}
};

unsigned short Particle::total_id=0;
#endif