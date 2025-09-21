#pragma once
#ifndef BULLET_STRUCT_H
#define BULLET_STRUCT_H

struct Bullet {
	vf2d pos, vel;

	void update(float dt) {
		pos+=vel*dt;
	}
};
#endif