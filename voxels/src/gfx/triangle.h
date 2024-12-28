#pragma once
#ifndef TRIANGLE_STRUCT_H
#define TRIANGLE_STRUCT_H

#include "../geom/aabb3.h"

vf3d segIntersectPlane(const vf3d& a, const vf3d& b, const vf3d& ctr, const vf3d& norm) {
	float t=norm.dot(ctr-a)/norm.dot(b-a);
	return a+t*(b-a);
}

struct Triangle {
	vf3d p[3];
	olc::Pixel col=olc::WHITE;

	vf3d getCtr() const {
		return (p[0]+p[1]+p[2])/3;
	}

	vf3d getNorm() const {
		vf3d e1=p[1]-p[0];
		vf3d e2=p[2]-p[0];
		return e1.cross(e2).norm();
	}

	AABB3 getAABB() const {
		AABB3 box;
		box.fitToEnclose(p[0]);
		box.fitToEnclose(p[1]);
		box.fitToEnclose(p[2]);
		return box;
	}
};

#endif//TRIANGLE_STRUCT_H