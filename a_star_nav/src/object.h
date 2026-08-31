#pragma once
#ifndef OBJECT_STRUCT_H
#define OBJECT_STRUCT_H

#include "mesh.h"

#include "common/math/mat4.h"

struct Object {
	Mesh* mesh;
	cmn::vf3d pos;
	float scl=1;
	cmn::vf3d rot;
	cmn::mat4 model;
	float rgb[3]{1, 1, 1};

	void updateMatrix() {
		cmn::mat4 trans=mat4::makeTranslation(pos);
		cmn::mat4 rot_x=mat4::makeRotX(rot.x);
		cmn::mat4 rot_y=mat4::makeRotY(rot.y);
		cmn::mat4 rot_z=mat4::makeRotZ(rot.z);
		cmn::mat4 rot=cmn::mat4::mul(rot_z, cmn::mat4::mul(rot_y, rot_x));
		cmn::mat4 scale=cmn::mat4::makeScale(scl*cmn::vf3d(1, 1, 1));
		model=cmn::mat4::mul(trans, cmn::mat4::mul(scale, rot));
	}
};
#endif