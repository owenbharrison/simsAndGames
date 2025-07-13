#pragma once
#ifndef BONE_STRUCT_H
#define BONE_STRUCT_H
struct Bone {
	int parent=-1;
	Mat4 mat_local;
	Mat4 mat_inv_model;
};
#endif