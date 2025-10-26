#pragma once
#ifndef TRIANGLE_SHAPE_STRUCT_H
#define TRIANGLE_SHAPE_STRUCT_H

#include "shape.h"

#define FOREACH(func)\
/*get bounds of triangle*/\
const int min_x=std::min(a.x, std::min(b.x, c.x));\
const int min_y=std::min(a.y, std::min(b.y, c.y));\
const int max_x=std::max(a.x, std::max(b.x, c.x));\
const int max_y=std::max(a.y, std::max(b.y, c.y));\
/*get edge vectors*/\
const vf2d ab=b-a;\
const vf2d bc=c-b;\
const vf2d ca=a-c;\
/*check each point in bounding box*/\
for(int i=min_x; i<=max_x; i++) {\
	for(int j=min_y; j<=max_y; j++) {\
		/*get corner vectors*/\
		vf2d p(.5f+i, .5f+j);\
		vf2d pa=p-a;\
		vf2d pb=p-b;\
		vf2d pc=p-c;\
		/*what side of each edge is point on*/\
		bool sab=pa.x*ab.y-pa.y*ab.x>0;\
		bool sbc=pb.x*bc.y-pb.y*bc.x>0;\
		bool sca=pc.x*ca.y-pc.y*ca.x>0;\
		/*is point on same side of all?*/\
		if(sab==sbc&&sbc==sca) {\
			func\
		}\
	}\
}

struct TriangleShape : ShapePrimitive {
	vf2d a, b, c;

	void randomizeGeometry(const vf2d& res) override {
		//vertex positioning
		float cx=cmn::randFloat(res.x);
		float cy=cmn::randFloat(res.y);
		a={cx+cmn::randFloat(-30, 30), cy+cmn::randFloat(-30, 30)};
		b={cx+cmn::randFloat(-30, 30), cy+cmn::randFloat(-30, 30)};
		c={cx+cmn::randFloat(-30, 30), cy+cmn::randFloat(-30, 30)};
	}

	olc::Pixel getAvgColor(olc::Sprite* spr) override {
		int red=0, green=0, blue=0, num=0;
		FOREACH(const auto& p=spr->GetPixel(i, j); red+=p.r; green+=p.g; blue+=p.b; num++;);
		return num==0?olc::WHITE:olc::Pixel(red/num, green/num, blue/num);
	}

	void addToImage(olc::Sprite* spr) override {
		FOREACH(spr->SetPixel(i, j, col););
	}

	std::vector<float*> getVariables() override {
		return {&a.x, &a.y, &b.x, &b.y, &c.x, &c.y};
	}
};
#undef FOREACH
#endif