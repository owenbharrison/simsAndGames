#pragma once
#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <cstdlib>
#include <cmath>

namespace cmn {
	//clever default param placement:
	//random()=0-1
	//random(a)=0-a
	//random(a, b)=a-b
	float random(float b=1, float a=0) {
		static const float rand_max=RAND_MAX;
		float t=rand()/rand_max;
		return a+t*(b-a);
	}

	//clamps x to [a, b]
	template<typename T>
	T clamp(T x, T a, T b) {
		if(x<a) return a;
		if(x>b) return b;
		return x;
	}

	//remaps x from [a, b] to [c,d]
	float map(float x, float a, float b, float c, float d) {
		float t=(x-a)/(b-a);
		return c+t*(d-c);
	}

	static constexpr float Pi=3.1415927f;

	//polar to cartesian helper
	template<typename V2D>
	V2D polar_generic(float rad, float angle) {
		return rad*V2D(std::cosf(angle), std::sinf(angle));
	}

	//thanks wikipedia + pattern recognition
	//NOTE: this returns t and u, NOT the point.
	template<typename V2D>
	V2D lineLineIntersection_generic(
		const V2D& a, const V2D& b,
		const V2D& c, const V2D& d) {
		//get segments
		V2D ab=a-b, ac=a-c, cd=c-d;

		//parallel
		float denom=ab.cross(cd);
		if(std::abs(denom)<1e-6f) return V2D(-1, -1);

		//find interpolators
		return V2D(
			ac.cross(cd),
			ac.cross(ab)
		)/denom;
	}
}
#endif