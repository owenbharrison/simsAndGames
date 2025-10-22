#pragma once
#ifndef MAT4_STRUCT_H
#define MAT4_STRUCT_H

//i saw these turn purple so i got scared :3
#undef near
#undef far

#include "v3d.h"

//for memset
#include <string>

//for trig
#include <cmath>

namespace cmn {
	struct Mat4 {
		float v[16];

		Mat4() {
			std::memset(v, 0.f, sizeof(float)*16);
		}

		float& operator()(int i, int j) {
			return v[i+4*j];
		}

		const float& operator()(int i, int j) const {
			return v[i+4*j];
		}

		//compute determinant of sub matrix
		float minor(int row, int col) const {
			float m[9];
			int subi=0;
			for(int i=0; i<4; i++) {
				if(i==row) continue;
				int subj=0;
				for(int j=0; j<4; j++) {
					if(j==col) continue;
					m[subi+3*subj]=operator()(i, j);
					subj++;
				}
				subi++;
			} 
			return
				m[0+3*0]*(m[1+3*1]*m[2+3*2]-m[1+3*2]*m[2+3*1])-
				m[0+3*1]*(m[1+3*0]*m[2+3*2]-m[1+3*2]*m[2+3*0])+
				m[0+3*2]*(m[1+3*0]*m[2+3*1]-m[1+3*1]*m[2+3*0]);
		}

		static Mat4 makeIdentity() {
			Mat4 m;
			for(int i=0; i<4; i++) {
				m(i, i)=1;
			}
			return m;
		}
	
		//rotation in all axes helpers
		static Mat4 makeRotX(float theta) {
			float c=std::cos(theta), s=std::sin(theta);
			Mat4 m;
			m(0, 0)=1;
			m(1, 1)=c, m(1, 2)=-s;
			m(2, 1)=s, m(2, 2)=c;
			m(3, 3)=1;
			return m;
		}
		static Mat4 makeRotY(float theta) {
			float c=std::cos(theta), s=std::sin(theta);
			Mat4 m;
			m(0, 0)=c, m(0, 2)=s;
			m(1, 1)=1;
			m(2, 0)=-s, m(2, 2)=c;
			m(3, 3)=1;
			return m;
		}
		static Mat4 makeRotZ(float theta) {
			float c=std::cos(theta), s=std::sin(theta);
			Mat4 m;
			m(0, 0)=c, m(0, 1)=-s;
			m(1, 0)=s, m(1, 1)=c;
			m(2, 2)=1;
			m(3, 3)=1;
			return m;
		}

		//translation matrix helper
		static Mat4 makeTrans(float x, float y, float z) {
			Mat4 m;
			m(0, 0)=1;
			m(1, 1)=1;
			m(2, 2)=1;
			m(3, 3)=1;
			m(3, 0)=x;
			m(3, 1)=y;
			m(3, 2)=z;
			return m;
		}

		//scale matrix helper
		static Mat4 makeScale(float x, float y, float z) {
			Mat4 m;
			m(0, 0)=x;
			m(1, 1)=y;
			m(2, 2)=z;
			m(3, 3)=1;
			return m;
		}

		//projection matrix helper
		static Mat4 makeProjection(float fov_deg, float aspect, float near, float far) {
			float fov_rad=fov_deg/180*3.1415927f;
			float inv_tan=1/std::tan(fov_rad/2);

			Mat4 m;
			m(0, 0)=aspect*inv_tan;
			m(1, 1)=inv_tan;
			m(2, 2)=far/(far-near);
			m(3, 2)=-far*near/(far-near);
			m(2, 3)=1;
			return m;
		}

		//model space -> world space
		static Mat4 makePointAt(const vf3d& pos, const vf3d& target, vf3d up) {
			vf3d fwd=(target-pos).norm();
			vf3d rgt=up.cross(fwd).norm();
			up=fwd.cross(rgt);

			Mat4 m;
			m(0, 0)=rgt.x, m(0, 1)=rgt.y, m(0, 2)=rgt.z;
			m(1, 0)=up.x, m(1, 1)=up.y, m(1, 2)=up.z;
			m(2, 0)=fwd.x, m(2, 1)=fwd.y;	m(2, 2)=fwd.z;
			m(3, 0)=pos.x, m(3, 1)=pos.y, m(3, 2)=pos.z;
			m(3, 3)=1;
			return m;
		}

