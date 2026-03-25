#pragma once
#ifndef PARTICLE_CLASS_H
#define PARTICLE_CLASS_H

#include "cmn/math/v2d.h"

//for pi
#include "cmn/utils.h"

class Particle {
	float rad=0;
	float mass=1;
	float inv_mass=1;

public:
	cmn::vf2d pos, oldpos, forces;
	float r=1, g=1, b=1;

	Particle() {}

	Particle(const cmn::vf2d& p, float r) {
		pos=p;
		oldpos=p;
		rad=r;
		mass=cmn::Pi*rad*rad;
		inv_mass=1/mass;
	}

	float getRadius() const { return rad; }
	float getMass() const { return mass; }

	void applyForce(const cmn::vf2d& f) {
		forces+=f;
	}

	void update(const float& dt) {
		//update pos store
		cmn::vf2d vel=pos-oldpos;
		oldpos=pos;

		//verlet integration
		cmn::vf2d acc=forces/mass;
		pos+=vel+acc*dt*dt;

		//reset forces
		forces={0, 0};
	}

	//reflect & damp
	void keepInsideRegion(const cmn::vf2d& min, const cmn::vf2d& max) {
		const float bounce=.4f;

		cmn::vf2d vel=pos-oldpos;
		if(pos.x<min.x+rad) pos.x=min.x+rad, vel.x*=-bounce;
		if(pos.y<min.y+rad) pos.y=min.y+rad, vel.y*=-bounce;
		if(pos.x>max.x-rad) pos.x=max.x-rad, vel.x*=-bounce;
		if(pos.y>max.y-rad) pos.y=max.y-rad, vel.y*=-bounce;

		oldpos=pos-vel;
	}

	static void checkCollide(Particle& a, Particle& b) {
		//are the circles overlapping?
		cmn::vf2d ab=b.pos-a.pos;
		float dist_sq=ab.mag_sq();
		float t_rad=a.rad+b.rad;
		if(dist_sq>t_rad*t_rad) return;

		float dist=std::sqrt(dist_sq);

		//safe norm
		cmn::vf2d norm;
		const float eps=1e-6f;
		if(dist<eps) norm=cmn::vf2d(1, 0), dist=eps;
		else norm=ab/dist;

		//inverse mass weighting
		float pen=t_rad-dist;
		float iit=1/(a.inv_mass+b.inv_mass);
		cmn::vf2d corr=pen*norm*iit;
		a.pos-=a.inv_mass*corr;
		b.pos+=b.inv_mass*corr;

		//velocity correction
		cmn::vf2d va=a.pos-a.oldpos;
		cmn::vf2d vb=b.pos-b.oldpos;

		//fix if moving towards eachother
		cmn::vf2d rel_vel=vb-va;
		float vn=norm.dot(rel_vel);
		if(vn<0) {
			float restitution=.1f;

			float j=-(1+restitution)*vn*iit;
			cmn::vf2d impulse=j*norm;
			va-=a.inv_mass*impulse;
			vb+=b.inv_mass*impulse;

			a.oldpos=a.pos-va;
			b.oldpos=b.pos-vb;
		}
	}
};
#endif