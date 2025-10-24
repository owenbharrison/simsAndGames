#pragma once
#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

#include "constraint.h"
#include "spring.h"

#include <list>

//for reverse
#include <algorithm>

//for memcpy
#include <string>

#include "common/utils.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

vf2d reflect(const vf2d& in, const vf2d& norm) {
	return in-2*norm.dot(in)*norm;
}

class Shape {
	int num_pts=0;

	//construction helpers
	void windShell(), connectSprings();

	void initMass(), initAnchors();

	//copy helpers
	void copyFrom(const Shape&), clear();

public:
	PointMass* points=nullptr;
	std::list<Constraint> shell;
	std::list<Spring> springs;

	//shape matching stuff
	vf2d* anchors=nullptr;
	vf2d anchor_pos;
	float anchor_rot=0;
	bool anchored=false;

	olc::Pixel col=olc::WHITE;

#pragma region CONSTRUCTION
	Shape() {}

	Shape(int n) {
		num_pts=n;
		points=new PointMass[num_pts];
		anchors=new vf2d[num_pts];
	}

	Shape(vf2d pos, float rad, int n=24) :
		num_pts(n) {

		points=new PointMass[num_pts];
		for(int i=0; i<num_pts; i++) {
			float angle=cmn::map(i, 0, num_pts, 0, 2*cmn::Pi);
			vf2d off=cmn::polar<vf2d>(rad, angle);
			points[i]=PointMass(pos+off);
		}

		windShell();

		initMass();

		anchors=new vf2d[num_pts];
		initAnchors();

		connectSprings();
	}

	Shape(const cmn::AABB& box, float res) {
		//determine sizing
		const int w=1+(box.max.x-box.min.x)/res;
		const int h=1+(box.max.y-box.min.y)/res;
		num_pts=w*h;
		
		//allocate
		points=new PointMass[num_pts];
		
		auto ix=[&w] (int i, int j) { return i+w*j; };

		//fill point grid
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				vf2d pos=box.min+res*vf2d(i, j);
				points[ix(i, j)]=PointMass(pos);
			}
		}

		//wind shell
		for(int i=1; i<w; i++) {
			shell.push_back(Constraint(&points[ix(i-1, 0)], &points[ix(i, 0)]));
		}
		for(int j=1; j<h; j++) {
			shell.push_back(Constraint(&points[ix(w-1, j-1)], &points[ix(w-1, j)]));
		}
		for(int i=w-1; i>=1; i--) {
			shell.push_back(Constraint(&points[ix(i, h-1)], &points[ix(i-1, h-1)]));
		}
		for(int j=h-1; j>=1; j--) {
			shell.push_back(Constraint(&points[ix(0, j)], &points[ix(0, j-1)]));
		}

		initMass();

		//attach springs
		for(int i=1; i<w; i++) {
			for(int j=1; j<h; j++) {
				//horizontal
				if(j<h-1) springs.push_back(Spring(&points[ix(i-1, j)], &points[ix(i, j)]));
				//vertical
				if(i<w-1) springs.push_back(Spring(&points[ix(i, j-1)], &points[ix(i, j)]));
				//diagonals
				springs.push_back(Spring(&points[ix(i-1, j-1)], &points[ix(i, j)]));
				springs.push_back(Spring(&points[ix(i-1, j)], &points[ix(i, j-1)]));
			}
		}

		//proportional to perim/area
		float coeff=2.f*num_pts/shell.size();
		for(auto& s:springs) {
			s.stiffness*=coeff;
			s.damping*=coeff;
		}

		anchors=new vf2d[num_pts];
		initAnchors();
	}

	//ro3: 1
	~Shape() {
		clear();
	}

	//ro3: 2
	Shape(const Shape& s) {
		copyFrom(s);
	}

	//ro3: 3
	Shape& operator=(const Shape& s) {
		if(&s==this) return *this;

		clear();

		copyFrom(s);

		return *this;
	}
