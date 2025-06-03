#pragma once
#ifndef QUAT_STRUCT_H
#define QUAT_STRUCT_H

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

	static Quat fromAxisAngle(const vf3d& dir, float theta) {
		float s=std::sinf(theta/2);
		return {
			std::cosf(theta/2),
			s*dir.x,
			s*dir.y,
			s*dir.z
		};
	}

	static cmn::Mat4 toMat4(const Quat& q) {
		cmn::Mat4 m;
		m.v[0][0]=1-2*q.y*q.y-2*q.z*q.z;
		m.v[0][1]=2*q.x*q.y-2*q.z*q.w;
		m.v[0][2]=2*q.x*q.z+2*q.y*q.w;
		m.v[1][0]=2*q.x*q.y+2*q.z*q.w;
		m.v[1][1]=1-2*q.x*q.x-2*q.z*q.z;
		m.v[1][2]=2*q.y*q.z-2*q.x*q.w;
		m.v[2][0]=2*q.x*q.z-2*q.y*q.w;
		m.v[2][1]=2*q.y*q.z+2*q.x*q.w;
		m.v[2][2]=1-2*q.x*q.x-2*q.y*q.y;
		m.v[3][3]=1;
		return m;
	}
};

Quat conjugate(const Quat& q) {
	return {q.w, -q.x, -q.y, -q.z};
}

Quat normalize(const Quat& q) {
	float mag=sqrt(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z);
	return {q.w/mag, q.x/mag, q.y/mag, q.z/mag};
}

vf3d rotateVec(const Quat& q, const vf3d& v) {
	Quat p={0, v.x, v.y, v.z};
	//unit inverse=conjugate
	Quat qInv=conjugate(q);
	Quat result=q*p*qInv;
	return {result.x, result.y, result.z};
}
#endif