#pragma once
#ifndef SHAPE_PRIMITIVE_STRUCT_H
#define SHAPE_PRIMITIVE_STRUCT_H

#include <vector>

struct ShapePrimitive {
	olc::Pixel col=olc::WHITE;

	ShapePrimitive() {
		//generate random color
		col.r=cmn::randInt(0, 255);
		col.g=cmn::randInt(0, 255);
		col.b=cmn::randInt(0, 255);
		col.a=255;
	}

	void chooseColor(olc::Sprite* spr) {
		col=getAvgColor(spr);

		//add some random to it
		col.r+=cmn::randInt(-40, 40);
		col.g+=cmn::randInt(-40, 40);
		col.b+=cmn::randInt(-40, 40);
	}

	virtual void randomizeGeometry(const vf2d&)=0;

	virtual olc::Pixel getAvgColor(olc::Sprite*)=0;
	
	virtual void addToImage(olc::Sprite*)=0;

	virtual std::vector<float*> getVariables()=0;
};
#endif