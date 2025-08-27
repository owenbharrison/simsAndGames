#pragma once
#ifndef VOXEL_SET_CLASS_H
#define VOXEL_SET_CLASS_H

#include <string>

class VoxelSet {
	int width=0, height=0, depth=0;

	void copyFrom(const VoxelSet&), clear();

public:
	bool* grid=nullptr;

	vf3d scale{1, 1, 1};
	vf3d offset;

	VoxelSet() {}

	VoxelSet(int w, int h, int d) {
		width=w;
		height=h;
		depth=d;
		grid=new bool[width*height*depth];
		std::memset(grid, false, sizeof(bool)*width*height*depth);
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
	std::memcpy(grid, v.grid, sizeof(bool)*width*height*depth);
	
	scale=v.scale;
	offset=v.offset;
}

void VoxelSet::clear() {
	delete[] grid;
}

#endif