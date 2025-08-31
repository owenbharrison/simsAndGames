#pragma once 
#ifndef RECTANGLE_SHAPE_STRUCT_H
#define RECTANGLE_SHAPE_STRUCT_H

#include "shape.h"

struct RectangleShape : ShapePrimitive {
	vf2d ctr, size;

	void randomize(const vf2d& res) override {
		//position & size
		ctr={cmn::random(res.x), cmn::random(res.y)};
		size={cmn::random(50), cmn::random(50)};
	}

	void addToImage(olc::Sprite* spr) const override {
		//get world bounds of rectangle
		const float min_x=ctr.x-size.x;
		const float min_y=ctr.y-size.y;
		const float max_x=ctr.x+size.x;
		const float max_y=ctr.y+size.y;

		//get screen bounds of triangle
		const int sx=std::max(0, int(min_x));
		const int sy=std::max(0, int(min_y));
		const int ex=std::min(spr->width-1, int(max_x));
		const int ey=std::min(spr->height-1, int(max_y));

		//for every point in rect:
		for(int i=sx; i<=ex; i++) {
			for(int j=sy; j<=ey; j++) {
				//draw pixel
				spr->SetPixel(i, j, col);
			}
		}
	}

	virtual std::vector<float*> getVariables() override {
		return {&ctr.x, &ctr.y, &size.x, &size.y};
	}
};
#endif