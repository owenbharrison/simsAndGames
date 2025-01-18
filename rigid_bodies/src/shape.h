#pragma once
#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

constexpr float Pi=3.1415927f;

vf2d polar(float rad, float angle) {
	return {rad*std::cosf(angle), rad*std::sinf(angle)};
}

#include "aabb.h"

//clever default param placement:
//random()=0-1
//random(a)=0-a
//random(a, b)=a-b
float random(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=rand()/rand_max;
	return a+t*(b-a);
}

//thanks wikipedia + pattern recognition
//this returns (t, u), NOT the point
vf2d lineLineIntersection(
	const vf2d& a, const vf2d& b,
	const vf2d& c, const vf2d& d) {
	vf2d ab=a-b, ac=a-c, cd=c-d;
	return vf2d(
		ac.cross(cd),
		ac.cross(ab)
	)/ab.cross(cd);
}

class Shape {
	int num_pts;

	void copyFrom(const Shape&), clear();

public:
	vf2d* points=nullptr;
	vf2d pos, old_pos, forces;
	float mass=1;
	
	float rot=0, old_rot=0, torques=0;
	vf2d cossin{1, 0};
	float moment_of_inertia=1;

	Shape() : Shape({0, 0}, 0, 1) {}

	Shape(vf2d ctr, float rad, int n) {
		//allocate
		num_pts=n;
		points=new vf2d[num_pts];
		
		//make n-gon
		for(int i=0; i<num_pts; i++) {
			float angle=2*Pi*i/num_pts;
			points[i]=ctr+polar(rad, angle);
		}

		initMass();
		initInertia();
	}

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

	Shape(const Shape& s) {
		copyFrom(s);
	}

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
	AABB getAABB() const {
		AABB box;
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
		vf2d pb=pa+polar(1, random(2*Pi));

		int num_ix=0;
		for(int i=0; i<num_pts; i++) {
			const auto& a=points[i];
			const auto& b=points[(i+1)%num_pts];
			vf2d tu=lineLineIntersection(a, b, pa, pb);
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
		moment_of_inertia=0;
		for(int i=0; i<num_pts; i++) {
			const auto& a=points[i];
			const auto& b=points[(i+1)%num_pts];
			float cross=a.x*b.y-b.x*a.y;
			float x_comp=a.x*a.x+a.x*b.x+b.x*b.x;
			float y_comp=a.y*a.y+a.y*b.y+b.y*b.y;
			moment_of_inertia+=cross*(x_comp+y_comp);
		}
		moment_of_inertia/=12;
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
		cossin=polar(1, rot);
	}

	void update(float dt) {
	//store vel and update oldpos
		vf2d vel=pos-old_pos;
		old_pos=pos;

		//verlet integration
		vf2d acc=forces/mass;
		pos+=vel+acc*dt*dt;

		//reset forces
		forces={0, 0};

		//store rotvel and update oldrot
		float rot_vel=rot-old_rot;
		old_rot=rot;

		//verlet integration
		float rot_acc=torques/moment_of_inertia;
		rot+=rot_vel+rot_acc*dt*dt;
		updateRot();

		//reset torques
		torques=0;
	}
#pragma endregion
};

void Shape::copyFrom(const Shape& s) {
	//copy over points
	num_pts=s.num_pts;
	points=new vf2d[num_pts];
	for(int i=0; i<num_pts; i++) {
		points[i]=s.points[i];
	}

	//copy over transforms and dynamics data
	pos=s.pos;
	old_pos=s.old_pos;

	rot=s.rot;
	old_rot=s.old_rot;
}

void Shape::clear() {
	delete[] points;
}
#endif//SHAPE_CLASS_H