#pragma once
#ifndef NOISE_UTIL_H
#define NOISE_UTIL_H

#include "hash.h"

float smoothstep(float x) {
	return x*x*(3-2*x);
}

float mix(float a, float b, float t) {
	return a+t*(b-a);
}

float noise13(float x, float y, float z) {
	float xi=std::floor(x);
	float yi=std::floor(y);
	float zi=std::floor(z);
	float xf=smoothstep(fract(x));
	float yf=smoothstep(fract(y));
	float zf=smoothstep(fract(z));

	float a=hash13(xi+0, yi+0, zi+0);
	float b=hash13(xi+1, yi+0, zi+0);
	float c=hash13(xi+0, yi+1, zi+0);
	float d=hash13(xi+1, yi+1, zi+0);
	float e=hash13(xi+0, yi+0, zi+1);
	float f=hash13(xi+1, yi+0, zi+1);
	float g=hash13(xi+0, yi+1, zi+1);
	float h=hash13(xi+1, yi+1, zi+1);

	float ab=mix(a, b, xf);
	float cd=mix(c, d, xf);
	float ef=mix(e, f, xf);
	float gh=mix(g, h, xf);
	float abcd=mix(ab, cd, yf);
	float efgh=mix(ef, gh, yf);
	return mix(abcd, efgh, zf);
}

//fractal brownian motion?
//overlaid layers of noise with
//increasing frequency
//decreasing amplitude
float fbm13(float x, float y, float z, int n=8) {
	float val=0.;
	float amp=.5f;
	for(int i=0; i<n; i++) {
		val+=amp*noise13(x, y, z);
		x*=2, y*=2, z*=2;
		amp*=.5f;
	}
	return val;
}
#endif