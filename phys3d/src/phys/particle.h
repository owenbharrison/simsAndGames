#pragma once
#ifndef PARTICLE_STRUCT_H
#define PARTICLE_STRUCT_H

#include "cmn/geom/aabb3.h"

struct Particle {
	static float restitution, friction;

	cmn::vf3d pos, pos_old, forces;
	float rad=0, mass=1;

	Particle(const cmn::vf3d& p, float r, float m) {
		pos=p;
		pos_old=pos;
		rad=r;
		mass=m;
	}

	void applyForce(const cmn::vf3d& f) {
		forces+=f;
	}

	void update(float dt) {
		//get vel & store pos
		cmn::vf3d vel=pos-pos_old;
		pos_old=pos;

		//verlet integration
		cmn::vf3d acc=forces/mass;
		pos+=vel+acc*dt*dt;

		//reset forces
		forces={0, 0, 0};
	}

	//plane collision with inelasticities
	void collideWithPlane(const cmn::vf3d& ctr, const cmn::vf3d& norm, float dt) {
		//skip if at least rad in front of plane
		float dist_norm=dot(pos-ctr, norm);
		if(dist_norm>rad) return;

		//get "velocity"
		cmn::vf3d vel=pos-pos_old;

		//place rad away from plane
		pos+=(rad-dist_norm)*norm;

		//skip if not moving into surface
		float norm_comp=dot(vel, norm);
		if(norm_comp>0) return;

		//get normal & tangential components
		cmn::vf3d vel_norm=norm_comp*norm;
		cmn::vf3d vel_tang=vel-vel_norm;
		
		//apply restitution
		vel_norm*=-restitution;
		
		//apply timestep independent friction
		float f=std::pow(friction, dt);
		vel_tang*=f;

		//recombine & update pos store
		cmn::vf3d vel_new=vel_norm+vel_tang;
		pos_old=pos-vel_new;
	}

	void keepInside(const cmn::AABBf3& box, float dt) {
		collideWithPlane({box.min.x, box.min.y, box.min.z}, {+1, 0, 0}, dt);//-x
		collideWithPlane({box.max.x, box.min.y, box.min.z}, {-1, 0, 0}, dt);//+x
		collideWithPlane({box.min.x, box.min.y, box.min.z}, {0, +1, 0}, dt);//-y
		collideWithPlane({box.min.x, box.max.y, box.min.z}, {0, -1, 0}, dt);//+y
		collideWithPlane({box.min.x, box.min.y, box.min.z}, {0, 0, +1}, dt);//-z
		collideWithPlane({box.min.x, box.min.y, box.max.z}, {0, 0, -1}, dt);//+z
	}

	void keepOutside(const cmn::AABBf3& box, float dt) {
		collideWithPlane({box.min.x, box.min.y, box.min.z}, {-1, 0, 0}, dt);//-x
		collideWithPlane({box.max.x, box.min.y, box.min.z}, {+1, 0, 0}, dt);//+x
		collideWithPlane({box.min.x, box.min.y, box.min.z}, {0, -1, 0}, dt);//-y
		collideWithPlane({box.min.x, box.max.y, box.min.z}, {0, +1, 0}, dt);//+y
		collideWithPlane({box.min.x, box.min.y, box.min.z}, {0, 0, -1}, dt);//-z
		collideWithPlane({box.min.x, box.min.y, box.max.z}, {0, 0, +1}, dt);//+z
	}
};

float Particle::restitution=.75f;
float Particle::friction=.1f;
#endif