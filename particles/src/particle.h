#pragma once
#ifndef PARTICLE_CLASS_H
#define PARTICLE_CLASS_H

constexpr float Pi=3.1415927f;

struct Particle {
	vf2d pos, oldpos, forces;
	float rad=0;
	float mass=1, inv_mass=1;
	int id=0;

	Particle() {}

	Particle(const vf2d& p, const float& r) {
		pos=p;
		oldpos=p;
		rad=r;
		mass=Pi*rad*rad;
		inv_mass=1/mass;
	}

	void applyForce(const vf2d& f) {
		forces+=f;
	}

	void update(const float& dt) {
		//update pos store
		olc::vf2d vel=pos-oldpos;
		oldpos=pos;

		//verlet integration
		vf2d acc=forces/mass;
		pos+=vel+acc*dt*dt;

		//reset forces
		forces={0, 0};
	}

	void keepIn(const AABB& box) {
		vf2d vel=pos-oldpos;
		if(pos.x<box.min.x+rad) {
			pos.x=box.min.x+rad;
			oldpos.x=pos.x+vel.x;
		}
		if(pos.y<box.min.y+rad) {
			pos.y=box.min.y+rad;
			oldpos.y=pos.y+vel.y;
		}
		if(pos.x>box.max.x-rad) {
			pos.x=box.max.x-rad;
			oldpos.x=pos.x+vel.x;
		}
		if(pos.y>box.max.y-rad) {
			pos.y=box.max.y-rad;
			oldpos.y=pos.y+vel.y;
		}
	}
};
#endif