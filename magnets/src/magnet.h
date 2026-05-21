//https://en.wikipedia.org/wiki/Iron
//https://en.wikipedia.org/wiki/Force_between_magnets
#pragma once
#ifndef MAGNET_CLASS_H
#define MAGNET_CLASS_H

#include "cmn/geom/aabb2.h"

#include "cmn/utils.h"

constexpr double Pi=3.141592653589793;

class Magnet {
	double rad=0;
	double mass=1;
	double inertia=1;

	cmn::vd2d old_pos, forces;
	double old_rot=0, torques=0;

	cmn::vd2d sincos{0, 1};

	static const double rel_pole_rad;

	void updateRot();

public:
	cmn::vd2d pos;
	double rot=0;

	Magnet(cmn::vd2d p, double ra, double ro=0) {
		pos=p;
		old_pos=p;

		rot=ro;
		old_rot=rot;

		rad=ra;
		double volume=4*Pi/3*rad*rad*rad;
		//density of iron
		mass=7874*volume;
		inertia=.5*mass*rad*rad;

		updateRot();
	}

	double getRad() const { return rad; }
	double getMass() const { return mass; }
	double getInertia() const { return inertia; }

	cmn::vd2d getVel(double dt=1) const { return (pos-old_pos)/dt; }
	double getRotVel(double dt=1) const { return (rot-old_rot)/dt; }

	cmn::vd2d rotVec(const cmn::vd2d& p) const {
		return {p.x*sincos.y-p.y*sincos.x, p.x*sincos.x+p.y*sincos.y};
	}

	cmn::vd2d unrotVec(const cmn::vd2d& p) const {
		return {p.x*sincos.y+p.y*sincos.x, -p.x*sincos.x+p.y*sincos.y};
	}

	cmn::vd2d loc2wld(const cmn::vd2d& l) const {
		return pos+rotVec(l);
	}

	cmn::vd2d wld2loc(const cmn::vd2d& w) const {
		return unrotVec(w-pos);
	}

	cmn::vd2d getNorth() const {
		return loc2wld({rel_pole_rad*rad, 0});
	}

	cmn::vd2d getSouth() const {
		return loc2wld({-rel_pole_rad*rad, 0});
	}

#pragma region PHYSICS
	void applyPureForce(const cmn::vd2d& f) { forces+=f; }
	void applyPureTorque(double t) { torques+=t; }

	//world space position
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

		updateRot();
	}

	//reflect & damp
	void keepInside(const cmn::AABBd2& box) {
		const double bounce=.6;

		cmn::vd2d vel=pos-old_pos;
		if(pos.x<box.min.x+rad) pos.x=box.min.x+rad, vel.x*=-bounce;
		if(pos.y<box.min.y+rad) pos.y=box.min.y+rad, vel.y*=-bounce;
		if(pos.x>box.max.x-rad) pos.x=box.max.x-rad, vel.x*=-bounce;
		if(pos.y>box.max.y-rad) pos.y=box.max.y-rad, vel.y*=-bounce;

		old_pos=pos-vel;
	}
#pragma endregion

#pragma region BEHAVIORS
	static void applyMonopoleForces(Magnet& a, Magnet& b) {
		//get pole locations
		cmn::vd2d a_n=a.getNorth();
		cmn::vd2d a_s=a.getSouth();
		cmn::vd2d b_n=b.getNorth();
		cmn::vd2d b_s=b.getSouth();

		//get vectors between
		cmn::vd2d anbn=b_n-a_n;
		cmn::vd2d anbs=b_s-a_n;
		cmn::vd2d asbn=b_n-a_s;
		cmn::vd2d asbs=b_s-a_s;

		//separation distances
		double anbn_l=length(anbn);
		double anbs_l=length(anbs);
		double asbn_l=length(asbn);
		double asbs_l=length(asbs);

		//safe norm(ignore) + inverse square law
		const double u=4*Pi*1e-7;//permeability
		const double q1=1, q2=1;
		const double coeff=u*q1*q2/4/Pi;
		cmn::vd2d f_anbn=(anbn_l<1e-6?0:coeff/anbn_l/(1e-4+anbn_l*anbn_l))*anbn;
		cmn::vd2d f_anbs=(anbs_l<1e-6?0:coeff/anbs_l/(1e-4+anbs_l*anbs_l))*anbs;
		cmn::vd2d f_asbn=(asbn_l<1e-6?0:coeff/asbn_l/(1e-4+asbn_l*asbn_l))*asbn;
		cmn::vd2d f_asbs=(asbs_l<1e-6?0:coeff/asbs_l/(1e-4+asbs_l*asbs_l))*asbs;

		//final force applications
		a.applyForce(f_anbs-f_anbn, a_n);
		a.applyForce(f_asbn-f_asbs, a_s);
		b.applyForce(f_anbn-f_asbn, b_n);
		b.applyForce(f_asbs-f_anbs, b_s);
	}

	static void checkCollide(Magnet& a, Magnet& b) {
		//are the circles overlapping?
		cmn::vd2d ab=b.pos-a.pos;
		double dist_sq=dot(ab, ab);
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
		double vn=dot(norm, rel_vel);
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
#pragma endregion
};

//pole inside magnet
const double Magnet::rel_pole_rad=.75;

//precompute rotation matrix
void Magnet::updateRot() {
	sincos.x=std::sin(rot);
	sincos.y=std::cos(rot);
}
#endif