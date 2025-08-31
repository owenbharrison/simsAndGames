#pragma once
#ifndef SHAPE_PRIMITIVE_STRUCT_H
#define SHAPE_PRIMITIVE_STRUCT_H

#include <vector>

struct ShapePrimitive {
	olc::Pixel col=olc::WHITE;

	ShapePrimitive() {
		//generate random color
		col.r=std::rand()%256;
		col.g=std::rand()%256;
		col.b=std::rand()%256;
		col.a=255;
	}

	virtual void randomize(const vf2d&)=0;

	virtual void addToImage(olc::Sprite* spr) const=0;

	virtual std::vector<float*> getVariables()=0;
};
#endif