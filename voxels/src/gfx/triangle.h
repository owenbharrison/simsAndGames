#pragma once
#ifndef TRIANGLE_STRUCT_H
#define TRIANGLE_STRUCT_H

#include "../geom/aabb3.h"
#include "../math/v2d.h"

//i only want to expose the t value some of the time
vf3d segIntersectPlane(const vf3d& a, const vf3d& b, const vf3d& ctr, const vf3d& norm, float* tp=nullptr) {
	float t=norm.dot(ctr-a)/norm.dot(b-a);
	if(tp) *tp=t;
	return a+t*(b-a);
}

struct Triangle {
	vf3d p[3];
	v2d t[3];
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

	int clipAgainstPlane(const vf3d& ctr, const vf3d& norm, Triangle& a, Triangle& b) const {
		const vf3d* in_pts[3];
		const v2d* in_tex[3];
		int in_ct=0;
		const vf3d* out_pts[3];
		const v2d* out_tex[3];
		int out_ct=0;

		//classify each point based on
		for(int i=0; i<3; i++) {
			//what side of plane it is on
			if(norm.dot(p[i]-ctr)>0) {
				in_pts[in_ct]=&p[i];
				in_tex[in_ct]=&t[i];
				in_ct++;
			} else {
				out_pts[out_ct]=&p[i];
				out_tex[out_ct]=&t[i];
				out_ct++;
			}
		}

		switch(in_ct) {
			default:
				//tri behind plane
				return 0;
			case 1:
			{
				//form tri
				a.p[0]=*in_pts[0];
				a.t[0]=*in_tex[0];

				float t=0;
				a.p[1]=segIntersectPlane(*in_pts[0], *out_pts[0], ctr, norm, &t);
				//interpolate tex coords
				a.t[1].u=in_tex[0]->u+t*(out_tex[0]->u-in_tex[0]->u);
				a.t[1].v=in_tex[0]->v+t*(out_tex[0]->v-in_tex[0]->v);
				a.t[1].w=in_tex[0]->w+t*(out_tex[0]->w-in_tex[0]->w);

				t=0;
				a.p[2]=segIntersectPlane(*in_pts[0], *out_pts[1], ctr, norm, &t);
				//interpolate tex coords
				a.t[2].u=in_tex[0]->u+t*(out_tex[1]->u-in_tex[0]->u);
				a.t[2].v=in_tex[0]->v+t*(out_tex[1]->v-in_tex[0]->v);
				a.t[2].w=in_tex[0]->w+t*(out_tex[1]->w-in_tex[0]->w);

				a.col=col;
				return 1;
			}
			case 2:
			{
				//form quad
				a.p[0]=*in_pts[0];
				a.t[0]=*in_tex[0];

				a.p[1]=*in_pts[1];
				a.t[1]=*in_tex[1];

				float t=0;
				a.p[2]=segIntersectPlane(*in_pts[1], *out_pts[0], ctr, norm, &t);
				//interpolate tex coords
				a.t[2].u=in_tex[1]->u+t*(out_tex[0]->u-in_tex[1]->u);
				a.t[2].v=in_tex[1]->v+t*(out_tex[0]->v-in_tex[1]->v);
				a.t[2].w=in_tex[1]->w+t*(out_tex[0]->w-in_tex[1]->w);

				a.col=col;

				b.p[0]=a.p[0];
				b.t[0]=a.t[0];

				b.p[1]=a.p[2];
				b.t[1]=a.t[2];

				t=0;
				b.p[2]=segIntersectPlane(*out_pts[0], a.p[0], ctr, norm, &t);
				//interpolate tex coords
				b.t[2].u=out_tex[0]->u+t*(a.t[0].u-out_tex[0]->u);
				b.t[2].v=out_tex[0]->v+t*(a.t[0].v-out_tex[0]->v);
				b.t[2].w=out_tex[0]->w+t*(a.t[0].w-out_tex[0]->w);

				b.col=col;
				return 2;
			}
			case 3:
				//tri infront of plane
				a=*this;
				return 1;
		}
	}
};
#endif//TRIANGLE_STRUCT_H