#pragma once
#ifndef PARTICLE_STRUCTS_H
#define PARTICLE_STRUCTS_H

//basic particle functionality
struct Particle {
protected:
	int id=0;

public:
	vf2d pos, vel, acc;
	float lifespan=0, age=0;

	olc::Pixel col;

	enum {
		BASE,
		MESSAGE,
		SPLATTER
	};

	Particle()=default;

	Particle(vf2d p, vf2d v, olc::Pixel c, float l) {
		pos=p;
		vel=v;
		col=c;
		lifespan=l;
	}

	int getID() const {
		return id;
	}

	void applyForce(const vf2d& f) {
		acc+=f;
	}

	virtual void update(float dt) {
		vel+=acc*dt;
		pos+=vel*dt;
		acc={0, 0};

		//~99% compounding
		float drag=1-.6f*dt;
		vel*=drag;

		age+=dt;
	}

	bool isDead() const {
		return age>lifespan;
	}
};

//string and rotation
struct Message : Particle {
	std::string msg;
	float rot=0, rot_vel=0;

	Message()=default;

	Message(vf2d p, vf2d v, olc::Pixel c, float l, const std::string& m, float r) : Particle(p, v, c, l) {
		msg=m;
		rot_vel=r;
		id=MESSAGE;
	}

	void update(float dt) override {
		Particle::update(dt);

		rot+=rot_vel*dt;

		float drag=1-.6f*dt;
		rot_vel*=drag;
	}
};

//this just has a radius
struct Splatter : Particle {
	float rad=0;

	Splatter()=default;

	Splatter(vf2d p, vf2d v, olc::Pixel c, float l, float r) : Particle(p, v, c, l) {
		rad=r;
		id=SPLATTER;
	}
};

//add explosion emitter
#endif