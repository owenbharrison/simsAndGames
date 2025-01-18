#pragma once
#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

constexpr float Pi=3.1415927f;

vf2d polar(float rad, float angle) {
	return {rad*std::cosf(angle), rad*std::sinf(angle)};
}

#include "aabb.h"

class Shape {
	int num_pts;

	void copyFrom(const Shape&), clear();
	void ensureWinding();

public:

	vf2d* points=nullptr;
	vf2d pos, old_pos;
	
	float rot=0, old_rot=0;
	vf2d cossin{1, 0};

	float mass=1;

	Shape() : Shape({0, 0}, 0, 1) {}

	Shape(vf2d ctr, float rad, int n) {
		pos=ctr;

		//allocate
		num_pts=n;
		points=new vf2d[num_pts];
		
		//make n-gon
		for(int i=0; i<num_pts; i++) {
			float angle=2*Pi*i/num_pts;
			points[i]=polar(rad, angle);
		}

		ensureWinding();
		mass=getArea();
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

	float getArea() const {
		float sum=0;
		for(int i=0; i<num_pts; i++) {
			const auto& a=points[i];
			const auto& b=points[(i+1)%num_pts];
			sum+=a.cross(b);
		}
		//parallelograms/2=triangles
		return sum/2;
	}

	//optimize??
	AABB getAABB() const {
		AABB box;
		for(int i=0; i<num_pts; i++) {
			box.fitToEnclose(localToWorld(points[i]));
		}
		return box;
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

//ensure shape is wound consistently
void Shape::ensureWinding() {
	if(getArea()<0) {
		std::reverse(points, points+num_pts);
	}
}
#endif//SHAPE_CLASS_H