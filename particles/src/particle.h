#pragma once
#ifndef PARTICLE_CLASS_H
#define PARTICLE_CLASS_H

struct Particle {
	vf2d pos, oldpos, forces;
	float rad=0;
	float mass=1;
	olc::Pixel col=olc::WHITE;

	Particle() {}

	Particle(const vf2d& p, const float& r) {
		pos=p;
		oldpos=p;
		rad=r;
		mass=cmn::Pi*rad*rad;
	}

	void applyForce(const vf2d& f) {
		forces+=f;
	}

	void update(const float& dt) {
		//update pos store
		vf2d vel=pos-oldpos;
		oldpos=pos;

		//verlet integration
		vf2d acc=forces/mass;
		pos+=vel+acc*dt*dt;

		//reset forces
		forces={0, 0};
	}

	void keepIn(const cmn::AABB& box) {
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

	static void checkCollide(Particle& a, Particle& b) {
		//are the circles overlapping?
		vf2d sub=a.pos-b.pos;
		float dist_sq=sub.mag2();
		float t_rad=a.rad+b.rad;
		if(dist_sq<t_rad*t_rad) {
			float dist=std::sqrt(dist_sq);
			float delta=dist-t_rad;

			//dont divide by 0
			vf2d norm(1, 0);
			if(dist>1e-6f) norm=sub/dist;

			float ia=1/a.mass, ib=1/b.mass;
			vf2d diff=delta*norm/(ia+ib);
			a.pos-=ia*diff;
			b.pos+=ib*diff;
		}
	}
};
#endif