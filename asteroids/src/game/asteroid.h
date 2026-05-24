#pragma once
#ifndef ASTEROID_STRUCT_H
#define ASTEROID_STRUCT_H

#include "cmn/geom/aabb2.h"
#include "cmn/utils.h"

#include <string>

cmn::vf2d randomPtOnEdge(const cmn::AABBf2& a) {
	switch(cmn::randInt(0, 3)) {
		//top
		default: return cmn::vf2d(cmn::randFloat(a.min.x, a.max.x), a.min.y);
			//bottom
		case 1: return cmn::vf2d(cmn::randFloat(a.min.x, a.max.x), a.max.y);
			//left
		case 2: return cmn::vf2d(a.min.x, cmn::randFloat(a.min.y, a.max.y));
			//right
		case 3: return cmn::vf2d(a.max.x, cmn::randFloat(a.min.y, a.max.y));
	}
}

class Asteroid {
	cmn::vf2d sincos{0, 1};

	void copyFrom(const Asteroid&), clear();

	void updateRot();

public:
	cmn::vf2d pos, vel;
	float base_rad=0;
	int num_pts=0;
	cmn::vf2d* model=nullptr;

	float rot=0, rot_vel=0;

	Asteroid() {}

	Asteroid(const cmn::vf2d& p, float ra, float ro, int n) {
		pos=p;
		base_rad=ra;
		rot=ro;
		num_pts=n;
		model=new cmn::vf2d[num_pts];
		for(int i=0; i<num_pts; i++) {
			float rad=base_rad*cmn::randFloat(.67f, 1);
			float angle=2*cmn::Pi*i/num_pts;
			model[i]=cmn::vf2d::polar({rad, angle});
		}
		updateRot();
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

	//get bounds
	cmn::AABBf2 getAABB() const {
		const cmn::vf2d inf(1e300, 1e300);
		cmn::AABBf2 a{inf, -inf};
		for(int i=0; i<num_pts; i++) {
			a.fitToEnclose(loc2wld(model[i]));
		}
		return a;
	}

	//is pt inside asteroid?
	bool contains(const cmn::vf2d& pt) const {
		//localize point
		cmn::vf2d a=wld2loc(pt);

		//random direction
		float angle=cmn::randFloat(2*cmn::Pi);
		cmn::vf2d b=a+cmn::vf2d::polar({1.f, angle});

		//count ray-segment intersections
		int num=0;
		for(int i=0; i<num_pts; i++) {
			cmn::vf2d c=model[i];
			cmn::vf2d d=model[(i+1)%num_pts];
			cmn::vf2d tu=cmn::lineLineIntersection(a, b, c, d);
			if(tu.x>0&&tu.y>0&&tu.y<1) num++;
		}

		//odd? inside!
		return num%2;
	}

	//can or should this be split more?
	bool split(Asteroid& a0, Asteroid& a1) const {
		//if asteroid is too small or not enough pts, dont bother splitting it
		if(base_rad<4||num_pts<7) return false;

		cmn::vf2d perp_vel(-vel.y, vel.x);

		//make 2 smaller, faster asteroids
		Asteroid* ast[2]{&a0, &a1};
		for(int i=0; i<2; i++) {
			float vel_scl=cmn::randFloat(1, 1.2f);
			float rad_scl=cmn::randFloat(.6f, .8f);
			float rot_scl=cmn::randFloat(.8f, 1.2f);
			auto& a=*ast[i];
			a=Asteroid(pos, base_rad*rad_scl, rot, num_pts-4);
			int sign=1-2*i;
			a.vel=sign*vel_scl*perp_vel;
			a.rot_vel=sign*rot_scl*rot_vel;
		}

		return true;
	}

	void update(float dt) {
		pos+=vel*dt;

		rot+=rot_vel*dt;

		updateRot();
	}

	//when hit edge spawn on other side: toroidal space?
	void checkAABB(const cmn::AABBf2& a) {
		if(pos.x<a.min.x) pos.x=a.max.x;
		if(pos.y<a.min.y) pos.y=a.max.y;
		if(pos.x>a.max.x) pos.x=a.min.x;
		if(pos.y>a.max.y) pos.y=a.min.y;
	}

	static Asteroid makeRandom(const cmn::AABBf2& box) {
		float rad=cmn::randFloat(5, 8);
		float rot=cmn::randFloat(2*cmn::Pi);
		int num_pts=cmn::randInt(20, 28);
		Asteroid a(randomPtOnEdge(box), rad, rot, num_pts);
		float speed=cmn::randFloat(9, 23);
		float angle=cmn::randFloat(2*cmn::Pi);
		a.vel=cmn::vf2d::polar({speed, angle});
		a.rot_vel=cmn::randFloat(-1, 1);
		return a;
	}
};

void Asteroid::copyFrom(const Asteroid& a) {
	pos=a.pos;
	vel=a.vel;
	base_rad=a.base_rad;
	num_pts=a.num_pts;
	model=new cmn::vf2d[num_pts];
	for(int i=0; i<num_pts; i++) {
		model[i]=a.model[i];
	}
	rot=a.rot;
	sincos=a.sincos;
	rot_vel=a.rot_vel;
}

void Asteroid::clear() {
	delete[] model;
}

//precompute rotation matrix
void Asteroid::updateRot() {
	sincos.x=std::sin(rot);
	sincos.y=std::cos(rot);
}
#endif