#pragma once
#ifndef VOXELSET_CLASS_H
#define VOXELSET_CLASS_H

typedef unsigned char byte;

class VoxelSet {
	void clear(), copyFrom(const VoxelSet& v);
	int w, h, d;

public:
	byte* grid=nullptr;

	enum {
		Empty,
		Inner,
		Surface,
		Edge,
		Corner
	};

	vf3d offset;
	float scale=1;

	VoxelSet() : VoxelSet(1, 1, 1) {}

	VoxelSet(int _w, int _h, int _d) : w(_w), h(_h), d(_d) {
		grid=new byte[w*h*d];
		memset(grid, false, sizeof(byte)*w*h*d);
	}

	//1
	VoxelSet(const VoxelSet& v) : w(v.w), h(v.h), d(v.d) {
		copyFrom(v);
	}

	//2
	~VoxelSet() {
		clear();
	}

	//3?
	VoxelSet& operator=(const VoxelSet& v) {
		if(&v==this) return *this;

		clear();

		copyFrom(v);

		return *this;
	}

	bool inRangeX(int i) const { return i>=0&&i<w; }
	bool inRangeY(int j) const { return j>=0&&j<h; }
	bool inRangeZ(int k) const { return k>=0&&k<d; }

	//flatten 3d array into 1d
	int ix(int i, int j, int k) const {
		return i+w*j+h*w*k;
	}

	//getter
	const byte& operator()(int i, int j, int k) const {
		return grid[ix(i, j, k)];
	}

	//setter
	byte& operator()(int i, int j, int k) {
		return grid[ix(i, j, k)];
	}

	int getW() const { return w; }
	int getH() const { return h; }
	int getD() const { return d; }

	int size() const { return w*h*d; }

	void updateTypes() {
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				for(int k=0; k<d; k++) {
					auto& v=grid[ix(i, j, k)];
					if(v==Empty) continue;

					bool left=i>0&&grid[ix(i-1, j, k)]!=Empty;
					bool right=i<w-1&&grid[ix(i+1, j, k)]!=Empty;
					bool lr=left&&right;
					bool up=j>0&&grid[ix(i, j-1, k)]!=Empty;
					bool down=j<h-1&&grid[ix(i, j+1, k)]!=Empty;
					bool ud=up&&down;
					bool front=k>0&&grid[ix(i, j, k-1)]!=Empty;
					bool back=k<d-1&&grid[ix(i, j, k+1)]!=Empty;
					bool fb=front&&back;

					//surrounded by how many axes?
					switch(lr+ud+fb) {
						case 0: v=Corner; break;
						case 1: v=Edge; break;
						case 2: v=Surface; break;
						//should inner have collisions??
						case 3: v=Inner; break;
					}
				}
			}
		}
	}

	bool empty() const {
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				for(int k=0; k<d; k++) {
					if(grid[ix(i, j, k)]!=Empty) return false;
				}
			}
		}
		return true;
	}

	//only keep solid blox
	//make sure to update offset
	void minimize() {
		//uhh
		if(empty()) return;

		//get bounds of solids
		int i_min=w-1, i_max=0;
		int j_min=h-1, j_max=0;
		int k_min=d-1, k_max=0;
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				for(int k=0; k<d; k++) {
					if(grid[ix(i, j, k)]==Empty) continue;

					//fit to enclose
					if(i<i_min) i_min=i;
					if(i>i_max) i_max=i;
					if(j<j_min) j_min=j;
					if(j>j_max) j_max=j;
					if(k<k_min) k_min=k;
					if(k>k_max) k_max=k;
				}
			}
		}

		//resizing
		bool resize_x=i_min>0||i_max<w-1;
		bool resize_y=j_min>0||j_max<h-1;
		bool resize_z=k_min>0||k_max<d-1;
		if(resize_x||resize_y||resize_z) {
			//make new minimized voxelset
			VoxelSet mini(1+i_max-i_min, 1+j_max-j_min, 1+k_max-k_min);
			for(int i=0; i<mini.getW(); i++) {
				for(int j=0; j<mini.getH(); j++) {
					for(int k=0; k<mini.getD(); k++) {
						//this space to mini space?
						mini(i, j, k)=grid[ix(i_min+i, j_min+j, k_min+k)];
					}
				}
			}
			//set this to mini!
			*this=mini;
		}
	}
};

void VoxelSet::copyFrom(const VoxelSet& v) {
	w=v.w, h=v.h, d=v.d;
	grid=new byte[w*h*d];
	memcpy(grid, v.grid, sizeof(byte)*w*h*d);
}

void VoxelSet::clear() {
	delete[] grid;
}
#endif