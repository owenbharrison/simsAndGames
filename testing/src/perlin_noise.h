#pragma once
#ifndef NOISE_CLASS_H
#define NOISE_CLASS_H

#include <random>
#include <algorithm>
#include <cmath>

class PerlinNoise {
	int p[512];

	static float fade(float t) {
		return t*t*t*(t*(t*6-15)+10);
	}

	static float lerp(float a, float b, float t) {
		return a+t*(b-a);
	}

	//2d gradient function
	static float grad(int hash, float x, float y) {
		int h=hash&7;//8 directions
		float u=h<4?x:y;
		float v=h<4?y:x;
		return ((h&1)?-u:u)+((h&2)?-v:v);
	}

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
	float noise(float x, float y) const {
		//integer parts
		int X=static_cast<int>(std::floor(x))&255;
		int Y=static_cast<int>(std::floor(y))&255;

		//fractional parts
		x-=std::floor(x);
		y-=std::floor(y);

		//smoothstep
		float u=fade(x);
		float v=fade(y);

		int A=p[X]+Y;
		int B=p[1+X]+Y;

		//interpolate along x
		float x1=lerp(grad(p[A], x, y), grad(p[B], x-1, y), u);
		float x2=lerp(grad(p[1+A], x, y-1), grad(p[1+B], x-1, y-1), u);

		//interpolate along y
		return .5f+.5f*lerp(x1, x2, v);
	}
};
#endif