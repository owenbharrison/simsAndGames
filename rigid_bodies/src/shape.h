#pragma once
#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

#include "common/utils.h"
#include "common/aabb.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

#include <list>
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
	Shape(const std::list<vf2d>& pts) {
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
		vf2d pb=pa+cmn::polar<vf2d>(1, cmn::random(2*cmn::Pi));

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

	static void collide(Shape& sa, Shape& sb, float dt) {
		//combine both shapes normals into axes list
		std::list<vf2d> axes;
		for(int i=0; i<sa.num_pts; i++) {
			vf2d a=sa.localToWorld(sa.points[i]);
			vf2d b=sa.localToWorld(sa.points[(1+i)%sa.num_pts]);
			vf2d tang=(b-a).norm();
			vf2d norm(tang.y, -tang.x);
			axes.push_back(norm);
		}
		for(int i=0; i<sb.num_pts; i++) {
			vf2d a=sb.localToWorld(sb.points[i]);
			vf2d b=sb.localToWorld(sb.points[(1+i)%sb.num_pts]);
			vf2d tang=(b-a).norm();
			vf2d norm(tang.y, -tang.x);
			axes.push_back(norm);
		}

		//for each axis
		float collide_dist=-1;
		vf2d collide_norm;
		vf2d collide_pt;
		for(const auto& n:axes) {
			//project a's pts
			float min_a=0, max_a=0;
			int min_a_ix=0;
			for(int i=0; i<sa.num_pts; i++) {
				float v=n.dot(sa.localToWorld(sa.points[i]));
				if(i==0||v<min_a) min_a=v, min_a_ix=i;
				if(i==0||v>max_a) max_a=v;
			}
			//project b's pts
			float min_b=0, max_b=0;
			int min_b_ix=0;
			for(int i=0; i<sb.num_pts; i++) {
				float v=n.dot(sb.localToWorld(sb.points[i]));
				if(i==0||v<min_b) min_b=v, min_b_ix=i;
				if(i==0||v>max_b) max_b=v;
			}

			//are ranges overlapping?
			float dab=max_a-min_b;
			if(dab<0) return;
			float dba=max_b-min_a;
			if(dba<0) return;

			//which side
			float d=0;
			vf2d pt;
			if(dab<dba) d=dab, pt=sb.localToWorld(sb.points[min_b_ix]);
			else d=dba, pt=sa.localToWorld(sa.points[min_a_ix]);

			//get min overlap
			if(collide_dist<0||d<collide_dist) {
				collide_dist=d;
				collide_norm=n;
				collide_pt=pt;
			}
		}

		//make sure norm pushes apart
		if(collide_norm.dot(sa.pos-sb.pos)<0) collide_norm*=-1;
		vf2d diff=.5f*collide_dist*collide_norm;
		
		//update positions
		if(!sa.locked) sa.pos+=diff;
		if(!sb.locked) sb.pos-=diff;

		//get relative velocities
		vf2d ra=collide_pt-sa.pos;
		vf2d rb=collide_pt-sb.pos;
		vf2d vel_a=(sa.pos-sa.old_pos)/dt;
		vf2d vel_b=(sb.pos-sb.old_pos)/dt;
		float rot_vel_a=(sa.rot-sa.old_rot)/dt;
		float rot_vel_b=(sb.rot-sb.old_rot)/dt;
		vf2d va=vel_a+rot_vel_a*ra;
		vf2d vb=vel_b+rot_vel_b*rb;
		float vrel=collide_norm.dot(vb-va);
		
		//impulse magnitude
		if(vrel<0) {
			const float e=.5f;//restitution
			float raxn=ra.x*collide_norm.y-ra.y*collide_norm.x;
			float rbxn=rb.x*collide_norm.y-rb.y*collide_norm.x;
			float denom=sa.inv_mass+sb.inv_mass+
				sa.inv_rotational_inertia*raxn*raxn+
				sb.inv_rotational_inertia*rbxn*rbxn;
			float impulse=-(1+e)*vrel/denom;

			//update velocities
			vel_a-=sa.inv_mass*impulse*collide_norm;
			vel_b-=sa.inv_mass*impulse*collide_norm;
			//update old_pos
			if(!sa.locked) sa.old_pos=sa.pos-dt*vel_a;
			if(!sb.locked) sb.old_pos=sb.pos-dt*vel_b;

			//update rotational velocities
			rot_vel_a-=sa.inv_rotational_inertia*impulse*raxn;
			rot_vel_b-=sb.inv_rotational_inertia*impulse*rbxn;
			//update old_rot
			if(!sa.locked) sa.old_rot=sa.rot-dt*rot_vel_a;
			if(!sb.locked) sb.old_rot=sb.rot-dt*rot_vel_b;
		}
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