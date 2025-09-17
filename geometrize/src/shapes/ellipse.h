#pragma once
#ifndef ELLIPSE_SHAPE_STRUCT_H
#define ELLIPSE_SHAPE_STRUCT_H

#include "shape.h"

#define FOREACH(func)\
/*get bounds of ellipse*/\
const float rad=std::max(size.x, size.y);\
const int min_x=ctr.x-rad;\
const int min_y=ctr.y-rad;\
const int max_x=ctr.x+rad;\
const int max_y=ctr.y+rad;\
/*precompute rotation matrix*/\
const float c=std::cos(rot), s=std::sin(rot);\
/*check each point in bounding box*/\
for(int i=min_x; i<=max_x; i++) {\
	for(int j=min_y; j<=max_y; j++) {\
		/*distance from pixel to center of ellipse*/\
		float dx=.5f+i-ctr.x;\
		float dy=.5f+j-ctr.y;\
		/*find deltas in new coordinate system*/\
		float rdx=dx*c-dy*s;\
		float rdy=dx*s+dy*c;\
		/*is point inside ellipse?*/\
		if(rdx*rdx/size.x/size.x+rdy*rdy/size.y/size.y<1) {\
			func\
		}\
	}\
}

struct EllipseShape : ShapePrimitive {
	vf2d ctr, size;
	float rot=0;

	void randomizeGeometry(const vf2d& res) override {
		//position, size, & rotation
		ctr={cmn::randFloat(res.x), cmn::randFloat(res.y)};
		size={cmn::randFloat(50), cmn::randFloat(50)};
		rot=cmn::randFloat(0, 2*cmn::Pi);
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
		return {&ctr.x, &ctr.y, &size.x, &size.y, &rot};
	}
};
#undef FOREACH
#endif