#pragma once
#ifndef NOISE_CLASS_H
#define NOISE_CLAS_H

#include <random>
#include <algorithm>

class PerlinNoise {
	int p[512];

	static float fade(float);
	static float lerp(float, float, float);
	static float grad(int, float, float, float);

public:
	PerlinNoise() {
		for(int i=0; i<512; i++) p[i]=0;
	}

	PerlinNoise(int seed) {
		//fill with 0-255
		for(int i=0; i<256; i++) p[i]=i;

		std::default_random_engine engine(seed);
		std::shuffle(p, p+256, engine);

		//duplicate the permutation vector
		for(int i=0; i<256; i++) p[256+i]=p[i];
	}

	//values ~ [0, 1]
	float noise(float x, float y, float z) const {
		//int portion 0-255
		int X=static_cast<int>(std::floor(x))&255;
		int Y=static_cast<int>(std::floor(y))&255;
		int Z=static_cast<int>(std::floor(z))&255;

		//fractional portion
		x-=std::floor(x);
		y-=std::floor(y);
		z-=std::floor(z);

		//smoothstep
		float u=fade(x);
		float v=fade(y);
		float w=fade(z);

		int A=Y+p[X];
		int AA=Z+p[A], AB=Z+p[A+1];
		int B=Y+p[X+1];
		int BA=Z+p[B], BB=Z+p[B+1];

		//interpolate along x, y
		float x1, x2, y1, y2;
		x1=lerp(grad(p[AA], x, y, z), grad(p[BA], x-1, y, z), u);
		x2=lerp(grad(p[AB], x, y-1, z), grad(p[BB], x-1, y-1, z), u);
		y1=lerp(x1, x2, v);

		x1=lerp(grad(p[AA+1], x, y, z-1), grad(p[BA+1], x-1, y, z-1), u);
		x2=lerp(grad(p[AB+1], x, y-1, z-1), grad(p[BB+1], x-1, y-1, z-1), u);
		y2=lerp(x1, x2, v);

		//interpolate along z
		return .5f+.5f*lerp(y1, y2, w);
	}
};

float PerlinNoise::fade(float t) {
	return t*t*t*(t*(t*6-15)+10);
}

float PerlinNoise::lerp(float a, float b, float t) {
	return a+t*(b-a);
}

float PerlinNoise::grad(int hash, float x, float y, float z) {
	//make 0-15
	int h=hash&15;
	float u=h<8?x:y;
	float v=h<4?y:(h==12||h==14?x:z);
	return ((h&1)?-u:u)+((h&2)?-v:v);
}
#endif