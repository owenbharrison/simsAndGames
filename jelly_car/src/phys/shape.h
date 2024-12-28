#pragma once
#ifndef SHAPE_STRUCT_H
#define SHAPE_STRUCT_H

#include "../geom/aabb.h"
#include "point_mass.h"
#include "constraint.h"
#include "spring.h"

static constexpr float Pi=3.1415927f;

float map(float x, float a, float b, float c, float d) {
	float t=(x-a)/(b-a);
	return c+t*(d-c);
}

float random(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=rand()/rand_max;
	return a+t*(b-a);
}

float clamp(float x, float a, float b) {
	if(x<a) return a;
	if(x>b) return b;
	return x;
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

vf2d reflect(const vf2d& in, const vf2d& norm) {
	return in-2*norm.dot(in)*norm;
}

struct Shape {
	int num_pts=0;
	PointMass* points=nullptr;
	std::list<Constraint> constraints;
	std::list<Spring> springs;

	Shape()=delete;

	void connectConstraints() {
		constraints.clear();
		for(int i=0; i<num_pts; i++) {
			constraints.emplace_back(&points[i], &points[(i+1)%num_pts]);
		}
	}

	void connectSprings() {
		springs.clear();
		for(int i=0; i<num_pts; i++) {
			for(int j=i+2; j<num_pts-(i==0); j++) {
				springs.emplace_back(&points[i], &points[j]);
			}
		}
	}

	void ensureWinding() {
		if(getArea()>0) return;

		//reverse points
		for(int i=0; i<num_pts/2; i++) {
			std::swap(points[i], points[num_pts-1-i]);
		}
	}

	Shape(olc::vf2d pos, float rad, int n=24) :
		num_pts(n) {
		const float t_mass=Pi*rad*rad;
		float ind_mass=t_mass/num_pts;

		points=new PointMass[num_pts];
		for(int i=0; i<num_pts; i++) {
			float angle=map(i, 0, num_pts, 0, 2*Pi);
			vf2d dir(cosf(angle), sinf(angle));
			points[i]=PointMass(pos+rad*dir, ind_mass);
		}

		ensureWinding();

		connectConstraints();
		connectSprings();
	}

	Shape(const AABB& a) :
		num_pts(4) {
		olc::vf2d sz=a.max-a.min;
		const float t_mass=sz.x*sz.y;
		float ind_mass=t_mass/4;

		points=new PointMass[4];
		points[0]=PointMass(a.min, ind_mass);
		points[1]=PointMass({a.max.x, a.min.y}, ind_mass);
		points[2]=PointMass(a.max, ind_mass);
		points[3]=PointMass({a.min.x, a.max.y}, ind_mass);

		ensureWinding();

		connectConstraints();
		connectSprings();
	}

	void clear() {
		delete[] points;

		constraints.clear();
		springs.clear();
	}

	//1
	~Shape() {
		clear();
	}

	void copyFrom(const Shape& s) {
		num_pts=s.num_pts;
		points=new PointMass[num_pts];
		for(int i=0; i<num_pts; i++) {
			points[i]=s.points[i];
		}

		//should i be copying connectors?
		connectConstraints();
		connectSprings();
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

	vf2d getCOM() const {
		float m_sum=0;
		vf2d p_sum;
		for(int i=0; i<num_pts; i++) {
			m_sum+=points[i].mass;
			p_sum+=points[i].mass*points[i].pos;
		}
		return p_sum/m_sum;
	}

	AABB getAABB() const {
		AABB a;
		for(int i=0; i<num_pts; i++) {
			a.fitToEnclose(points[i].pos);
		}
		return a;
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

	bool contains(const vf2d& c, bool opt=true) const {
		if(opt&&!getAABB().contains(c)) return false;

		//deterministic, but avoids edge artifacing
		float angle=random(0, 2*Pi);
		vf2d d=c+vf2d(cosf(angle), sinf(angle));

		int num_ix=0;
		for(int i=0; i<num_pts; i++) {
			const auto& a=points[i].pos;
			const auto& b=points[(i+1)%num_pts].pos;
			vf2d tu=lineLineIntersection(a, b, c, d);
			if(tu.x>=0&&tu.x<=1&&tu.y>=0) num_ix++;
		}

		//odd?
		return num_ix%2;
	}

	//update MY points, update THEIR edges
	void collide_exact(Shape& s) {
		//bounding box optimization
		AABB s_box=s.getAABB();
		if(!s_box.overlaps(getAABB())) return;

		//for each of my points
		for(int i=0; i<num_pts; i++) {
			//is it inside s?
			auto& p=points[i];
			vf2d p_vel=p.pos-p.oldpos;
			if(!s_box.contains(p.pos)) continue;

			//find closest edge
			float record=INFINITY;
			int idx=-1;
			for(int j=0; j<s.num_pts; j++) {
				const auto& a=s.points[j].pos;
				const auto& b=s.points[(j+1)%s.num_pts].pos;

				vf2d pa=p.pos-a, ba=b-a;
				float t=clamp(pa.dot(ba)/ba.dot(ba), 0, 1);
				vf2d close_pt=a+t*ba;
				float dist=(close_pt-p.pos).mag2();
				if(dist<record) {
					record=dist;
					idx=j;
				}
			}
			//shouldnt happen
			if(idx==-1) continue;

			//this is not optimized.
			auto& a=s.points[idx];
			auto& b=s.points[(idx+1)%s.num_pts];
			vf2d pa=p.pos-a.pos, ba=b.pos-a.pos;
			float t=clamp(pa.dot(ba)/ba.dot(ba), 0, 1);

			//find mass of section of segment
			float e_mass=a.mass+t*(b.mass-a.mass);
			//p movement weighted by invmass
			float pct_p=e_mass/(e_mass+p.mass);

			//given p's change
			//map a and b based on ratios
			vf2d norm=-ba.norm().perp();
			vf2d close_pt=a.pos+t*ba;
			float d=norm.dot(p.pos-close_pt);
			float d_p=pct_p*d;
			if(!p.locked) p.pos+=d_p*norm;
			float denom=2*t*t-2*t+1;

			float d_a=d*(1-pct_p)*(1-t)/denom;
			if(!a.locked) a.pos-=d_a*norm;
			float d_b=d*(1-pct_p)*t/denom;
			if(!b.locked) b.pos-=d_b*norm;

			//only update p vel for now
			p.oldpos=p.pos-reflect(p_vel, norm);
		}
	}

	void collide_ish(Shape& s) {
		//bounding box optimization
		if(!getAABB().overlaps(s.getAABB())) return;

		//check all of my points
		for(int i=0; i<num_pts; i++) {
			auto& p=points[i];
			if(!s.contains(p.pos)) continue;

			//find closest segment
			float record=INFINITY;
			int idx=-1;
			for(int j=0; j<s.num_pts; j++) {
				const auto& a=s.points[j].pos;
				const auto& b=s.points[(j+1)%num_pts].pos;
				vf2d pa=p.pos-a, ba=b-a;
				float t=clamp(pa.dot(ba)/ba.dot(ba), 0, 1);
				vf2d close_pt=a+t*ba;
				float dist=(close_pt-p.pos).mag2();
				if(dist<record) {
					record=dist;
					idx=j;
				}
			}
			//shouldnt happen
			if(idx==-1) continue;

			vf2d p_vel=p.pos-p.oldpos;

			//this is NOT optimized
			auto& a=s.points[idx];
			auto& b=s.points[(idx+1)%num_pts];
			vf2d pa=p.pos-a.pos, ba=b.pos-a.pos;
			float t=clamp(pa.dot(ba)/ba.dot(ba), 0, 1);
			vf2d close_pt=a.pos+t*ba;

			//determine individual contributions
			float p_cont=p.mass;
			float a_cont=(1-t)*a.mass;
			float b_cont=t*b.mass;
			float t_cont=p_cont+a_cont+b_cont;

			//update p
			if(!p.locked) {
				const float alpha=.005f;//heuristic?
				vf2d p_delta=close_pt-p.pos;
				p.pos+=alpha*(1-p_cont/t_cont)*p_delta;
				//update velocity by updating oldpos
				p.oldpos=p.pos-reflect(p_vel, p_delta.norm());
			}

			//update edge
			const float beta=.002f;
			vf2d ab_delta=-ba.perp();
			if(!a.locked) a.pos-=beta*(1-a_cont/t_cont)*ab_delta;
			if(!b.locked) b.pos-=beta*(1-b_cont/t_cont)*ab_delta;

			//update a,b oldpos
		}
	}
};
#endif//SHAPE_STRUCT_H