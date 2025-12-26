#pragma once
#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

#include "cmn/utils.h"
#include "cmn/geom/aabb.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

#include <vector>
#include <algorithm>
#include <string>

class Shape {
	int num_pts;

	void copyFrom(const Shape&), clear();

public:
	vf2d* points=nullptr;
	vf2d pos, old_pos, forces;
	float mass=1;
	float inv_mass=1;

	float rot=0, old_rot=0, torques=0;
	vf2d cossin{1, 0};
	float rotational_inertia=1;
	float inv_rotational_inertia=1;

	bool locked=false;

	olc::Pixel col=olc::WHITE;

	Shape() : Shape({0, 0}, 0, 1) {}

	Shape(vf2d ctr, float rad, int n) {
		//allocate
		num_pts=n;
		points=new vf2d[num_pts];

		//make n-gon
		for(int i=0; i<num_pts; i++) {
			float angle=2*cmn::Pi*i/num_pts;
			points[i]=ctr+cmn::polar<vf2d>(rad, angle);
		}

		initMass();
		initInertia();
	}

	Shape(const cmn::AABB& a) {
		//allocate
		num_pts=4;
		points=new vf2d[num_pts];

		//make rect
		points[0]=a.min;
		points[1]={a.max.x, a.min.y};
		points[2]=a.max;
		points[3]={a.min.x, a.max.y};

		initMass();
		initInertia();
	}

	//ro3 1
	Shape(const std::vector<vf2d>& pts) {
		//allocate
		num_pts=pts.size();
		points=new vf2d[num_pts];

		//copy over
		int i=0;
		for(const auto& p:pts) {
			points[i++]=p;
		}

		initMass();
		initInertia();
	}

	//ro3 2
	Shape(const Shape& s) {
		copyFrom(s);
	}

	//ro3 3
	~Shape() {
		clear();
	}

	Shape& operator=(const Shape& s) {
		//dont remake self
		if(&s==this) return *this;

		//deallocate
		clear();

		//copy over
		copyFrom(s);

		return *this;
	}

	int getNum() const {
		return num_pts;
	}

	//optimize??
	cmn::AABB getAABB() const {
		cmn::AABB box;
		for(int i=0; i<num_pts; i++) {
			box.fitToEnclose(localToWorld(points[i]));
		}
		return box;
	}

	//polygon raycasting algorithm
	bool contains(const vf2d& pt) {
		if(!getAABB().contains(pt)) return false;

		//deterministic, but no artifacts
		vf2d pa=worldToLocal(pt);
		vf2d pb=pa+cmn::polar<vf2d>(1, cmn::randFloat(2*cmn::Pi));

		int num_ix=0;
		for(int i=0; i<num_pts; i++) {
			const auto& a=points[i];
			const auto& b=points[(i+1)%num_pts];
			vf2d tu=cmn::lineLineIntersection(a, b, pa, pb);
			if(tu.x>0&&tu.x<1&&tu.y>0) num_ix++;
		}

		//odd? inside!
		return num_ix%2;
	}

	vf2d localToWorld(const vf2d& p) const {
		//rotation matrix
		vf2d rotated(
			cossin.x*p.x-cossin.y*p.y,
			cossin.y*p.x+cossin.x*p.y
		);
		vf2d translated=pos+rotated;
		return translated;
	}

	//inverse of rotation matrix is its transpose!
	vf2d worldToLocal(const vf2d& p) const {
		//undo transformations...
		vf2d rotated=p-pos;
		vf2d original(
			cossin.x*rotated.x+cossin.y*rotated.y,
			cossin.x*rotated.y-cossin.y*rotated.x
		);
		return original;
	}

#pragma region PHYSICS
	void initMass() {
		//signed area calculation
		mass=0;
		for(int i=0; i<num_pts; i++) {
			const auto& a=points[i];
			const auto& b=points[(i+1)%num_pts];
			mass+=a.x*b.y-b.x*a.y;
		}
		//parallelogram/2=triangle
		mass/=2;

		//ensure points wound consistently
		if(mass<0) {
			std::reverse(points, points+num_pts);
			mass=-mass;
		}

		inv_mass=1/mass;

		//COM of polygon
		vf2d center_of_mass;
		for(int i=0; i<num_pts; i++) {
			const auto& a=points[i];
			const auto& b=points[(i+1)%num_pts];
			float cross=a.x*b.y-b.x*a.y;
			center_of_mass+=cross*(a+b);
		}
		center_of_mass/=6*mass;

		//recenter points about COM=pos
		for(int i=0; i<num_pts; i++) {
			points[i]-=center_of_mass;
		}
		pos=center_of_mass;
		old_pos=pos;
	}

	//MOI of polygon where com=origin
	void initInertia() {
		rotational_inertia=0;
		for(int i=0; i<num_pts; i++) {
			const auto& a=points[i];
			const auto& b=points[(i+1)%num_pts];
			float cross=a.x*b.y-b.x*a.y;
			float x_comp=a.x*a.x+a.x*b.x+b.x*b.x;
			float y_comp=a.y*a.y+a.y*b.y+b.y*b.y;
			rotational_inertia+=cross*(x_comp+y_comp);
		}
		rotational_inertia/=12;

		inv_rotational_inertia=1/rotational_inertia;
	}

	void applyForce(const vf2d& f, const vf2d& p) {
		//update translational forces
		forces+=f;

		//torque based on pivot dist
		vf2d r=p-pos;
		torques+=r.cross(f);
	}

	//percompute for rotation matrix
	void updateRot() {
		cossin=cmn::polar<vf2d>(1, rot);
	}

