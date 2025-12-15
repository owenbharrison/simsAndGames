#pragma once
#ifndef SHAPE_STRUCT_H
#define SHAPE_STRUCT_H

#include "mesh.h"

#include "math/mat4.h"

#include "texture.h"

struct Shape {
	Mesh mesh;

	vf3d translation, rotation, scale{1, 1, 1};
	mat4 model=mat4::makeIdentity();

	sg_view tex{};

	bool draggable=false;

	Shape() {}

	Shape(const Mesh& m, sg_view t, bool d) {
		mesh=m;
		tex=t;
		draggable=d;
	}

	void updateMatrixes() {
		//xyz euler angles?
		mat4 rot_x=mat4::makeRotX(rotation.x);
		mat4 rot_y=mat4::makeRotY(rotation.y);
		mat4 rot_z=mat4::makeRotZ(rotation.z);
		mat4 rot=mat4::mul(rot_z, mat4::mul(rot_y, rot_x));

		mat4 scl=mat4::makeScale(scale);

		mat4 trans=mat4::makeTranslation(translation);

		//combine & invert
		model=mat4::mul(trans, mat4::mul(rot, scl));
	}
};
#endif