#pragma once
#ifndef FLUID_CLASS_H
#define FLUID_CLASS_H

//for memcpy & memset
#include <string>

class Fluid {
	int width=0;
	int height=0;
	int size_u=0;
	int size_v=0;

	void copyFrom(const Fluid&), clear();

public:
	float* u=nullptr;
	float* v=nullptr;
	float* u_prev=nullptr;
	float* v_prev=nullptr;

	Fluid() {}

	Fluid(int w, int h) {
		width=w;
		height=h;
		size_u=(width-1)*height;
		size_v=width*(height-1);
		u=new float[size_u];
		v=new float[size_v];
		std::memset(u, 0.f, sizeof(float)*size_u);
		std::memset(v, 0.f, sizeof(float)*size_v);
		u_prev=new float[size_u];
		v_prev=new float[size_v];
	}

	//ro3 1
	Fluid(const Fluid& f) {
		copyFrom(f);
	}

	//ro3 2
	~Fluid() {
		clear();
	}

	//ro3 3
	Fluid& operator=(const Fluid& f) {
		if(&f!=this) {
			clear();

			copyFrom(f);
		}

		return *this;
	}

	int uIX(int i, int j) const {
		return i+(width-1)*j;
	}

	int vIX(int i, int j) const {
		return i+width*j;
	}

	int getWidth() const { return width; }
	int getHeight() const { return height; }

	int getSizeU() const { return size_u; }
	int getSizeV() const { return size_v; }

	//bring divergence -> 0
	void solveIncompressibility() {
		//dont update while iterating, so first store into prev
		std::memcpy(u_prev, u, sizeof(float)*size_u);
		std::memcpy(v_prev, v, sizeof(float)*size_v);

		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				//state says which cell contributed
				int state=0;
				float div=0;
				//NOT rgt
				if(i!=width-1) state|=1, div+=u_prev[uIX(i, j)];
				//NOT left
				if(i!=0) state|=2, div-=u_prev[uIX(i-1, j)];
				//NOT BTM
				if(j!=height-1) state|=4, div+=v_prev[vIX(i, j)];
				//NOT top
				if(j!=0) state|=8, div-=v_prev[vIX(i, j-1)];

				//distribute
				int num=(1&(state>>3))+(1&(state>>2))+(1&(state>>1))+(1&(state>>0));
				float diff=div/num;
				if(state&1) u[uIX(i, j)]-=diff;
				if(state&2) u[uIX(i-1, j)]+=diff;
				if(state&4) v[vIX(i, j)]-=diff;
				if(state&8) v[vIX(i, j-1)]+=diff;
			}
		}
	}
};

void Fluid::copyFrom(const Fluid& f) {
	width=f.width;
	height=f.height;
	size_u=f.size_u;
	size_v=f.size_v;
	u=new float[size_u];
	v=new float[size_v];
	std::memcpy(u, f.u, sizeof(float)*size_u);
	std::memcpy(v, f.v, sizeof(float)*size_v);
	u_prev=new float[size_u];
	v_prev=new float[size_v];
	std::memcpy(u_prev, f.u_prev, sizeof(float)*size_u);
	std::memcpy(v_prev, f.v_prev, sizeof(float)*size_v);
}

void Fluid::clear() {
	delete[] u;
	delete[] v;
	delete[] u_prev;
	delete[] v_prev;
}
#endif