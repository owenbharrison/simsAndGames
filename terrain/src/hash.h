//https://www.shadertoy.com/view/4djSRW
#pragma once
#ifndef HASH_UTIL_H
#define HASH_UTIL_H

//for floor
#include <cmath>

float fract(float x) {
	return x-std::floor(x);
}

//inplace

void hash4(float& x, float& y, float& z, float& w) {
	x=fract(.1031f*x);
	y=fract(.1030f*y);  
	z=fract(.0973f*z);
	w=fract(.1099f*w);
	static const float f=33.33f;
	float d=x*(f+y)+y*(f+z)+z*(f+w)+w*(f+x);
	x+=d, y+=d, z+=d, w+=d;
	x=fract(y*(z+w));
	y=fract(z*(w+x));
	z=fract(w*(x+y));
	w=fract(x*(y+z));
}

void hash1(float& x) {
	float y=0, z=0, w=0;
	hash4(x, y, z, w);
}

void hash2(float& x, float& y) {
	float z=0, w=0;
	hash4(x, y, z, w);
}

void hash3(float& x, float& y, float& z) {
	float w=0;
	hash4(x, y, z, w);
}

//#in, #out

float hash11(float x) {
	hash1(x);
	return x;
}

float hash12(float x, float y) {
	hash2(x, y);
	return x;
}

float hash13(float x, float y, float z) {
	hash3(x, y, z);
	return x;
}

float hash14(float x, float y, float z, float w) {
	hash4(x, y, z, w);
	return x;
}
#endif