		//only for rot/proj
		//transpose of rotation, negative rotation&translation.
		static Mat4 quickInverse(const Mat4& a) {
			Mat4 b;
			b(0, 0)=a(0, 0), b(0, 1)=a(1, 0), b(0, 2)=a(2, 0);
			b(1, 0)=a(0, 1), b(1, 1)=a(1, 1), b(1, 2)=a(2, 1);
			b(2, 0)=a(0, 2), b(2, 1)=a(1, 2), b(2, 2)=a(2, 2);
			b(3, 0)=-(a(3, 0)*b(0, 0)+a(3, 1)*b(1, 0)+a(3, 2)*b(2, 0));
			b(3, 1)=-(a(3, 0)*b(0, 1)+a(3, 1)*b(1, 1)+a(3, 2)*b(2, 1));
			b(3, 2)=-(a(3, 0)*b(0, 2)+a(3, 1)*b(1, 2)+a(3, 2)*b(2, 2));
			b(3, 3)=1;
			return b;
		}

		static Mat4 inverse(const Mat4& m) {
			//get cofactor matrix
			Mat4 cofactors;
			for(int i=0; i<4; i++) {
				for(int j=0; j<4; j++) {
					cofactors(i, j)=((i+j)%2?-1:1)*m.minor(i, j);
				}
			}

			//get determinant using first row and cofactors
			float det=0;
			for(int j=0; j<4; j++) {
				det+=m(0, j)*cofactors(0, j);
			}

			//default inv to identity.
			Mat4 inv=Mat4::makeIdentity();
			if(det!=0) {
				//adjugate=transpose of cofactors
				//divide adjugate by det
				float inv_det=1/det;
				for(int i=0; i<4; i++) {
					for(int j=0; j<4; j++) {
						inv(i, j)=inv_det*cofactors(j, i);
					}
				}
			}

			return inv;
		}
	};

	//matrix-matrix addition
	Mat4 operator+(const Mat4& a, const Mat4& b) {
		Mat4 c;
		for(int i=0; i<4; i++) {
			for(int j=0; j<4; j++) {
				c(i, j)=a(i, j)+b(i, j);
			}
		}
		return c;
	}

	//matrix negation
	Mat4 operator-(const Mat4& a) {
		Mat4 b;
		for(int i=0; i<4; i++) {
			for(int j=0; j<4; j++) {
				b(i, j)=-a(i, j);
			}
		}
		return b;
	}

	//matrix-matrix subtraction
	Mat4 operator-(const Mat4& a, const Mat4& b) {
		return a+(-b);
	}

	//matrix float multiplication
	Mat4 operator*(const Mat4& a, float f) {
		Mat4 b;
		for(int i=0; i<4; i++) {
			for(int j=0; j<4; j++) {
				b(i, j)=a(i, j)*f;
			}
		}
		return b;
	}

	//matrix-matrix multiplication
	Mat4 operator*(const Mat4& a, const Mat4& b) {
		Mat4 c;
		for(int i=0; i<4; i++) {
			for(int j=0; j<4; j++) {
				float sum=0;
				for(int k=0; k<4; k++) sum+=a(i, k)*b(k, j);
				c(i, j)=sum;
			}
		}
		return c;
	}

	//vector-matrix multiplication
	vf3d operator*(const vf3d& v, const Mat4& m) {
		vf3d r;
		r.x=v.x*m(0, 0)+v.y*m(1, 0)+v.z*m(2, 0)+v.w*m(3, 0);
		r.y=v.x*m(0, 1)+v.y*m(1, 1)+v.z*m(2, 1)+v.w*m(3, 1);
		r.z=v.x*m(0, 2)+v.y*m(1, 2)+v.z*m(2, 2)+v.w*m(3, 2);
		r.w=v.x*m(0, 3)+v.y*m(1, 3)+v.z*m(2, 3)+v.w*m(3, 3);
		return r;
	}
}
#endif//MAT4_STRUCT_H