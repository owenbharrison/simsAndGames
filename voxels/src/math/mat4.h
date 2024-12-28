#pragma once
#ifndef MAT4_STRUCT_H
#define MAT4_STRUCT_H

//i saw these turn purple so i got scared :3
#undef near
#undef far

constexpr float Pi=3.1415927f;

struct Mat4 {
	float v[4][4]={0};

	static Mat4 identity() {
		Mat4 m;
		for(int i=0; i<4; i++) {
			m.v[i][i]=1;
		}
		return m;
	}

	static Mat4 makeRotX(float theta) {
		Mat4 m;
		m.v[0][0]=std::cosf(theta);
		m.v[0][1]=std::sinf(theta);
		m.v[1][0]=-std::sinf(theta);
		m.v[1][1]=std::cosf(theta);
		m.v[2][2]=1;
		m.v[3][3]=1;
		return m;
	}
	static Mat4 makeRotY(float theta) {
		Mat4 m;
		m.v[0][0]=std::cosf(theta);
		m.v[0][2]=std::sinf(theta);
		m.v[2][0]=-std::sinf(theta);
		m.v[1][1]=1;
		m.v[2][2]=std::cosf(theta);
		m.v[3][3]=1;
		return m;
	}
	static Mat4 makeRotZ(float theta) {
		Mat4 m;
		m.v[0][0]=1;
		m.v[1][1]=std::cosf(.5f*theta);
		m.v[1][2]=std::sinf(.5f*theta);
		m.v[2][1]=-std::sinf(.5f*theta);
		m.v[2][2]=std::cosf(.5f*theta);
		m.v[3][3]=1;
		return m;
	}

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

	static Mat4 makeProj(float fov_deg, float aspect, float near, float far) {
		float fov_rad=1/std::tanf(fov_deg/2/180*Pi);

		Mat4 m;
		m.v[0][0]=aspect*fov_rad;
		m.v[1][1]=fov_rad;
		m.v[2][2]=far/(far-near);
		m.v[3][2]=-far*near/(far-near);
		m.v[2][3]=1;
		return m;
	}

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
};
#endif//MAT4_STRUCT_H