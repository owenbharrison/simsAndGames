#pragma once
#ifndef RUBIKS_CUBE_CLASS_H
#define RUBIKS_CUBE_CLASS_H

//for memcpy
#include <string>

//reverse pointed-to elements
template<typename T>
static void reverse_ptr(T** a, int l, int r) {
	T tmp;
	while(l<r) {
		tmp=*a[l];
		*a[l]=*a[r];
		*a[r]=tmp;
		l++, r--;
	}
}

//rotate pointed-to elements
template<typename T>
void rotate_left_ptr(T** a, int n, int k) {
	k%=n;
	if(k==0) return;

	reverse_ptr(a, 0, k-1);
	reverse_ptr(a, k, n-1);
	reverse_ptr(a, 0, n-1);
}

class RubiksCube {
	int num;

	void copyFrom(const RubiksCube&), clear();

	int** slice;
	int* face;

	void spinFace(int, bool);

public:
	int* grid;

	enum {
		Red=0,
		Yellow,
		Blue,
		Orange,
		White,
		Green
	};

	enum {
		Right=0,
		Top,
		Front,
		Left,
		Bottom,
		Back
	};

	RubiksCube() : RubiksCube(1) {}

	RubiksCube(int n) {
		num=n;
		grid=new int[6*num*num];
		slice=new int* [4*num];
		face=new int[num*num];
		reset();
	}

	//ro3: 1
	RubiksCube(const RubiksCube& r) {
		copyFrom(r);
	}

	//ro3: 2
	~RubiksCube() {
		clear();
	}

	//ro3: 3
	RubiksCube& operator=(const RubiksCube& r) {
		if(&r!=this) {
			clear();

			copyFrom(r);
		}

		return *this;
	}

	int getNum() const { return num; }

	int ix(int f, int i, int j) const { return i+num*j+num*num*f; }

	void inv_ix(int f, int i, int j, int& x, int& y, int& z) const {
		switch(f) {
			case Right: x=num-1, y=num-1-j, z=num-1-i; break;
			case Top: x=i, y=num-1, z=j; break;
			case Front: x=i, y=num-1-j, z=num-1; break;
			case Left: x=0, y=num-1-j, z=i; break;
			case Bottom: x=i, y=0, z=num-1-j; break;
			case Back: x=num-1-i, y=num-1-j, z=0; break; 
		}
	}

	void reset() {
		for(int f=0; f<6; f++) {
			for(int i=0; i<num; i++) {
				for(int j=0; j<num; j++) {
					grid[ix(f, i, j)]=f;
				}
			}
		}
	}

	void turnX(int x, bool ccw) {
		for(int i=0; i<num; i++) {
			slice[i]=&grid[ix(Top, x, i)];
			slice[num+i]=&grid[ix(Front, x, i)];
			slice[2*num+i]=&grid[ix(Bottom, x, i)];
			slice[3*num+i]=&grid[ix(Back, num-1-x, num-1-i)];
		}

		int amt=ccw?3*num:num;
		rotate_left_ptr(slice, 4*num, amt);

		if(x==0) spinFace(Left, ccw);
		else if(x==num-1) spinFace(Right, !ccw);
	}

	void turnY(int y, bool ccw) {
		for(int i=0; i<num; i++) {
			slice[i]=&grid[ix(Front, i, num-1-y)];
			slice[num+i]=&grid[ix(Right, i, num-1-y)];
			slice[2*num+i]=&grid[ix(Back, i, num-1-y)];
			slice[3*num+i]=&grid[ix(Left, i, num-1-y)];
		}

		int amt=ccw?3*num:num;
		rotate_left_ptr(slice, 4*num, amt);

		if(y==0) spinFace(Bottom, ccw);
		if(y==num-1) spinFace(Top, !ccw);
	}

	void turnZ(int z, bool ccw) {
		for(int i=0; i<num; i++) {
			slice[i]=&grid[ix(Top, num-1-i, z)];
			slice[num+i]=&grid[ix(Left, z, i)];
			slice[2*num+i]=&grid[ix(Bottom, i, num-1-z)];
			slice[3*num+i]=&grid[ix(Right, num-1-z, num-1-i)];
		}

		int amt=ccw?3*num:num;
		rotate_left_ptr(slice, 4*num, amt);

		if(z==0) spinFace(Back, ccw);
		else if(z==num-1) spinFace(Front, !ccw);
	}

	void scramble() {
		for(int i=0; i<30; i++) {
			int s=std::rand()%num;
			bool p=std::rand()%2;
			switch(std::rand()%3) {
				case 0: turnX(s, p); break;
				case 1: turnY(s, p); break;
				case 2: turnZ(s, p); break;
			}
		}
	}
};

void RubiksCube::copyFrom(const RubiksCube& r) {
	num=r.num;

	grid=new int[6*num*num];
	std::memcpy(grid, r.grid, sizeof(int)*6*num*num);

	slice=new int* [4*num];
	std::memcpy(slice, r.slice, sizeof(int*)*4*num);

	face=new int[num*num];
	std::memcpy(face, r.face, sizeof(int)*num*num);
}

void RubiksCube::clear() {
	delete[] grid;
	delete[] slice;
	delete[] face;
}

//helper for the turn methods
void RubiksCube::spinFace(int f, bool opp) {
	for(int i=0; i<num; i++) {
		for(int j=0; j<num; j++) {
			face[ix(0, i, j)]=grid[ix(f, i, j)];
		}
	}

	for(int i=0; i<num; i++) {
		for(int j=0; j<num; j++) {
			int ri=opp?num-1-j:j;
			int rj=opp?i:num-1-i;
			grid[ix(f, ri, rj)]=face[ix(0, i, j)];
		}
	}
}
#endif