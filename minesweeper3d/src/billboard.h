#pragma once
#ifndef BILLBOARD_STRUCT_H
#define BILLBOARD_STRUCT_H

#include "cmn/math/v3d.h"

//anchors: 0,0 = bl, 1,1 = tr
struct Billboard {
	cmn::vf3d pos;
	float size=0;
	float anchor_x=0, anchor_y=0;
	sg_view tex{};
	float ltrb[4]{0, 0, 1, 1};
	float rgba[4]{1, 1, 1, 1};

	Billboard() {}

	Billboard(
		const cmn::vf3d& p, float s, float ax, float ay,
		const sg_view& tx, float tl, float tt, float tr, float tb,
		float r, float g, float b, float a
		) {
		pos=p, size=s, anchor_x=ax, anchor_y=ay;
		tex=tx, ltrb[0]=tl, ltrb[1]=tt, ltrb[2]=tr, ltrb[3]=tb;
		rgba[0]=r, rgba[1]=g, rgba[2]=b, rgba[3]=a;
	}
};
#endif