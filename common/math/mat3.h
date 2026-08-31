#pragma once
#ifndef CMN_MAT3_STRUCT_H
#define CMN_MAT3_STRUCT_H

//for memset
#include <string>

//for sqrt & trig
#include <cmath>

#include "v2d.h"

namespace cmn {
	struct mat3 {
		//column major
		float m[9];

		mat3() { std::memset(m, 0.f, sizeof(float)*9); }

		//row, col
		const float& operator()(int r, int c) const { return m[r+3*c]; }
		float& operator()(int r, int c) { return m[r+3*c]; }

		static mat3 makeIdentity() {
			mat3 c;
			c(0, 0)=1;
			c(1, 1)=1;
			c(2, 2)=1;
			c(3, 3)=1;
			return c;
		}

		//transform order is right to left: A then B = mul(B, A)
		static mat3 mul(const mat3& lhs, const mat3& rhs) {
			mat3 res;
			//basically a lot of corresponding dot products...
			for(int r=0; r<3; r++) {
				for(int c=0; c<3; c++) {
					res(r, c)=
						lhs(r, 0)*rhs(0, c)+
						lhs(r, 1)*rhs(1, c)+
						lhs(r, 2)*rhs(2, c);
				}
			}
			return res;
		}

		//thank you wolframalpha!
		static mat3 inverse(const mat3& a) {
			const float& m00=a(0, 0), & m01=a(0, 1), & m02=a(0, 2);
			const float& m10=a(1, 0), & m11=a(1, 1), & m12=a(1, 2);
			const float& m20=a(2, 0), & m21=a(2, 1), & m22=a(2, 2);
			float denom=
				m02*(m10*m21-m11*m20)+
				m01*(m12*m20-m10*m22)+
				m00*(m11*m22-m12*m21);
			if(denom==0) return makeIdentity();

			float recip=1/denom;
			mat3 m;
			m(0, 0)=recip*(m11*m22-m12*m21);
			m(0, 1)=recip*(m02*m21-m01*m22);
			m(0, 2)=recip*(m01*m12-m02*m11);
			m(1, 0)=recip*(m12*m20-m10*m22);
			m(1, 1)=recip*(m00*m22-m02*m20);
			m(1, 2)=recip*(m02*m10-m00*m12);
			m(2, 0)=recip*(m10*m21-m11*m20);
			m(2, 1)=recip*(m01*m20-m00*m21);
			m(2, 2)=recip*(m00*m11-m01*m10);
			return m;
		}

		static mat3 makeTranslation(const vf2d& t) {
			mat3 c=mat3::makeIdentity();
			c(0, 2)=t.x;
			c(1, 2)=t.y;
			return c;
		}

		static mat3 makeScale(const vf2d& s) {
			mat3 c;
			c(0, 0)=s.x;
			c(1, 1)=s.y;
			c(2, 2)=1;
			return c;
		}

		static mat3 makeRot(float t) {
			mat3 a;
			float c=std::cos(t), s=std::sin(t);
			a(0, 0)=c, a(0, 1)=-s;
			a(1, 0)=s, a(1, 1)=c;
			a(2, 2)=1;
			return a;
		}
	};

	//mat3 * vec3 implies homogeneous coordinates
	//but im not going to use the vec3 struct for this.
	vf2d matMulVec(const mat3& m, const vf2d& v, float& w) {
		float x=m(0, 0)*v.x+m(0, 1)*v.y+m(0, 2)*w;
		float y=m(1, 0)*v.x+m(1, 1)*v.y+m(1, 2)*w;
		w=m(2, 0)*v.x+m(2, 1)*v.y+m(2, 2)*w;

		return {x, y};
	}
}
#endif