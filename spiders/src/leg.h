//fabrik
//forward & backward reaching inverse kinematics
#pragma once
#ifndef LEG_STRUCT_H
#define LEG_STRUCT_H

#include "cmn/math/v2d.h"

template<int N>
struct Leg {
	const int num=N;
	float len=0;
	cmn::vf2d pts[N];
	cmn::vf2d st, en;

	Leg() {}

	Leg(float l) {
		len=l;
	}

	void fixFrom(cmn::vf2d& c, cmn::vf2d& p) {
		cmn::vf2d sub=c-p;
		cmn::vf2d n{1, 0};
		float d_sq=dot(sub, sub);
		if(d_sq!=0) n=sub/std::sqrt(d_sq);
		c=p+len*n;
	}

	void updateForward() {
		cmn::vf2d* p=&(pts[0]=st);
		for(int i=1; i<num; i++) {
			auto& c=pts[i];
			fixFrom(c, *p);
			p=&c;
		}
	}

	void updateBackward() {
		cmn::vf2d* p=&(pts[num-1]=en);
		for(int i=num-2; i>=1; i--) {
			auto& c=pts[i];
			fixFrom(c, *p);
			p=&c;
		}
	}
};
#endif