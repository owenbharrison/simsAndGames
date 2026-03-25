#pragma once
#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

//for rand
#include <cstdlib>

//for trig
#include <cmath>

namespace cmn {
	//clever default param placement:
	//random()=0-1
	//random(a)=0-a
	//random(a, b)=a-b
	float randFloat(float b=1, float a=0) {
		static const float rand_max=RAND_MAX;
		float t=std::rand()/rand_max;
		return a+t*(b-a);
	}

	//inclusive integer choice [a, b]
	int randInt(int a, int b) {
		return a+std::rand()%(1+b-a);
	}

	//clamps x to [a, b]
	template<typename T>
	T clamp(const T& x, const T& a, const T& b) {
		if(x<a) return a;
		if(x>b) return b;
		return x;
	}

	//remaps x from [a, b] to [c, d]
	template<typename T>
	T map(const T& x, const T& a, const T& b, const T& c, const T& d) {
		float t=float(x-a)/float(b-a);
		return c+t*(d-c);
	}

	static constexpr float Pi=3.1415927f;

	//polar to cartesian helper
	template<typename V2D>
	V2D polar(float rad, float angle) {
		return rad*V2D(std::cos(angle), std::sin(angle));
	}

	//thanks wikipedia + pattern recognition
	//NOTE: this returns t and u, NOT the point.
	template<typename V2D>
	V2D lineLineIntersection(
		const V2D& a, const V2D& b,
		const V2D& c, const V2D& d) {
		//get segments
		V2D ab=a-b, ac=a-c, cd=c-d;

		//parallel
		float denom=ab.x*cd.y-ab.y*cd.x;
		if(std::abs(denom)<1e-6f) return V2D(-1, -1);

		//find interpolators
		return V2D(
			ac.x*cd.y-ac.y*cd.x,
			ac.x*ab.y-ac.y*ab.x
		)/denom;
	}
}
#endif