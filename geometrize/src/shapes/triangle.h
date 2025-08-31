#pragma once
#ifndef TRIANGLE_SHAPE_STRUCT_H
#define TRIANGLE_SHAPE_STRUCT_H

#include "shape.h"

struct TriangleShape : ShapePrimitive {
	vf2d a, b, c;

	void randomize(const vf2d& res) override {
		//vertex positioning
		a={cmn::random(res.x), cmn::random(res.y)};
		b={cmn::random(res.x), cmn::random(res.y)};
		c={cmn::random(res.x), cmn::random(res.y)};
	}

	void addToImage(olc::Sprite* spr) const override {
		//get world bounds of triangle
		float min_x=std::min(a.x, std::min(b.x, c.x));
		float min_y=std::min(a.y, std::min(b.y, c.y));
		float max_x=std::max(a.x, std::max(b.x, c.x));
		float max_y=std::max(a.y, std::max(b.y, c.y));

		//get screen bounds of triangle
		const int sx=std::max(0, int(min_x));
		const int sy=std::max(0, int(min_y));
		const int ex=std::min(spr->width-1, int(max_x));
		const int ey=std::min(spr->height-1, int(max_y));

		//get edge vectors
		const vf2d ab=b-a;
		const vf2d bc=c-b;
		const vf2d ca=a-c;

		//for every point in rect:
		for(int i=sx; i<=ex; i++) {
			for(int j=sy; j<=ey; j++) {
				//get corner vectors
				vf2d p(.5f+i, .5f+j);
				vf2d pa=p-a;
				vf2d pb=p-b;
				vf2d pc=p-c;

				//what side of each edge is point on
				bool sab=pa.x*ab.y-pa.y*ab.x>0;
				bool sbc=pb.x*bc.y-pb.y*bc.x>0;
				bool sca=pc.x*ca.y-pc.y*ca.x>0;

				//is point on same side of all?
				if(sab==sbc&&sbc==sca) {
					spr->SetPixel(i, j, col);
				}
			}
		}
	}
};
#endif