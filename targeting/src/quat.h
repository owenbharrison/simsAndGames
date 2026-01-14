#pragma once
#ifndef QUAT_STRUCT_H
#define QUAT_STRUCT_H

#include <cmath>

struct Quat {
	float w=1, x=0, y=0, z=0;

	//quaternion multiplication
	Quat operator*(const Quat& q) const {
		return {
			w*q.w-x*q.x-y*q.y-z*q.z,
			w*q.x+x*q.w+y*q.z-z*q.y,
			w*q.y-x*q.z+y*q.w+z*q.x,
			w*q.z+x*q.y-y*q.x+z*q.w
		};
	}

	Quat conjugate() const {
		return {w, -x, -y, -z};
	}

	Quat normalize() const {
		float mag=std::sqrt(w*w+x*x+y*y+z*z);
		return {w/mag, x/mag, y/mag, z/mag};
	}

	mat4 getMatrix() const {
		mat4 m;
		m(0, 0)=1-2*y*y-2*z*z;
		m(0, 1)=2*x*y-2*z*w;
		m(0, 2)=2*x*z+2*y*w;
		m(1, 0)=2*x*y+2*z*w;
		m(1, 1)=1-2*x*x-2*z*z;
		m(1, 2)=2*y*z-2*x*w;
		m(2, 0)=2*x*z-2*y*w;
		m(2, 1)=2*y*z+2*x*w;
		m(2, 2)=1-2*x*x-2*y*y;
		m(3, 3)=1;
		return m;
	}

	static Quat fromAxisAngle(const vf3d& dir, float theta) {
		float s=std::sin(theta/2);
		return {
			std::cos(theta/2),
			s*dir.x,
			s*dir.y,
			s*dir.z
		};
	}
};

vf3d quatMulVec(const Quat& q, const vf3d& v) {
	Quat p{0, v.x, v.y, v.z};
	//unit inverse=conjugate
	Quat q_inv=q.conjugate();
	Quat result=q*p*q_inv;
	return {result.x, result.y, result.z};
}
#endif