#pragma endregion

	int getNum() const {
		return num_pts;
	}

	//requires shell be initialized
	float getArea() const {
		float sum=0;
		for(const auto& c:shell) {
			const auto& a=c.a->pos, & b=c.b->pos;
			sum+=a.x*b.y-a.y*b.x;
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

	void getOrientation(vf2d& pos, float& rot) {
		//pos=center of mass
		pos=getCOM();

		//rot=avg angle between anchor and current points
		float dot=0, cross=0;
		for(int i=0; i<num_pts; i++) {
			const auto& anc=anchors[i];
			vf2d sub=points[i].pos-pos;

			//accumulate direction
			dot+=anc.x*sub.x+anc.y*sub.y;
			cross+=anc.x*sub.y-anc.y*sub.x;
		}

		//signed angle
		rot=std::atan2(cross, dot);
	}

	bool contains(const vf2d& p) const {
		if(!getAABB().contains(p)) return false;

		//deterministic, but avoids edge artifacing
		vf2d dir=cmn::polar<vf2d>(1, cmn::randFloat(2*cmn::Pi));

		int num_ix=0;
		for(const auto& c:shell) {
			vf2d tu=cmn::lineLineIntersection(c.a->pos, c.b->pos, p, p+dir);
			if(tu.x>=0&&tu.x<=1&&tu.y>=0) num_ix++;
		}

		//odd?
		return num_ix%2;
	}

	//apply same "force" to all points
	//i.e. gravity or bouyancy
	void accelerate(const vf2d& f) {
		for(int i=0; i<num_pts; i++) {
			points[i].accelerate(f);
		}
	}

	void updateAnchors(float dt) {
		//rotation matrix
		vf2d cosin=cmn::polar<vf2d>(1, anchor_rot);
		auto rotate=[cosin] (const vf2d& p) {
			return vf2d(
				p.x*cosin.x-p.y*cosin.y,
				p.x*cosin.y+p.y*cosin.x
			);
		};

		//spring toward anchors
		for(int i=0; i<num_pts; i++) {
			const auto& p=points[i];
			//bring anchor from local->world
			PointMass p_temp(anchor_pos+rotate(anchors[i]), p.mass);
			Spring s_temp(&points[i], &p_temp);
			s_temp.len_rest=0;
			s_temp.update();
		}
	}

	void update(float dt) {
		if(!anchored) getOrientation(anchor_pos, anchor_rot);
		else updateAnchors(dt);

		//update all constraints
		for(auto& c:shell) {
			c.update();
		}

		//update all springs
		for(auto& s:springs) {
			s.update();
		}

		//update all particles
		for(int i=0; i<num_pts; i++) {
			points[i].update(dt);
		}
	}

	void keepIn(const cmn::AABB& a) {
		for(int i=0; i<num_pts; i++) {
			points[i].keepIn(a);
		}
	}

	//check point aginst MY polygon
	void collide(PointMass& p) {
		if(!contains(p.pos)) return;

		//find closest segment
		float record;
		Constraint* said_seg=nullptr;
		float said_t;
		vf2d said_sub;
		for(auto& c:shell) {
			vf2d pa=p.pos-c.a->pos, ba=c.b->pos-c.a->pos;
			float t=cmn::clamp(pa.dot(ba)/ba.dot(ba), 0.f, 1.f);
			vf2d close_pt=c.a->pos+t*ba;
			vf2d sub=close_pt-p.pos;
			float dist=sub.mag();
			if(!said_seg||dist<record) {
				record=dist;
				said_seg=&c;
				said_t=t;
				said_sub=sub;
			}
		}
		//shouldnt happen
		if(!said_seg) return;

		auto& a=*said_seg->a;
		auto& b=*said_seg->b;

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

void Shape::windShell() {
	shell.clear();
	for(int i=0; i<num_pts; i++) {
		shell.push_back(Constraint(&points[i], &points[(i+1)%num_pts]));
	}
}

void Shape::connectSprings() {
	springs.clear();
	for(int i=0; i<num_pts; i++) {
		for(int j=i+2; j<num_pts-(i==0); j++) {
			springs.push_back(Spring(&points[i], &points[j]));
		}
	}
}

void Shape::initMass() {
	//ensure winding
	float area=getArea();
	if(area<0) {
		area*=-1;
		//reverse each constraint
		for(auto& c:shell) std::swap(c.a, c.b);
		//reverse shell
		std::reverse(shell.begin(), shell.end());
	}

	//distribute mass evenly
	float ind_mass=area/num_pts;
	for(int i=0; i<num_pts; i++) {
		points[i].mass=ind_mass;
	}
}

//localize anchors
void Shape::initAnchors() {
	vf2d ctr=getCOM();
	for(int i=0; i<num_pts; i++) {
		anchors[i]=points[i].pos-ctr;
	}
}

void Shape::copyFrom(const Shape& shp) {
	num_pts=shp.num_pts;

	//copy points
	points=new PointMass[num_pts];
	std::memcpy(points, shp.points, sizeof(PointMass)*num_pts);

	//copy anchors
	anchors=new vf2d[num_pts];
	std::memcpy(anchors, shp.anchors, sizeof(vf2d)*num_pts);
	anchor_pos=shp.anchor_pos;
	anchor_rot=shp.anchor_rot;
	anchored=shp.anchored;

	//copy color
	col=shp.col;

	//copy over shell
	for(const auto& shp_c:shp.shell) {
		Constraint c;
		//relative pointer arithmetic
		c.a=points+(shp_c.a-shp.points);
		c.b=points+(shp_c.b-shp.points);
		c.len_rest=shp_c.len_rest;
		shell.push_back(c);
	}

	//copy over springs
	for(const auto& shp_s:shp.springs) {
		Spring s;
		//relative pointer arithmetic
		s.a=points+(shp_s.a-shp.points);
		s.b=points+(shp_s.b-shp.points);
		s.len_rest=shp_s.len_rest;
		s.stiffness=shp_s.stiffness;
		s.damping=shp_s.damping;
		springs.push_back(s);
	}
}

void Shape::clear() {
	delete[] points;

	delete[] anchors;

	shell.clear();
	springs.clear();
}
#endif//SHAPE_STRUCT_H