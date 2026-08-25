#pragma once
#ifndef SPIDER_STRUCT_H
#define SPIDER_STRUCT_H

#include "leg.h"

template<int N>
struct LeggedWalker {
	static_assert(N%2==0, "# legs must be even");
	
	float length=0;
	float width=0;

	cmn::vf2d pos;
	float rot=0;
	cmn::vf2d sc{0, 1};

	const int num=N;
	Leg<3> legs[N];
	
	LeggedWalker() {}

	LeggedWalker(float l, float w, float ll) {
		length=l;
		width=w;
		for(int i=0; i<num; i++) {
			legs[i].len=ll;
		}
	}

	cmn::vf2d rotVec(const cmn::vf2d& p) const {
		return {p.x*sc.y-p.y*sc.x, p.x*sc.x+p.y*sc.y};
	}

	cmn::vf2d unrotVec(const cmn::vf2d& p) const {
		return {p.x*sc.y+p.y*sc.x, -p.x*sc.x+p.y*sc.y};
	}

	cmn::vf2d loc2wld(const cmn::vf2d& l) const {
		return pos+rotVec(l);
	}

	cmn::vf2d wld2loc(const cmn::vf2d& w) const {
		return unrotVec(w-pos);
	}

	//precompute sin & cos for rotation
	void updateMatrix() {
		sc.x=std::sin(rot);
		sc.y=std::cos(rot);
	}
	
	void update() {
		updateMatrix();
		
		//update leg start and end positions
		const float recip=1.f/(num/2-1);
		const float rad=.75f*length*recip;
		for(int i=0; i<num/2; i++) {
			auto& r=legs[0+2*i];
			auto& l=legs[1+2*i];
			float l01=i*recip-.5f;
			float sx=l01*length;
			float sy=.5f*width;
			r.st=loc2wld({sx, sy});
			l.st=loc2wld({sx, -sy});
			
			//walk legs if too far
			float ex=sx;
			float ey=30+sy;//outside of rect
			cmn::vf2d er=loc2wld({ex, ey});
			if(cmn::length(er-r.en)>rad) r.en=er;
			cmn::vf2d el=loc2wld({ex, -ey});
			if(cmn::length(el-l.en)>rad) l.en=el;
		}

		for(int i=0; i<num; i++) {
			legs[i].updateForward();
			legs[i].updateBackward();
		}
	}
};
#endif