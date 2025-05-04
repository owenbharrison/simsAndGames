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
			case 1: {
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
			case 2: {
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

	//https://stackoverflow.com/a/74395029
	vf3d getClosePt(const vf3d& pt) const {
		vf3d ab=p[1]-p[0];
		vf3d ac=p[2]-p[0];

		vf3d ap=pt-p[0];
		float d1=ab.dot(ap);
		float d2=ac.dot(ap);
		if(d1<=0&&d2<=0) return p[0];

		vf3d bp=pt-p[1];
		float d3=ab.dot(bp);
		float d4=ac.dot(bp);
		if(d3>=0&&d4<=d3) return p[1];

		vf3d cp=pt-p[2];
		float d5=ab.dot(cp);
		float d6=ac.dot(cp);
		if(d6>=0&&d5<=d6) return p[2];

		float vc=d1*d4-d3*d2;
		if(vc<=0&&d1>=0&&d3<=0) {
			float v=d1/(d1-d3);
			return p[0]+v*ab;
		}

		float vb=d5*d2-d1*d6;
		if(vb<=0&&d2>=0&&d6<=0) {
			float v=d2/(d2-d6);
			return p[0]+v*ac;
		}

		float va=d3*d6-d5*d4;
		if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0) {
			float v=(d4-d3)/((d4-d3)+(d5-d6));
			return p[1]+v*(p[2]-p[1]);
		}

		float denom=1/(va+vb+vc);
		float v=vb*denom;
		float w=vc*denom;
		return p[0]+v*ab+w*ac;
	}
};

float segIntersectTri(const vf3d& s0, const vf3d& s1, const Triangle& tri, float* uptr=nullptr, float* vptr=nullptr) {
	/*segment equation
	s0+t(s1-s0)=p
	triangle equation
	t0+u(t1-t0)+v(t2-t0)
	set equal
	s0+t(s1-s0)=t0+u(t1-t0)+v(t2-t0)
	rearrange
	t(s1-s0)+u(t0-t1)+v(t0-t2)=t0-s0
	solve like matrix!
	*/
	static const float epsilon=1e-6f;
	//column vectors
	vf3d a=s1-s0;
	vf3d b=tri.p[0]-tri.p[1];
	vf3d c=tri.p[0]-tri.p[2];
	vf3d d=tri.p[0]-s0;
	vf3d bxc=b.cross(c);
	float det=a.dot(bxc);
	//parallel
	if(std::abs(det)<epsilon) return -1;

	//row vectors
	vf3d f=c.cross(a)/det;
	float u=f.dot(d);
	//out of range
	if(u<0||u>1) return -1;

	vf3d g=a.cross(b)/det;
	float v=g.dot(d);
	//out of range
	if(v<0||v>1) return -1;

	//barycentric uv coordinates
	if(u+v>1) return -1;

	vf3d e=bxc/det;
	return e.dot(d);
}
#endif//TRIANGLE_STRUCT_H