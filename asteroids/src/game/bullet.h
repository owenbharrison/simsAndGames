#pragma once
#ifndef BULLET_STRUCT_H
#define BULLET_STRUCT_H

#include "cmn/math/v2d.h"

struct Bullet {
	cmn::vf2d pos, vel;

	void update(float dt) {
		pos+=vel*dt;
	}
};
#endif