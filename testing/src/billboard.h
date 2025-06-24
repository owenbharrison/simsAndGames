#pragma once
#ifndef BILLBOARD_STRUCT_H
#define BILLBOARD_STRUCT_H

#include <string>

struct Billboard {
	vf3d pos;
	int id=0;
	std::string state;
	float anim=0;
	float w=1, h=1;
	float rot=0;

	void update(float dt) {
		anim+=dt;
	}
};
#endif