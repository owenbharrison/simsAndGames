//https://en.wikipedia.org/wiki/Iron
//https://en.wikipedia.org/wiki/Force_between_magnets
#pragma once
#ifndef MAGNET_CLASS_H
#define MAGNET_CLASS_H

#include "cmn/math/v2d.h"

constexpr double Pi=3.141592653589793;

cmn::vd2d polar(double rad, double angle) {
	return {
		rad* std::cos(angle),
		rad* std::sin(angle)
	};
}

class Magnet {
	double rad=0;
	double mass=1;
	double inertia=1;

	cmn::vd2d old_pos, forces;
	double old_rot=0, torques=0;

public:
	cmn::vd2d pos;
	double rot=0;

	Magnet(cmn::vd2d p, double r) {
		pos=p;
		old_pos=p;

		rad=r;
		double volume=4*Pi/3*r*r*r;
		//density of iron
		mass=7874*volume;
		inertia=.5*mass*r*r;
	}

	double getRad() const { return rad; }
	double getMass() const { return mass; }
	double getInertia() const { return inertia; }

	cmn::vd2d getVel(double dt=1) const { return (pos-old_pos)/dt; }
	double getRotVel(double dt=1) const { return (rot-old_rot)/dt; }

	void applyPureForce(const cmn::vd2d& f) { forces+=f; }
	void applyPureTorque(double t) { torques+=t; }

	void applyForce(const cmn::vd2d& f, const cmn::vd2d& p) {
		applyPureForce(f);

		//rxF
		cmn::vd2d r=p-pos;
		applyPureTorque(r.x*f.y-r.y*f.x);
	}

	void update(double dt) {
		cmn::vd2d vel=getVel();
		double rot_vel=getRotVel();

		//store previous
		old_pos=pos;
		old_rot=rot;

		//get acceleration
		cmn::vd2d acc=forces/mass;
		double rot_acc=torques/inertia;
		
		//timestep-independent damping
		double k=1.81;
		double damp=std::exp(-k*dt);

		//verlet integration
		pos+=damp*vel+acc*dt*dt;
		rot+=damp*rot_vel+rot_acc*dt*dt;

		//reset
		forces={0, 0};
		torques=0;
	}

	//reflect & damp
	void keepInside(const cmn::vd2d& min, const cmn::vd2d& max) {
		const double bounce=.6;

		cmn::vd2d vel=pos-old_pos;
		if(pos.x<min.x+rad) pos.x=min.x+rad, vel.x*=-bounce;
		if(pos.y<min.y+rad) pos.y=min.y+rad, vel.y*=-bounce;
		if(pos.x>max.x-rad) pos.x=max.x-rad, vel.x*=-bounce;
		if(pos.y>max.y-rad) pos.y=max.y-rad, vel.y*=-bounce;

		old_pos=pos-vel;
	}

	static void applyMonopoleForces(Magnet& a, Magnet& b) {
		//get directions
		cmn::vd2d a_sn=polar(1, a.rot);
		cmn::vd2d b_sn=polar(1, b.rot);

		//get pole locations(inside magnet?)
		double a_rad=a.getRad();
		double b_rad=b.getRad();
		cmn::vd2d a_n=a.pos+.5*a_rad*a_sn;
		cmn::vd2d a_s=a.pos-.5*a_rad*a_sn;
		cmn::vd2d b_n=b.pos+.5*b_rad*b_sn;
		cmn::vd2d b_s=b.pos-.5*b_rad*b_sn;

		//get vectors
		cmn::vd2d anbn=b_n-a_n;
		cmn::vd2d anbs=b_s-a_n;
		cmn::vd2d asbn=b_n-a_s;
		cmn::vd2d asbs=b_s-a_s;

		//separation distances
		double anbn_r=anbn.mag();
		double anbs_r=anbs.mag();
		double asbn_r=asbn.mag();
		double asbs_r=asbs.mag();

		//safe norm(ignore) + inverse square law
		const double u=4*Pi*1e-7;//permeability
		const double q1=1, q2=1;
		const double coeff=u*q1*q2/4/Pi;
		cmn::vd2d f_anbn=(anbn_r<1e-6?0:coeff/(anbn_r*(1e-4+anbn_r*anbn_r)))*anbn;
		cmn::vd2d f_anbs=(anbs_r<1e-6?0:coeff/(anbs_r*(1e-4+anbs_r*anbs_r)))*anbs;
		cmn::vd2d f_asbn=(asbn_r<1e-6?0:coeff/(asbn_r*(1e-4+asbn_r*asbn_r)))*asbn;
		cmn::vd2d f_asbs=(asbs_r<1e-6?0:coeff/(asbs_r*(1e-4+asbs_r*asbs_r)))*asbs;

		//final force applications
		a.applyForce(f_anbs-f_anbn, a_n);
		a.applyForce(f_asbn-f_asbs, a_s);
		b.applyForce(f_anbn-f_asbn, b_n);
		b.applyForce(f_asbs-f_anbs, b_s);
	}

	static void checkCollide(Magnet& a, Magnet& b) {
		//are the circles overlapping?
		cmn::vd2d ab=b.pos-a.pos;
		double dist_sq=ab.mag_sq();
		double t_rad=a.rad+b.rad;
		if(dist_sq>t_rad*t_rad) return;

		double dist=std::sqrt(dist_sq);

		//safe norm
		cmn::vd2d norm;
		const double eps=1e-6;
		if(dist<eps) norm=cmn::vd2d(1, 0), dist=eps;
		else norm=ab/dist;

		//inverse mass weighting
		double pen=t_rad-dist;
		double ia=1/a.mass;
		double ib=1/b.mass;
		double iit=1/(ia+ib);
		cmn::vd2d corr=pen*norm*iit;
		a.pos-=ia*corr;
		b.pos+=ib*corr;

		//velocity correction
		cmn::vd2d va=a.pos-a.old_pos;
		cmn::vd2d vb=b.pos-b.old_pos;

		//fix if moving towards eachother
		cmn::vd2d rel_vel=vb-va;
		double vn=norm.dot(rel_vel);
		if(vn<0) {
			const double restitution=.1;

			double j=-(1+restitution)*vn*iit;
			cmn::vd2d impulse=j*norm;
			va-=ia*impulse;
			vb+=ib*impulse;

			a.old_pos=a.pos-va;
			b.old_pos=b.pos-vb;
		}
	}
};
#endif