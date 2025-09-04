#pragma once 
#ifndef RECTANGLE_SHAPE_STRUCT_H
#define RECTANGLE_SHAPE_STRUCT_H

#include "shape.h"

#define FOREACH(func)\
/*get bounds of rectangle*/\
const int min_x=ctr.x-size.x;\
const int min_y=ctr.y-size.y;\
const int max_x=ctr.x+size.x;\
const int max_y=ctr.y+size.y;\
/*check each point in bounding box*/\
for(int i=min_x; i<=max_x; i++) {\
	for(int j=min_y; j<=max_y; j++) {\
		func\
	}\
}

struct RectangleShape : ShapePrimitive {
	vf2d ctr, size;

	void randomizeGeometry(const vf2d& res) override {
		//position & size
		ctr={cmn::random(res.x), cmn::random(res.y)};
		size={cmn::random(50), cmn::random(50)};
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
		return {&ctr.x, &ctr.y, &size.x, &size.y};
	}
};
#undef FOREACH
#endif