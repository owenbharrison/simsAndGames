#pragma once
#ifndef VOXEL_SET_CLASS_H
#define VOXEL_SET_CLASS_H

#include "cmn/math/v3d.h"

class VoxelSet {
	int width=0, height=0, depth=0;

	void copyFrom(const VoxelSet&), clear();

public:
	bool* grid=nullptr;

	cmn::vf3d trans;
	cmn::vf3d scale{1, 1, 1};

	VoxelSet() {}

	VoxelSet(int w, int h, int d) {
		width=w;
		height=h;
		depth=d;
		grid=new bool[width*height*depth];
		for(int i=0; i<width*height*depth; i++) grid[i]=false;
	}

	//ro3 1
	VoxelSet(const VoxelSet& v) {
		copyFrom(v);
	}

	//ro3 2
	~VoxelSet() {
		clear();
	}

	//ro3 3
	VoxelSet& operator=(const VoxelSet& v) {
		if(&v==this) return *this;

		clear();

		copyFrom(v);

		return *this;
	}

	int ix(int i, int j, int k) const {
		return i+width*j+width*height*k;
	}

	int getWidth() const { return width; }
	int getHeight() const { return height; }
	int getDepth() const { return depth; }
};

void VoxelSet::copyFrom(const VoxelSet& v) {
	width=v.width;
	height=v.height;
	depth=v.depth;
	grid=new bool[width*height*depth];
	for(int i=0; i<width*height*depth; i++) {
		grid[i]=v.grid[i];
	}
	trans=v.trans;
	scale=v.scale;
}

void VoxelSet::clear() {
	delete[] grid;
}

#endif