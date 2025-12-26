#pragma once
#ifndef SHAPE_STRUCT_H
#define SHAPE_STRUCT_H

#include "mesh.h"

#include "linemesh.h"

#include "cmn/math/v3d.h"

#include "cmn/math/mat4.h"

struct Shape {
	Mesh mesh;

	LineMesh linemesh;

	sg_view tex{};

	cmn::vf3d translation, rotation, scale{1, 1, 1};
	cmn::mat4 model=cmn::mat4::makeIdentity();

	Shape() {}

	Shape(const Mesh& m, sg_view t) {
		mesh=m;
		linemesh=LineMesh::makeFromMesh(m);
		linemesh.randomizeColors();
		linemesh.updateVertexBuffer();
		tex=t;
	}

	void updateMatrixes() {
		//xyz euler angles?
		cmn::mat4 rot_x=cmn::mat4::makeRotX(rotation.x);
		cmn::mat4 rot_y=cmn::mat4::makeRotY(rotation.y);
		cmn::mat4 rot_z=cmn::mat4::makeRotZ(rotation.z);
		cmn::mat4 rot=cmn::mat4::mul(rot_z, cmn::mat4::mul(rot_y, rot_x));

		cmn::mat4 scl=cmn::mat4::makeScale(scale);

		cmn::mat4 trans=cmn::mat4::makeTranslation(translation);

		//combine & invert
		model=cmn::mat4::mul(trans, cmn::mat4::mul(rot, scl));
	}
};
#endif