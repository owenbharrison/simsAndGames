#pragma once
#ifndef SHAPE_STRUCT_H
#define SHAPE_STRUCT_H

#include "constraint.h"
#include "spring.h"

#include "common/utils.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;

	vf2d polar(float rad, float angle) {
		return polar_generic<vf2d>(rad, angle);
	}

	vf2d lineLineIntersection(
		const vf2d& a, const vf2d& b,
		const vf2d& c, const vf2d& d) {
		return lineLineIntersection_generic(a, b, c, d);
	}
}

vf2d reflect(const vf2d& in, const vf2d& norm) {
	return in-2*norm.dot(in)*norm;
}

class Shape {
	int num_pts=0;
	
	void copyFrom(const Shape&), clear();

	void updateMass();
	void connectConstraints(), connectSprings();

public:
	PointMass* points=nullptr;
	std::list<Constraint> constraints;
	std::list<Spring> springs;

	Shape(vf2d pos, float rad, int n=24) :
		num_pts(n) {

		points=new PointMass[num_pts];
		for(int i=0; i<num_pts; i++) {
			float angle=cmn::map(i, 0, num_pts, 0, 2*cmn::Pi);
			vf2d off=cmn::polar(rad, angle);
			points[i]=PointMass(pos+off);
		}

		updateMass();

		connectConstraints();
		connectSprings();
	}

	Shape(const cmn::AABB& a) : num_pts(4) {
		points=new PointMass[4];
		points[0]=PointMass(a.min);
		points[1]=PointMass({a.max.x, a.min.y});
		points[2]=PointMass(a.max);
		points[3]=PointMass({a.min.x, a.max.y});

		updateMass();

		connectConstraints();
		connectSprings();
	}

	//1
	~Shape() {
		clear();
	}

	//2
	Shape(const Shape& s) {
		copyFrom(s);
	}

	//3
	Shape& operator=(const Shape& s) {
		if(&s==this) return *this;

		clear();

		copyFrom(s);

		return *this;
	}

	int getNum() const {
		return num_pts;
	}

	float getArea() const {
		float sum=0;
		for(int i=0; i<num_pts; i++) {
			const auto& a=points[i].pos;
			const auto& b=points[(i+1)%num_pts].pos;
			sum+=a.cross(b);
		}
		return sum;
	}

	vf2d getCOM() const {
		float m_sum=0;
		vf2d p_sum;
		for(int i=0; i<num_pts; i++) {
			m_sum+=points[i].mass;
			p_sum+=points[i].mass*points[i].pos;
		}
		return p_sum/m_sum;
	}

	cmn::AABB getAABB() const {
		cmn::AABB a;
		for(int i=0; i<num_pts; i++) {
			a.fitToEnclose(points[i].pos);
		}
		return a;
	}

	bool contains(const vf2d& p) const {
		if(!getAABB().contains(p)) return false;

		//deterministic, but avoids edge artifacing
		vf2d dir=cmn::polar(1, cmn::random(2*cmn::Pi));

		int num_ix=0;
		for(int i=0; i<num_pts; i++) {
			const auto& a=points[i].pos;
			const auto& b=points[(i+1)%num_pts].pos;
			vf2d tu=cmn::lineLineIntersection(a, b, p, p+dir);
			if(tu.x>=0&&tu.x<=1&&tu.y>=0) num_ix++;
		}

		//odd?
		return num_ix%2;
	}

	//check point aginst MY polygon
	void collide(PointMass& p) {
		if(!contains(p.pos)) return;

		//find closest segment
		float record;
		int said_j=-1;
		float said_t;
		vf2d said_sub;
		for(int j=0; j<num_pts; j++) {
			const auto& a=points[j].pos;
			const auto& b=points[(j+1)%num_pts].pos;
			vf2d pa=p.pos-a, ba=b-a;
			float t=cmn::clamp(pa.dot(ba)/ba.dot(ba), 0.f, 1.f);
			vf2d close_pt=a+t*ba;
			vf2d sub=close_pt-p.pos;
			float dist=sub.mag();
			if(said_j<0||dist<record) {
				record=dist;
				said_j=j;
				said_t=t;
				said_sub=sub;
			}
		}
		//shouldnt happen
		if(said_j==-1) return;

		//get close segment
		auto& a=points[said_j];
		auto& b=points[(said_j+1)%num_pts];

		//inverse mass weighting
		float m1a=1/a.mass;
		float m1b=1/b.mass;
		float m1p=1/p.mass;

		//find contributions
		float a_cont=(1-said_t)*m1a;
		float b_cont=said_t*m1b;
		float t_cont=a_cont+b_cont+m1p;
		vf2d correction=said_sub/t_cont;

		//update segment
		a.pos-=a_cont*correction;
		b.pos-=b_cont*correction;

		//update particle
		vf2d vel=p.pos-p.oldpos;
		if(!p.locked) {
			p.pos+=m1p*correction;
			//reflect velocity about collision normal
			//p.oldpos=p.pos-reflect(vel, said_sub.norm());
		}
	}

	//check THEIR points against MY polygon
	void collide(Shape& s) {
		//bounding box optimization
		if(!getAABB().overlaps(s.getAABB())) return;

		//for each point
		for(int i=0; i<s.num_pts; i++) {
			collide(s.points[i]);
		}
	}
};

void Shape::copyFrom(const Shape& shp) {
	num_pts=shp.num_pts;
	points=new PointMass[num_pts];
	for(int i=0; i<num_pts; i++) {
		points[i]=shp.points[i];
	}

	//pointer lookup to copy over constraints and springs
	std::unordered_map<PointMass*, PointMass*> shp_to_me;
	for(int i=0; i<num_pts; i++) {
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
	//ensure winding
	float area=getArea();
	if(area<0) {
		area*=-1;
		std::reverse(points, points+num_pts);
	}

	//distribute evenly
	float ind_mass=area/num_pts;
	for(int i=0; i<num_pts; i++) {
		points[i].mass=ind_mass;
	}
}

void Shape::connectConstraints() {
	for(int i=0; i<num_pts; i++) {
		constraints.emplace_back(&points[i], &points[(i+1)%num_pts]);
	}
}

void Shape::connectSprings() {
	for(int i=0; i<num_pts; i++) {
		for(int j=i+2; j<num_pts-(i==0); j++) {
			springs.emplace_back(&points[i], &points[j]);
		}
	}
}
#endif//SHAPE_STRUCT_H