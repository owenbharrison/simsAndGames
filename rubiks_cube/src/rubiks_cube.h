#pragma once
#ifndef RUBIKS_CUBE_STRUCT_H
#define RUBIKS_CUBE_STRUCT_H

//for memcpy
#include <string>

//reverse pointed-to elements
static void reverse_ptr(int** a, int l, int r) {
	int tmp;
	while(l<r) {
		tmp=*a[l];
		*a[l++]=*a[r];
		*a[r--]=tmp;
	}
}

//rotate pointed-to elements
void rotate_left_ptr(int** a, int n, int k) {
	k%=n;
	if(k==0) return;

	reverse_ptr(a, 0, k-1);
	reverse_ptr(a, k, n-1);
	reverse_ptr(a, 0, n-1);
}

struct RubiksCube {
	const int num;
	int* grid[6];
	enum {
		Right=0,
		Top,
		Front,
		Left,
		Bottom,
		Back
	};

	RubiksCube(int n) : num(n) {
		for(int i=0; i<6; i++) {
			grid[i]=new int[num*num];
		}
		reset();
	}

	~RubiksCube() {
		for(int i=0; i<6; i++) {
			delete[] grid[i];
		}
	}

	int ix(int i, int j) const {
		return i+num*j;
	}

	void reset() {
		for(int i=0; i<6; i++) {
			for(int j=0; j<num*num; j++) {
				 grid[i][j]=i;
			}
		}
	}

	void turnX(int x, bool ccw) {
		int slice_num=4*num;
		int** slice=new int* [slice_num];
		for(int i=0; i<num; i++) {
			slice[i]=&grid[Top][ix(x, i)];
			slice[num+i]=&grid[Front][ix(x, i)];
			slice[2*num+i]=&grid[Bottom][ix(x, i)];
			slice[3*num+i]=&grid[Back][ix(num-1-x, num-1-i)];
		}

		int amt=ccw?3*num:num;
		rotate_left_ptr(slice, slice_num, amt);

		if(x==0) spinFace(Left, ccw);
		else if(x==num-1) spinFace(Right, !ccw);

		delete[] slice;
	}

	void turnY(int y, bool ccw) {
		int slice_num=4*num;
		int** slice=new int* [slice_num];
		for(int i=0; i<num; i++) {
			slice[i]=&grid[Front][ix(i, num-1-y)];
			slice[num+i]=&grid[Right][ix(i, num-1-y)];
			slice[2*num+i]=&grid[Back][ix(i, num-1-y)];
			slice[3*num+i]=&grid[Left][ix(i, num-1-y)];
		}

		int amt=ccw?3*num:num;
		rotate_left_ptr(slice, slice_num, amt);

		if(y==0) spinFace(Bottom, ccw);
		if(y==num-1) spinFace(Top, !ccw);

		delete[] slice;
	}

	void turnZ(int z, bool ccw) {
		int slice_num=4*num;
		int** slice=new int* [slice_num];
		for(int i=0; i<num; i++) {
			slice[i]=&grid[Top][ix(num-1-i, z)];
			slice[num+i]=&grid[Left][ix(z, i)];
			slice[2*num+i]=&grid[Bottom][ix(i, num-1-z)];
			slice[3*num+i]=&grid[Right][ix(num-1-z, num-1-i)];
		}

		int amt=ccw?3*num:num;
		rotate_left_ptr(slice, slice_num, amt);

		if(z==0) spinFace(Back, ccw);
		else if(z==num-1) spinFace(Front, !ccw);

		delete[] slice;
	}

	//helper for the turn methods
	void spinFace(int f, bool opp) {
		int* temp=new int[num*num];
		std::memcpy(temp, grid[f], sizeof(int)*num*num);

		if(opp) {
			for(int i=0; i<num; i++) {
				for(int j=0; j<num; j++) {
					grid[f][ix(num-1-j, i)]=temp[ix(i, j)];
				}
			}
		} else {
			for(int i=0; i<num; i++) {
				for(int j=0; j<num; j++) {
					grid[f][ix(j, num-1-i)]=temp[ix(i, j)];
				}
			}
		}

		delete[] temp;
	}
};
#endif