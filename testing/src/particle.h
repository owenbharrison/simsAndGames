#pragma once
#ifndef PARTICLE_CLASS_H
#define PARTICLE_CLASS_H

struct Particle {
	vf3d pos, oldpos, forces;
	float rad=0;
	float mass=1, inv_mass=1;
	olc::Pixel col=olc::WHITE;

	Particle() {}

	Particle(const vf3d& p, const float& r) {
		pos=p;
		oldpos=p;
		rad=r;
		mass=4*cmn::Pi/3*rad*rad*rad;
		inv_mass=1/mass;
	}

	void applyForce(const vf3d& f) {
		forces+=f;
	}

	void update(const float& dt) {
		//update pos store
		vf3d vel=pos-oldpos;
		oldpos=pos;

		//verlet integration
		vf3d acc=forces/mass;
		pos+=vel+acc*dt*dt;

		//reset forces
		forces={0, 0, 0};
	}

	//keep inside box, bounce off walls
	void keepIn(const cmn::AABB3& box) {
		vf3d vel=pos-oldpos;
		if(pos.x<box.min.x+rad) {
			pos.x=box.min.x+rad;
			oldpos.x=pos.x+vel.x;
		}
		if(pos.y<box.min.y+rad) {
			pos.y=box.min.y+rad;
			oldpos.y=pos.y+vel.y;
		}
		if(pos.z<box.min.z+rad) {
			pos.z=box.min.z+rad;
			oldpos.z=pos.z+vel.z;
		}
		if(pos.x>box.max.x-rad) {
			pos.x=box.max.x-rad;
			oldpos.x=pos.x+vel.x;
		}
		if(pos.y>box.max.y-rad) {
			pos.y=box.max.y-rad;
			oldpos.y=pos.y+vel.y;
		}
		if(pos.z>box.max.z-rad) {
			pos.z=box.max.z-rad;
			oldpos.z=pos.z+vel.z;
		}
	}

	static void checkCollide(Particle& a, Particle& b) {
		//are the circles overlapping?
		vf3d sub=a.pos-b.pos;
		float dist_sq=sub.mag2();
		float t_rad=a.rad+b.rad;
		if(dist_sq<t_rad*t_rad) {
			float dist=std::sqrt(dist_sq);
			float delta=dist-t_rad;

			//dont divide by 0
			vf3d norm(1, 0, 0);
			if(dist>1e-6f) norm=sub/dist;

			//move porportional to inv mass
			float inv_sum=a.inv_mass+b.inv_mass;
			vf3d diff=delta*norm/inv_sum;
			a.pos-=a.inv_mass*diff;
			b.pos+=b.inv_mass*diff;
		}
	}
};
#endif