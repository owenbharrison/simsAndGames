#pragma once
#ifndef ELLIPSE_SHAPE_STRUCT_H
#define ELLIPSE_SHAPE_STRUCT_H

#include "shape.h"

struct EllipseShape : ShapePrimitive {
	vf2d ctr, size;
	float rot=0;

	void randomize(const vf2d& res) override {
		//position, size, & rotation
		ctr={cmn::random(res.x), cmn::random(res.y)};
		size={cmn::random(50), cmn::random(50)};
		rot=cmn::random(0, 2*cmn::Pi);
	}

	void addToImage(olc::Sprite* spr) const override {
		//get world bounds of ellipse
		const float r=std::max(size.x, size.y);
		const float min_x=ctr.x-r;
		const float min_y=ctr.y-r;
		const float max_x=ctr.x+r;
		const float max_y=ctr.y+r;

		//get screen bounds of ellipse
		const int sx=std::max(0, int(min_x));
		const int sy=std::max(0, int(min_y));
		const int ex=std::min(spr->width-1, int(max_x));
		const int ey=std::min(spr->height-1, int(max_y));

		//precompute rotation matrix
		const float c=std::cos(rot), s=std::sin(rot);

		//check all points in rect
		for(int i=sx; i<=ex; i++) {
			for(int j=sy; j<=ey; j++) {
				//distance from pixel to center of ellipse
				float dx=.5f+i-ctr.x;
				float dy=.5f+j-ctr.y;

				//find deltas in new coordinate system
				float rdx=dx*c-dy*s;
				float rdy=dx*s+dy*c;

				//is point inside ellipse?
				if(rdx*rdx/size.x/size.x+rdy*rdy/size.y/size.y<1) {
					spr->SetPixel(i, j, col);
				}
			}
		}
	}

	virtual std::vector<float*> getVariables() override {
		return {&ctr.x, &ctr.y, &size.x, &size.y, &rot};
	}
};
#endif