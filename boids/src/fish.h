#pragma once
#ifndef FISH_STRUCT_H
#define FISH_STRUCT_H

#include "boid.h"

struct Fish : Boid {
	float length;
	float height;
	float breadth;

	float anim=0;
	float anim_speed;
	float arg_scl;

	static int num_seg;
	static const int max_seg=16;

	sg_view tex{};

	cmn::vf3d dir_smooth;

	void update(float dt) {
		Boid::update(dt);

		anim+=dt*anim_speed;

		const float turn_rate=8;
		float t=1-std::exp(-turn_rate*dt);
		dir_smooth=normalize(dir_smooth+t*(dir-dir_smooth));
	}
};
int Fish::num_seg=8;
#endif