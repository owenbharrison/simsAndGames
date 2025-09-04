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

	void chooseColor(olc::Sprite* spr) {
		col=getAvgColor(spr);

		//add some random to it
		col.r+=40-std::rand()%81;
		col.g+=40-std::rand()%81;
		col.b+=40-std::rand()%81;
	}

	virtual void randomizeGeometry(const vf2d&)=0;

	virtual olc::Pixel getAvgColor(olc::Sprite*)=0;
	
	virtual void addToImage(olc::Sprite*)=0;

	virtual std::vector<float*> getVariables()=0;
};
#endif