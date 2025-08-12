#pragma once
#ifndef BILLBOARD_STRUCT_H
#define BILLBOARD_STRUCT_H

struct Billboard {
	vf3d pos;
	float size=1;
	float ax=.5f, ay=.5f;
	int spr_ix=0;
	olc::Pixel col=olc::WHITE;
	int i=-1, j=-1, k=-1;
};
#endif