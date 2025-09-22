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
	
	float rot=0, rot_vel=0;
	vf2d cossin{1, 0};

	Asteroid() {}

	Asteroid(const vf2d& p, float ra, float ro, int n) {
		pos=p;
		base_rad=ra;
		rot=ro;
		num_pts=n;
		model=new vf2d[num_pts];
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

	//rotation matrix + translation
	vf2d localToWorld(const vf2d& v) const {
		vf2d rotated(
			cossin.x*v.x-cossin.y*v.y,
			cossin.y*v.x+cossin.x*v.y
		);
		return pos+rotated;
	}

	//inverse of rotation matrix = transpose
	vf2d worldToLocal(const vf2d& v) const {
		vf2d rotated=v-pos;
		return {
			cossin.x*rotated.x+cossin.y*rotated.y,
			-cossin.y*rotated.x+cossin.x*rotated.y
		};
	}

	cmn::AABB getAABB() const {
		cmn::AABB a;
		for(int i=0; i<num_pts; i++) {
			a.fitToEnclose(localToWorld(model[i]));
		}
		return a;
	}

	//is pt inside asteroid?
	bool contains(const vf2d& pt) const {
		//aabb optimization
		if(!getAABB().contains(pt)) return false;

		//localize point
		vf2d a=worldToLocal(pt);

		//random direction
		float angle=2*cmn::Pi*cmn::randFloat();
		vf2d b=a+cmn::polar<vf2d>(1, angle);

		//count ray-segment intersections
		int num=0;
		for(int i=0; i<num_pts; i++) {
			vf2d c=model[i];
			vf2d d=model[(i+1)%num_pts];
			vf2d tu=cmn::lineLineIntersection(a, b, c, d);
			if(tu.x>0&&tu.y>0&&tu.y<1) num++;
		}

		//odd? inside!
		return num%2;
	}

	void update(float dt) {
		pos+=vel*dt;

		rot+=rot_vel*dt;

		//precompute rotation matrix
		cossin=cmn::polar<vf2d>(1, rot);
	}

	//toroidal space
	void checkAABB(const cmn::AABB& a) {
		if(pos.x<a.min.x) pos.x=a.max.x;
		if(pos.y<a.min.y) pos.y=a.max.y;
		if(pos.x>a.max.x) pos.x=a.min.x;
		if(pos.y>a.max.y) pos.y=a.min.y;
	}

	//can or should this be split more?
	bool split(Asteroid& a0, Asteroid& a1) const {
		//if asteroid is too small or not enough pts, dont bother splitting it
		if(base_rad<4||num_pts<7) return false;

		vf2d perp_vel(-vel.y, vel.x);

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

	static Asteroid makeRandom(const cmn::AABB& box) {
		float rad=cmn::randFloat(5, 8);
		float rot=2*cmn::Pi*cmn::Pi*cmn::randFloat();
		int num_pts=cmn::randInt(20, 28);
		Asteroid a(randomPtOnEdge(box), rad, rot, num_pts);
		float speed=cmn::randFloat(9, 23);
		float angle=2*cmn::Pi*cmn::randFloat();
		a.vel=cmn::polar<vf2d>(speed, angle);
		a.rot_vel=cmn::randFloat(-1, 1);
		return a;
	}
};

void Asteroid::copyFrom(const Asteroid& a) {
	pos=a.pos;
	vel=a.vel;
	base_rad=a.base_rad;
	num_pts=a.num_pts;
	model=new vf2d[num_pts];
	std::memcpy(model, a.model, sizeof(vf2d)*num_pts);
	rot=a.rot;
	cossin=a.cossin;
	rot_vel=a.rot_vel;
}

void Asteroid::clear() {
	delete[] model;
}
#endif