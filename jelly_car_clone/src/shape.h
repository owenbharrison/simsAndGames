//todo: add anchors
#pragma once
#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

//didnt think i'd have to include this...
#include "common/olcPixelGameEngine.h"

#include "point_mass.h"

#include <list>

#include "constraint.h"
#include "spring.h"

#include <unordered_map>

constexpr float Pi=3.1415927f;

olc::vf2d polar(float angle, float rad=1) {
	return rad*olc::vf2d(std::cosf(angle), std::sinf(angle));
}

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
	size_t num_pts=0;
	Shape()=delete;

	void copyFrom(const Shape& s), clear();

	void updateMass();
	void connectConstraints(), connectSprings();

public:
	PointMass* points=nullptr;
	std::list<Constraint> constraints;
	std::list<Spring> springs;

	Shape(const olc::vf2d& pos, float rad, size_t n=24) {
		num_pts=n;
		points=new PointMass[num_pts];
		for(size_t i=0; i<num_pts; i++) {
			float angle=2*Pi*i/num_pts;
			points[i]=PointMass(pos+polar(angle, rad), 1);
		}

		updateMass();
		connectConstraints();
		connectSprings();
	}

	Shape(const std::list<olc::vf2d>& pts) {
		num_pts=pts.size();
		points=new PointMass[num_pts];
		size_t i=0;
		for(const auto& p:pts) {
			points[i++]=PointMass(p, 1);
		}

		updateMass();
		connectConstraints();
		connectSprings();
	}

	//1
	Shape(const Shape& s) {
		copyFrom(s);
	}

	//2
	~Shape() {
		clear();
	}

	//3
	Shape& operator=(const Shape& s) {
		if(&s==this) return *this;

		clear();

		copyFrom(s);

		return *this;
	}

	size_t getNum() const {
		return num_pts;
	}

	float getArea() const {
		float sum=0;
		for(size_t i=0; i<num_pts; i++) {
			const auto& a=points[i].pos;
			const auto& b=points[(i+1)%num_pts].pos;
			sum+=a.x*b.y-a.y*b.x;
		}
		//triangle = parallelogram / 2
		return sum/2;
	}

	AABB getAABB() const {
		AABB box;
		for(size_t i=0; i<num_pts; i++) {
			box.fitToEnclose(points[i].pos);
		}
		return box;
	}

	olc::vf2d getCOM() const {
		//all mass=
		olc::vf2d sum;
		for(size_t i=0; i<num_pts; i++) {
			sum+=points[i].pos;
		}
		return sum/num_pts;
	}

	float getMass() const {
		//all mass=
		return points[0].mass*num_pts;
	}

	//polygon ray intersection
	bool contains(const olc::vf2d& p) const {
		if(!getAABB().contains(p)) return false;

		olc::vf2d dir=polar(random(0, 2*Pi));

		size_t num_ix=0;
		for(size_t i=0; i<num_pts; i++) {
			const olc::vf2d a=points[i].pos, b=points[(i+1)%num_pts].pos;
			olc::vf2d tu=lineLineIntersection(a, b, p, p+dir);
			if(tu.x>=0&&tu.x<=1&&tu.y>=0) num_ix++;
		}
		return num_ix%2;
	}

	void update(float dt) {
		//update springs
		for(auto& s:springs) {
			s.update();
		}

		//update constraints
		for(auto& c:constraints) {
			c.update();
		}

		//update points
		for(size_t i=0; i<num_pts; i++) {
			points[i].update(dt);
		}
	}
};

void Shape::copyFrom(const Shape& shp) {
	num_pts=shp.num_pts;
	points=new PointMass[num_pts];
	for(size_t i=0; i<num_pts; i++) {
		points[i]=shp.points[i];
	}

	//pointer lookup to copy over constraints and springs
	std::unordered_map<PointMass*, PointMass*> shp_to_me;
	for(size_t i=0; i<num_pts; i++) {
		//shp_to_me[shp]=me
		shp_to_me[&shp.points[i]]=&points[i];
	}

	//copy over constraints
	for(const auto& cst:shp.constraints) {
		Constraint c;
		c.a=shp_to_me[cst.a];
		c.b=shp_to_me[cst.b];
		c.rest_len=cst.rest_len;
		constraints.push_back(c);
	}

	//copy over springs
	for(const auto& spr:shp.springs) {
		Spring s;
		s.a=shp_to_me[spr.a];
		s.b=shp_to_me[spr.b];
		s.rest_len=spr.rest_len;
		s.stiffness=spr.stiffness;
		s.damping=spr.damping;
		springs.push_back(s);
	}
}

void Shape::clear() {
	delete[] points;
	constraints.clear();
	springs.clear();
}

void Shape::updateMass() {
	float area=getArea();

	//ensure CW winding
	if(area<0) {
		area*=-1;
		std::reverse(points, points+num_pts);
	}

	//distribute mass evenly
	float ind_mass=area/num_pts;
	for(size_t i=0; i<num_pts; i++) {
		points[i].mass=ind_mass;
	}
}

void Shape::connectConstraints() {
	constraints.clear();

	//connect like a shell
	for(size_t i=0; i<num_pts; i++) {
		auto& a=points[i];
		auto& b=points[(i+1)%num_pts];
		constraints.emplace_back(&a, &b);
	}
}

void Shape::connectSprings() {
	springs.clear();

	//connect to all consequtive(except 0 -> N-1)
	for(size_t i=0; i<num_pts; i++) {
		for(size_t j=i+2; j<num_pts-(i==0); j++) {
			springs.emplace_back(&points[i], &points[j], 1, 1);
		}
	}
}
#endif//SHAPE_CLASS_H