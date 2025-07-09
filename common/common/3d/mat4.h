#pragma once
#ifndef MAT4_STRUCT_H
#define MAT4_STRUCT_H

//i saw these turn purple so i got scared :3
#undef near
#undef far

#include "v3d.h"

namespace cmn {
	struct Mat4 {
		float v[4][4]={0};

		//compute determinant of sub matrix
		float minor(int row, int col) const {
			float m[3][3];
			int subi=0;
			for(int i=0; i<4; i++) {
				if(i==row) continue;
				int subj=0;
				for(int j=0; j<4; j++) {
					if(j==col) continue;
					m[subi][subj]=v[i][j];
					subj++;
				}
				subi++;
			}
			return
				m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1])-
				m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0])+
				m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]);
		}

		static Mat4 identity() {
			Mat4 m;
			for(int i=0; i<4; i++) {
				m.v[i][i]=1;
			}
			return m;
		}
	
		//rotation in all axes helpers
		static Mat4 makeRotX(float theta) {
			float c=std::cosf(theta), s=std::sinf(theta);
			Mat4 m;
			m.v[0][0]=1;
			m.v[1][1]=c, m.v[1][2]=-s;
			m.v[2][1]=s, m.v[2][2]=c;
			m.v[3][3]=1;
			return m;
		}
		static Mat4 makeRotY(float theta) {
			float c=std::cosf(theta), s=std::sinf(theta);
			Mat4 m;
			m.v[0][0]=c, m.v[0][2]=s;
			m.v[1][1]=1;
			m.v[2][0]=-s, m.v[2][2]=c;
			m.v[3][3]=1;
			return m;
		}
		static Mat4 makeRotZ(float theta) {
			float c=std::cosf(theta), s=std::sinf(theta);
			Mat4 m;
			m.v[0][0]=c, m.v[0][1]=-s;
			m.v[1][0]=s, m.v[1][1]=c;
			m.v[2][2]=1;
			m.v[3][3]=1;
			return m;
		}

		//translation matrix helper
		static Mat4 makeTrans(float x, float y, float z) {
			Mat4 m;
			m.v[0][0]=1;
			m.v[1][1]=1;
			m.v[2][2]=1;
			m.v[3][3]=1;
			m.v[3][0]=x;
			m.v[3][1]=y;
			m.v[3][2]=z;
			return m;
		}

		//scale matrix helper
		static Mat4 makeScale(float x, float y, float z) {
			Mat4 m;
			m.v[0][0]=x;
			m.v[1][1]=y;
			m.v[2][2]=z;
			m.v[3][3]=1;
			return m;
		}

		//projection matrix helper
		static Mat4 makeProj(float fov_deg, float aspect, float near, float far) {
			float fov_rad=fov_deg*3.1415927f/180;
			float inv_tan=1/std::tanf(fov_rad/2);

			Mat4 m;
			m.v[0][0]=aspect*inv_tan;
			m.v[1][1]=inv_tan;
			m.v[2][2]=far/(far-near);
			m.v[3][2]=-far*near/(far-near);
			m.v[2][3]=1;
			return m;
		}

		//model space -> world space
		static Mat4 makePointAt(const vf3d& pos, const vf3d& target, const vf3d& _up) {
			vf3d forward=(target-pos).norm();

			vf3d a=forward*forward.dot(_up);
			vf3d up=(_up-a).norm();

			vf3d right=up.cross(forward);

			Mat4 m;
			m.v[0][0]=right.x, m.v[0][1]=right.y, m.v[0][2]=right.z;
			m.v[1][0]=up.x, m.v[1][1]=up.y, m.v[1][2]=up.z;
			m.v[2][0]=forward.x, m.v[2][1]=forward.y;	m.v[2][2]=forward.z;
			m.v[3][0]=pos.x, m.v[3][1]=pos.y, m.v[3][2]=pos.z;
			m.v[3][3]=1;
			return m;
		}

		//only for rot/proj
		//transpose of rotation, negative rotation&translation.
		static Mat4 quickInverse(const Mat4& a) {
			Mat4 b;
			b.v[0][0]=a.v[0][0], b.v[0][1]=a.v[1][0], b.v[0][2]=a.v[2][0];
			b.v[1][0]=a.v[0][1], b.v[1][1]=a.v[1][1], b.v[1][2]=a.v[2][1];
			b.v[2][0]=a.v[0][2], b.v[2][1]=a.v[1][2], b.v[2][2]=a.v[2][2];
			b.v[3][0]=-(a.v[3][0]*b.v[0][0]+a.v[3][1]*b.v[1][0]+a.v[3][2]*b.v[2][0]);
			b.v[3][1]=-(a.v[3][0]*b.v[0][1]+a.v[3][1]*b.v[1][1]+a.v[3][2]*b.v[2][1]);
			b.v[3][2]=-(a.v[3][0]*b.v[0][2]+a.v[3][1]*b.v[1][2]+a.v[3][2]*b.v[2][2]);
			b.v[3][3]=1;
			return b;
		}

		static Mat4 inverse(const Mat4& m) {
			Mat4 cofactors;

			//get cofactor matrix
			for(int i=0; i<4; i++) {
				for(int j=0; j<4; j++) {
					cofactors.v[i][j]=((i+j)%2?-1:1)*m.minor(i, j);
				}
			}

			//transpose of cofactor
			Mat4 adjugate;
			for(int i=0; i<4; i++) {
				for(int j=0; j<4; j++) {
					adjugate.v[i][j]=cofactors.v[j][i];
				}
			}

			//get determinant using first row and cofactors
			float det=0;
			for(int j=0; j<4; j++) {
				det+=m.v[0][j]*cofactors.v[0][j];
			}

			if(std::abs(det)<1e-6f) {
				throw std::runtime_error("matrix is singular");
			}

			//divide adjugate by determinant
			Mat4 inv;
			for(int i=0; i<4; i++) {
				for(int j=0; j<4; j++) {
					inv.v[i][j]=adjugate.v[i][j]/det;
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
				c.v[i][j]=a.v[i][j]+b.v[i][j];
			}
		}
		return c;
	}

	//matrix negation
	Mat4 operator-(const Mat4& a) {
		Mat4 b;
		for(int i=0; i<4; i++) {
			for(int j=0; j<4; j++) {
				b.v[i][j]=-a.v[i][j];
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
				b.v[i][j]=a.v[i][j]*f;
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
				for(int k=0; k<4; k++) sum+=a.v[i][k]*b.v[k][j];
				c.v[i][j]=sum;
			}
		}
		return c;
	}

	//vector-matrix multiplication
	vf3d operator*(const vf3d& v, const Mat4& m) {
		vf3d r;
		r.x=v.x*m.v[0][0]+v.y*m.v[1][0]+v.z*m.v[2][0]+v.w*m.v[3][0];
		r.y=v.x*m.v[0][1]+v.y*m.v[1][1]+v.z*m.v[2][1]+v.w*m.v[3][1];
		r.z=v.x*m.v[0][2]+v.y*m.v[1][2]+v.z*m.v[2][2]+v.w*m.v[3][2];
		r.w=v.x*m.v[0][3]+v.y*m.v[1][3]+v.z*m.v[2][3]+v.w*m.v[3][3];
		return r;
	}
}
#endif//MAT4_STRUCT_H