	void update(float dt) {
		//store vel and update oldpos
		vf2d vel=pos-old_pos;
		old_pos=pos;

		//verlet integration
		vf2d acc=forces/mass;
		if(!locked) pos+=vel+acc*dt*dt;

		//reset forces
		forces={0, 0};

		//store rotvel and update oldrot
		float rot_vel=rot-old_rot;
		old_rot=rot;

		//verlet integration
		float rot_acc=torques/rotational_inertia;
		if(!locked) rot+=rot_vel+rot_acc*dt*dt;
		updateRot();

		//reset torques
		torques=0;
	}

	static void collide(Shape& shp_a, Shape& shp_b, float dt) {
		//separating axis theorem
		float best_overlap=-1;
		vf2d best_axis;
		for(int si=0; si<2; si++) {
			Shape& sa=si==0?shp_a:shp_b;
			Shape& sb=si==0?shp_b:shp_a;
			for(int i=0; i<sa.getNum(); i++) {
				//get axis
				vf2d a=sa.localToWorld(sa.points[i]);
				vf2d b=sa.localToWorld(sa.points[(1+i)%sa.getNum()]);
				vf2d tang=(b-a).norm();
				vf2d axis(tang.y, -tang.x);

				//project a's pts onto axis
				float min_a=0, max_a=0;
				for(int j=0; j<sa.getNum(); j++) {
					float v=axis.dot(sa.localToWorld(sa.points[j]));
					if(j==0||v<min_a) min_a=v;
					if(j==0||v>max_a) max_a=v;
				}

				//project b's pts onto axis
				float min_b=0, max_b=0;
				for(int j=0; j<sb.getNum(); j++) {
					float v=axis.dot(sb.localToWorld(sb.points[j]));
					if(j==0||v<min_b) min_b=v;
					if(j==0||v>max_b) max_b=v;
				}

				//find overlap of ranges
				float overlap=std::min(max_a, max_b)-std::max(min_a, min_b);
				if(overlap<0) return;//separating axis found
				if(best_overlap<0||overlap<best_overlap) {
					best_overlap=overlap;
					best_axis=axis;
				}
			}
		}

		//ensure axis points from a->b
		if(best_axis.dot(shp_b.pos-shp_a.pos)<0) best_axis*=-1;

		//which points of a inside b
		std::vector<vf2d> contacts;
		for(int i=0; i<shp_a.num_pts; i++) {
			vf2d pt=shp_a.localToWorld(shp_a.points[i]);
			if(shp_b.contains(pt)) contacts.push_back(pt);
		}
		//which points of b inside a
		for(int i=0; i<shp_b.num_pts; i++) {
			vf2d pt=shp_b.localToWorld(shp_b.points[i]);
			if(shp_a.contains(pt)) contacts.push_back(pt);
		}

		//update positions
		vf2d mtv=.5f*best_overlap*best_axis;
		if(!shp_a.locked) shp_a.pos-=mtv;
		if(!shp_b.locked) shp_b.pos+=mtv;

		//apply impulses
		vf2d vel_a=(shp_a.pos-shp_a.old_pos)/dt;
		vf2d vel_b=(shp_b.pos-shp_b.old_pos)/dt;
		float rot_vel_a=(shp_a.rot-shp_a.old_rot)/dt;
		float rot_vel_b=(shp_b.rot-shp_b.old_rot)/dt;
		for(const auto& c:contacts) {
			//get relative velocities
			vf2d ra=c-shp_a.pos;
			vf2d rb=c-shp_b.pos;
			vf2d va=vel_a+rot_vel_a*ra;
			vf2d vb=vel_b+rot_vel_b*rb;
			float vrel=best_axis.dot(vb-va);

			//impulse magnitude
			if(vrel<0) {
				const float e=.5f;//restitution
				float raxn=ra.x*best_axis.y-ra.y*best_axis.x;
				float rbxn=rb.x*best_axis.y-rb.y*best_axis.x;
				float denom=shp_a.inv_mass+shp_b.inv_mass+
					shp_a.inv_rotational_inertia*raxn*raxn+
					shp_b.inv_rotational_inertia*rbxn*rbxn;
				float impulse=-(1+e)*vrel/denom;

				//update velocities
				vel_a-=shp_a.inv_mass*impulse*best_axis;
				vel_b+=shp_a.inv_mass*impulse*best_axis;

				//update rotational velocities
				rot_vel_a-=shp_a.inv_rotational_inertia*impulse*raxn;
				rot_vel_b+=shp_b.inv_rotational_inertia*impulse*rbxn;
			}
		}

		//update old_pos
		if(!shp_a.locked) shp_a.old_pos=shp_a.pos-dt*vel_a;
		if(!shp_b.locked) shp_b.old_pos=shp_b.pos-dt*vel_b;

		//update old_rot
		if(!shp_a.locked) shp_a.old_rot=shp_a.rot-dt*rot_vel_a;
		if(!shp_b.locked) shp_b.old_rot=shp_b.rot-dt*rot_vel_b;
	}
#pragma endregion
};

void Shape::copyFrom(const Shape& s) {
	//copy over points
	num_pts=s.num_pts;
	points=new vf2d[num_pts];
	std::memcpy(points, s.points, sizeof(vf2d)*num_pts);

	//copy over transforms and dynamics data
	pos=s.pos;
	old_pos=s.old_pos;
	forces=s.forces;
	mass=s.mass;
	inv_mass=s.inv_mass;

	rot=s.rot;
	old_rot=s.old_rot;
	torques=s.torques;
	cossin=s.cossin;
	rotational_inertia=s.rotational_inertia;
	inv_rotational_inertia=s.inv_rotational_inertia;

	locked=s.locked;

	col=s.col;
}

void Shape::clear() {
	delete[] points;
}
#endif//SHAPE_CLASS_H