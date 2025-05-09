#pragma once
#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

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

	//construction helpers
	void windShell(), connectSprings();

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
			vf2d off=cmn::polar(rad, angle);
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
		auto ix=[&w] (int i, int j) {
			return i+w*j;
		};
		num_pts=w*h;
		points=new PointMass[num_pts];

		//fill point grid
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				vf2d pos=box.min+res*vf2d(i, j);
				points[ix(i, j)]=PointMass(pos);
			}
		}

		//wind shell
		for(int i=1; i<w; i++) {
			shell.emplace_back(&points[ix(i-1, 0)], &points[ix(i, 0)]);
		}
		for(int j=1; j<h; j++) {
			shell.emplace_back(&points[ix(w-1, j-1)], &points[ix(w-1, j)]);
		}
		for(int i=w-1; i>=1; i--) {
			shell.emplace_back(&points[ix(i, h-1)], &points[ix(i-1, h-1)]);
		}
		for(int j=h-1; j>=1; j--) {
			shell.emplace_back(&points[ix(0, j)], &points[ix(0, j-1)]);
		}

		initMass();

		//attach springs
		for(int i=1; i<w; i++) {
			for(int j=1; j<h; j++) {
				//rows
				if(j<h-1) springs.emplace_back(&points[ix(i-1, j)], &points[ix(i, j)]);
				//columns
				if(i<w-1) springs.emplace_back(&points[ix(i, j-1)], &points[ix(i, j)]);
				//boxes
				springs.emplace_back(&points[ix(i-1, j-1)], &points[ix(i, j)]);
				springs.emplace_back(&points[ix(i-1, j)], &points[ix(i, j-1)]);
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

	void initMass() {
		//ensure winding
		float area=getArea();
		if(area<0) {
			area*=-1;
			//reverse each constraint
			for(auto& c:shell) {
				std::swap(c.a, c.b);
			}
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
	void initAnchors() {
		vf2d ctr=getCOM();
		for(int i=0; i<num_pts; i++) {
			anchors[i]=points[i].pos-ctr;
		}
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
		rot=std::atan2f(cross, dot);
	}

	bool contains(const vf2d& p) const {
		if(!getAABB().contains(p)) return false;

		//deterministic, but avoids edge artifacing
		vf2d dir=cmn::polar(1, cmn::random(2*cmn::Pi));

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

	void update(float dt) {
		if(anchored) {
			//rotation matrix
			vf2d cosin=cmn::polar(1, anchor_rot);
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
				s_temp.rest_len=0;
				s_temp.update();
			}
		} else {
			//store when NOT anchored
			getOrientation(anchor_pos, anchor_rot);
		}

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

	bool cut(const std::vector<vf2d>& cut_ref, Shape& left, Shape& right);
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

void Shape::copyFrom(const Shape& shp) {
	num_pts=shp.num_pts;

	//copy points
	points=new PointMass[num_pts];
	for(int i=0; i<num_pts; i++) {
		points[i]=shp.points[i];
	}

	//copy anchors
	anchors=new vf2d[num_pts];
	for(int i=0; i<num_pts; i++) {
		anchors[i]=shp.anchors[i];
	}
	anchor_pos=shp.anchor_pos;
	anchor_rot=shp.anchor_rot;
	anchored=shp.anchored;

	//copy colors
	col=shp.col;

	//pointer lookup to copy over constraints and springs
	std::unordered_map<PointMass*, PointMass*> shp_to_me;
	for(int i=0; i<num_pts; i++) {
		//shp_to_me[shp]=me
		shp_to_me[&shp.points[i]]=&points[i];
	}

	//copy over shell
	for(const auto& cst:shp.shell) {
		Constraint c;
		c.a=shp_to_me[cst.a];
		c.b=shp_to_me[cst.b];
		c.rest_len=cst.rest_len;
		shell.push_back(c);
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

	delete[] anchors;

	shell.clear();
	springs.clear();
}
#endif//SHAPE_STRUCT_H