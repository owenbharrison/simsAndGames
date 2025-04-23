#pragma once
#ifndef LINE_STRUCT_H
#define LINE_STRUCT_H

struct Line {
	vf3d p[2];
	v2d t[2];
	olc::Pixel col=olc::WHITE;

	bool clipAgainstPlane(const vf3d& ctr, const vf3d& norm, Line& a) const {
		const vf3d* in_pts[2];
		const v2d* in_tex[2];
		int in_ct=0;
		const vf3d* out_pts[2];
		const v2d* out_tex[2];
		int out_ct=0;

		//classify each point based on
		for(int i=0; i<2; i++) {
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
				//line behind plane
				return false;
			case 1: {
				//reconstruct half of it
				a.p[0]=*in_pts[0];
				a.t[0]=*in_tex[0];
				float t=0;
				a.p[1]=segIntersectPlane(*in_pts[0], *out_pts[0], ctr, norm, &t);

				//interpolate tex coords
				a.t[1].u=in_tex[0]->u+t*(out_tex[0]->u-in_tex[0]->u);
				a.t[1].v=in_tex[0]->v+t*(out_tex[0]->v-in_tex[0]->v);
				a.t[1].w=in_tex[0]->w+t*(out_tex[0]->w-in_tex[0]->w);
				a.col=col;

				return true;
			}
			case 2:
				//line infront of plane
				a=*this;
				return true;
		}
	}
};
#endif