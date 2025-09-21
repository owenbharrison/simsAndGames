#pragma once
#ifndef ASTEROID_STRUCT_H
#define ASTEROID_STRUCT_H

#include <string>

vf2d randomPtOnEdge(const cmn::AABB& a) {
	switch(cmn::randInt(0, 3)) {
		//top
		default: return vf2d(cmn::randFloat(a.min.x, a.max.x), a.min.y);
		//bottom
		case 1: return vf2d(cmn::randFloat(a.min.x, a.max.x), a.max.y);
		//left
		case 2: return vf2d(a.min.x, cmn::randFloat(a.min.y, a.max.y));
		//right
		case 3: return vf2d(a.max.x, cmn::randFloat(a.min.y, a.max.y));
	}
}

class Asteroid {
	void copyFrom(const Asteroid&), clear();

public:
	vf2d pos, vel;
	float base_rad=0;
	int num_pts=0;
	vf2d* model=nullptr;
	vf2d* points=nullptr;

	Asteroid() {}

	Asteroid(const vf2d& p, const vf2d& v, float r, int n) {
		pos=p;
		vel=v;
		base_rad=r;
		num_pts=n;
		model=new vf2d[num_pts];
		points=new vf2d[num_pts];
		for(int i=0; i<num_pts; i++) {
			float rad=base_rad*cmn::randFloat(.67f, 1);
			float angle=2*cmn::Pi*i/num_pts;
			model[i]=cmn::polar<vf2d>(rad, angle);
		}
	}

	//ro3 1
	Asteroid(const Asteroid& a) {
		copyFrom(a);
	}

	//ro3 2
	~Asteroid() {
		clear();
	}

	//ro3 3
	Asteroid& operator=(const Asteroid& a) {
		//avoid resetting self...
		if(&a!=this) {
			clear();

			copyFrom(a);
		}

		return *this;
	}

	void update(float dt) {
		pos+=vel*dt;

		for(int i=0; i<num_pts; i++) {
			points[i]=pos+model[i];
		}
	}

	//toroidal space
	void checkAABB(const cmn::AABB& a) {
		if(pos.x<a.min.x) pos.x=a.max.x;
		if(pos.y<a.min.y) pos.y=a.max.y;
		if(pos.x>a.max.x) pos.x=a.min.x;
		if(pos.y>a.max.y) pos.y=a.min.y;
	}

	//can or should this be split more?
	bool split(Asteroid& a, Asteroid& b) const {
		//if asteroid is too small or not enough pts, dont bother splitting it
		if(base_rad<4||num_pts<5) return false;

		float spd_a=cmn::randFloat(0.75f, 1), spd_b=cmn::randFloat(0.75f, 1);
		float rad_a=cmn::randFloat(0.5f, 0.8f), rad_b=cmn::randFloat(0.5f, 0.8f);
		a=Asteroid(pos, spd_a*vf2d(-vel.y, vel.x), base_rad*rad_a, num_pts-2);
		b=Asteroid(pos, spd_b*vf2d(vel.y, -vel.x), base_rad*rad_b, num_pts-2);
		return true;
	}

	cmn::AABB getAABB() const {
		cmn::AABB a;
		for(int i=0; i<num_pts; i++) {
			a.fitToEnclose(points[i]);
		}
		return a;
	}

	//is pt inside asteroid?
	bool contains(const vf2d& pt) const {
		//aabb optimization
		if(!getAABB().contains(pt)) return false;

		//random direction
		vf2d a=pt;
		float angle=2*cmn::Pi*cmn::randFloat();
		vf2d dir=cmn::polar<vf2d>(1, angle);
		vf2d b=a+dir;

		//count ray-segment intersections
		int num=0;
		for(int i=0; i<num_pts; i++) {
			vf2d c=points[i];
			vf2d d=points[(i+1)%num_pts];
			vf2d tu=cmn::lineLineIntersection(a, b, c, d);
			if(tu.x>0&&tu.y>0&&tu.y<1) num++;
		}

		//odd? inside!
		return num%2;
	}

	static Asteroid makeRandom(const cmn::AABB& a) {
		float speed=cmn::randFloat(15, 27);
		float angle=2*cmn::Pi*cmn::randFloat();
		float rad=cmn::randFloat(5, 8);
		int num_pts=cmn::randFloat(20, 28);
		return Asteroid(randomPtOnEdge(a), cmn::polar<vf2d>(speed, angle), rad, num_pts);
	}
};

void Asteroid::copyFrom(const Asteroid& a) {
	pos=a.pos;
	vel=a.vel;
	base_rad=a.base_rad;
	num_pts=a.num_pts;
	model=new vf2d[num_pts];
	std::memcpy(model, a.model, sizeof(vf2d)*num_pts);
	points=new vf2d[num_pts];
	std::memcpy(points, a.points, sizeof(vf2d)*num_pts);
}

void Asteroid::clear() {
	delete[] model;
	delete[] points;
}
#endif