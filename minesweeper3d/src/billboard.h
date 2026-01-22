#pragma once
#ifndef BILLBOARD_STRUCT_H
#define BILLBOARD_STRUCT_H

//anchors: 0,0 = bl, 1,1 = tr
struct Billboard {
	cmn::vf3d pos;
	float size=0;
	float anchor_x=0, anchor_y=0;
	sg_view tex{};
	float left=0, top=0, right=0, bottom=0;
	sg_color col{1, 1, 1, 1};

	Billboard() {}

	Billboard(
		const cmn::vf3d& p, float s, float ax, float ay,
		const sg_view& tx, float l, float t, float r, float b,
		const sg_color& c
		) {
		pos=p, size=s, anchor_x=ax, anchor_y=ay;
		tex=tx, left=l, top=t, right=r, bottom=b;
		col=c;
	}
};
#endif