//column major
#pragma once
#ifndef MAT4_STRUCT_H
#define MAT4_STRUCT_H

//for memset
#include <string>

//for trig
#include <cmath>

//not sure what these macros did :3
#undef near
#undef far

#include "v3d.h"

struct mat4 {
	float m[16];

	mat4() { std:memset(m, 0.f, sizeof(float)*16); }

	const float& operator()(int i, int j) const { return m[j+4*i]; }
	float& operator()(int i, int j) { return m[j+4*i]; }

	static mat4 makeIdentity() {
		mat4 c;
		c(0, 0)=1;
		c(1, 1)=1;
		c(2, 2)=1;
		c(3, 3)=1;
		return c;
	}

	static mat4 makeTranslation(const vf3d& trans) {
		mat4 c=mat4::makeIdentity();
		c(3, 0)=trans.x;
		c(3, 1)=trans.y;
		c(3, 2)=trans.z;
		return c;
	}

	static mat4 makeScale(const vf3d& scl) {
		mat4 c;
		c(0, 0)=scl.x;
		c(1, 1)=scl.y;
		c(2, 2)=scl.z;
		c(3, 3)=1;
		return c;
	}

	static mat4 makeRotX(float t) {
		mat4 a;
		float c=std::cos(t), s=std::sin(t);
		a(0, 0)=1;
		a(1, 1)=c;
		a(2, 1)=-s;
		a(1, 2)=s;
		a(2, 2)=c;
		a(3, 3)=1;
		return a;
	}

	static mat4 makeRotY(float t) {
		mat4 a;
		float c=std::cos(t), s=std::sin(t);
		a(0, 0)=c;
		a(2, 0)=s;
		a(1, 1)=1;
		a(0, 2)=-s;
		a(2, 2)=c;
		a(3, 3)=1;
		return a;
	}

	static mat4 makeRotZ(float t) {
		mat4 a;
		float c=std::cos(t), s=std::sin(t);
		a(0, 0)=c;
		a(1, 0)=-s;
		a(0, 1)=s;
		a(1, 1)=c;
		a(2, 2)=1;
		a(3, 3)=1;
		return a;
	}

	//from coordinate system to world.
	//invert this for view matrix.
	static mat4 makeLookAt(const vf3d& eye, const vf3d& target, vf3d y_axis) {
		//coordinate axes from RHR
		vf3d z_axis=(eye-target).norm();
		vf3d x_axis=y_axis.cross(z_axis).norm();
		y_axis=z_axis.cross(x_axis);

		//column vectors + translation
		mat4 a;
		a(0, 0)=x_axis.x, a(1, 0)=y_axis.x, a(2, 0)=z_axis.x, a(3, 0)=eye.x;
		a(0, 1)=x_axis.y, a(1, 1)=y_axis.y, a(2, 1)=z_axis.y, a(3, 1)=eye.y;
		a(0, 2)=x_axis.z, a(1, 2)=y_axis.z, a(2, 2)=z_axis.z, a(3, 2)=eye.z;
		a(3, 3)=1;
		return a;
	}

	static mat4 makeView(const vf3d& eye, const vf3d& target, vf3d up) {
		vf3d fwd=(target-eye).norm();
		vf3d rgt=fwd.cross(up).norm();
		up=rgt.cross(fwd);

		mat4 a;
		a(0, 0)=rgt.x, a(0, 1)=rgt.y, a(0, 2)=rgt.z, a(0, 3)=-eye.dot(rgt);
		a(1, 0)=up.x, a(1, 1)=up.y, a(1, 2)=up.z, a(1, 3)=-eye.dot(up);
		a(2, 0)=-fwd.x, a(2, 1)=-fwd.y, a(2, 2)=-fwd.z, a(2, 3)=eye.dot(fwd);
		a(3, 3)=1;
		return a;
	}

	//this takes degrees.
	//asp=width/height
	//https://gamedev.stackexchange.com/questions/120338
	static mat4 makeProjection(float fov_deg, float asp, float near, float far) {
		float fov_rad=fov_deg/180*3.1415927f;
		float inv_tan=1/std::tan(fov_rad/2);
		float inv_nearfar=1/(near-far);
		mat4 a;
		a(0, 0)=inv_tan/asp;
		a(1, 1)=inv_tan;
		a(2, 2)=(far+near)*inv_nearfar;
		a(3, 2)=2*far*near*inv_nearfar;
		a(2, 3)=-1;
		return a;
	}
};

mat4 mul(const mat4& a, const mat4& b) {
	mat4 c;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
			c(i, j)=
				a(0, j)*b(i, 0)+
				a(1, j)*b(i, 1)+
				a(2, j)*b(i, 2)+
				a(3, j)*b(i, 3);
		}
	}
	return c;
}

mat4 transpose(const mat4& a) {
	mat4 b;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
			b(i, j)=a(j, i);
		}
	}
	return b;
}

//compute determinant of sub matrix
float minor(const mat4& a, int row, int col) {
	float m[9];
	int subi=0;
	for(int i=0; i<4; i++) {
		if(i==row) continue;
		int subj=0;
		for(int j=0; j<4; j++) {
			if(j==col) continue;
			m[subj+3*subi]=a(i, j);
			subj++;
		}
		subi++;
	}
	return
		m[0+3*0]*(m[1+3*1]*m[2+3*2]-m[1+3*2]*m[2+3*1])-
		m[0+3*1]*(m[1+3*0]*m[2+3*2]-m[1+3*2]*m[2+3*0])+
		m[0+3*2]*(m[1+3*0]*m[2+3*1]-m[1+3*1]*m[2+3*0]);
}

mat4 inverse(const mat4& a) {
	mat4 cofactors;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
			cofactors(i, j)=((i+j)%2?-1:1)*minor(a, i, j);
		}
	}
	mat4 adjugate=transpose(cofactors);

	//get determinant using first row and cofactors
	float det=0;
	for(int j=0; j<4; j++) {
		det+=a(0, j)*cofactors(0, j);
	}

	//return identity if singular...
	mat4 inv=mat4::makeIdentity();
	if(det!=0) {
		//divide adjugate by determinant
		float recip=1/det;
		for(int i=0; i<16; i++) inv.m[i]=recip*adjugate.m[i];
	}

	return inv;
}
#endif