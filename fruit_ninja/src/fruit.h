#pragma once
#ifndef FRUIT_CLASS_H
#define FRUIT_CLASS_H

#include "common/aabb.h"
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


class Fruit {
	int num_pts=0;

	void ensureWinding(), copyFrom(const Fruit& f), clear();

public:
	vf2d* pts=nullptr;

	vf2d pos, vel, forces;

	float mass=0;

	float rot=0;
	float rot_vel=0;
	vf2d cossin{1, 0};

	olc::Pixel col=olc::WHITE;

	Fruit() : Fruit(1) {}

	Fruit(int n) : num_pts(n) {
		pts=new vf2d[num_pts];
	}

	void init() {
		ensureWinding();

		mass=getArea();
	}

	//make a circle
	Fruit(vf2d p, float r, int n, olc::Pixel c) : Fruit(n) {
		pos=p;
		for(int i=0; i<num_pts; i++) {
			float angle=2*cmn::Pi*i/num_pts;
			pts[i]=cmn::polar(r, angle);
		}

		col=c;

		init();
	}

	//make a rect
	Fruit(const cmn::AABB& a, olc::Pixel c) : num_pts(4), pos(a.getCenter()), col(c) {
		pts=new vf2d[4];
		pts[0]=a.min;
		pts[1]={a.max.x, a.min.y};
		pts[2]=a.max;
		pts[3]={a.min.x, a.max.y};

		init();
	}

	Fruit(const std::vector<vf2d>& points, olc::Pixel c) : num_pts(points.size()), col(c) {
		pts=new vf2d[num_pts];

		//find bounds of shape and copy over
		cmn::AABB box;
		for(int i=0; i<num_pts; i++) {
			const auto& p=points[i];
			pts[i]=p;
			box.fitToEnclose(p);
		}

		//localize about new ctr
		pos=box.getCenter();
		for(int i=0; i<num_pts; i++) {
			pts[i]-=pos;
		}

		init();
	}

	//1
	Fruit(const Fruit& f) {
		copyFrom(f);
	}

	//2
	~Fruit() {
		clear();
	}

	//3
	Fruit& operator=(const Fruit& f) {
		if(&f==this) return *this;

		clear();

		copyFrom(f);

		return *this;
	}

	int getNum() const { return num_pts; }

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

	float getArea() const {
		float sum=0;
		for(int i=0; i<num_pts; i++) {
			const auto& a=pts[i];
			const auto& b=pts[(i+1)%num_pts];
			sum+=a.cross(b);
		}
		return sum;
	}

	cmn::AABB getAABB() const {
		cmn::AABB box;
		for(int i=0; i<num_pts; i++) {
			box.fitToEnclose(localToWorld(pts[i]));
		}
		return box;
	}

	//polygon raycasting algorithm
	bool contains(vf2d p) const {
		//aabb optimization
		if(!getAABB().contains(p)) return false;

		//localize point
		p=worldToLocal(p);

		//deterministic, but removes artifacting
		float angle=cmn::random(2*cmn::Pi);
		vf2d dir=cmn::polar(1, angle);

		//for every edge...
		int num=0;
		for(int i=0; i<num_pts; i++) {
			const auto& a=pts[i];
			const auto& b=pts[(i+1)%num_pts];
			vf2d tu=cmn::lineLineIntersection(a, b, p, p+dir);
			if(tu.x>0&&tu.x<1&&tu.y>0) num++;
		}

		//odd? inside!
		return num%2;
	}

	//precompute cos and sin
	//this makes things a lot faster
	void updateRot() {
		cossin=cmn::polar(1, rot);
	}

	void applyForce(const vf2d& f) {
		forces+=f;
	}

	void update(float dt) {
		vf2d acc=forces/mass;
		vel+=acc*dt;
		pos+=vel*dt;

		forces={0, 0};

		rot+=rot_vel*dt;

		updateRot();
	}

	bool slice(const std::vector<vf2d>& slice_ref, Fruit& left, Fruit& right) {
		//bounding box optimization
		cmn::AABB slice_box;
		for(const auto& s:slice_ref) {
			slice_box.fitToEnclose(s);
		}
		if(!slice_box.overlaps(getAABB())) return false;

		//copy the sucker
		std::vector<vf2d> slice=slice_ref;

		//find intersections
		int num=0;
		int sixa=-1, fixa=-1; float ta; vf2d ixa;
		int sixb=-1, fixb=-1; float tb; vf2d ixb;
		for(int i=0; i<slice.size()-1; i++) {
			//localize slice points
			const auto& s0=worldToLocal(slice[i]);
			const auto& s1=worldToLocal(slice[i+1]);
			for(int j=0; j<num_pts; j++) {
				//modulo wrap MY points
				const auto& f0=pts[j];
				const auto& f1=pts[(j+1)%num_pts];
				//find segment intersection
				vf2d tu=cmn::lineLineIntersection(f0, f1, s0, s1);
				if(tu.x>0&&tu.x<1&&tu.y>0&&tu.y<1) {
					//find point
					vf2d ix=f0+tu.x*(f1-f0);
					switch(num) {
						//store intersections
						case 0: sixa=i, fixa=j, ta=tu.x, ixa=ix; break;
						case 1: sixb=i, fixb=j, tb=tu.x, ixb=ix; break;
						//too many intersections
						case 2: return false;
					}
					num++;
				}
			}
		}
		//too few intersections
		if(num<2) return false;

		//sort slice along similar segment
		if(fixa==fixb&&ta>tb) {
			//index reorientation
			sixa=slice.size()-2-sixa;
			sixb=slice.size()-2-sixb;
			std::swap(ixa, ixb);
			std::reverse(slice.begin(), slice.end());
		}

		//traverse to find shared portion
		std::vector<vf2d> shared{localToWorld(ixa)};
		for(int i=sixa+1; i<=sixb; i++) {
			//allegedly inbetween intersections:
			//ensure slice doesnt leave shape
			if(!contains(slice[i])) return false;

			shared.emplace_back(slice[i]);
		}
		shared.emplace_back(localToWorld(ixb));

		//traverse and switch outlines
		std::vector<vf2d> left_side;
		std::vector<vf2d> right_side;
		auto to_add=&right_side;
		int i=fixb;
		do {
			//modulo wrapping
			i=(i+1)%num_pts;
			to_add->emplace_back(localToWorld(pts[i]));
			//swap sides if we encounter intersection
			if(i==fixa) {
				to_add=&left_side;
			}
		} while(i!=fixb);
		
		//add shared to right side
		right_side.insert(right_side.end(), shared.begin(), shared.end());
		right=Fruit(right_side, col);

		//add reverse of shared to left side
		std::reverse(shared.begin(), shared.end());
		left_side.insert(left_side.end(), shared.begin(), shared.end());
		left=Fruit(left_side, col);

		return true;
	}
};

//make sure points are ccw
void Fruit::ensureWinding() {
	if(getArea()<0) {
		std::reverse(pts, pts+num_pts);
	}
}

void Fruit::copyFrom(const Fruit& f) {
	//allocate
	num_pts=f.num_pts;
	pts=new vf2d[num_pts];

	//copy over points
	for(int i=0; i<num_pts; i++) {
		pts[i]=f.pts[i];
	}

	//copy over dynamics
	pos=f.pos;
	vel=f.vel;
	forces=f.forces;

	mass=f.mass;

	//copy over "transform" data
	rot=f.rot;
	rot_vel=f.rot_vel;
	cossin=f.cossin;

	//copy over graphics
	col=f.col;
}

void Fruit::clear() {
	delete[] pts;
}
#endif//FRUIT_CLASS_H