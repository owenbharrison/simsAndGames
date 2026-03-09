#pragma once
#ifndef CONSTRAINTS_UTIL_H
#define CONSTRAINTS_UTIL_H

static vf2d rotVec(const vf2d& v, float f) {
	//rotation matrix
	float c=std::cos(f), s=std::sin(f);
	return {v.x*c-v.y*s, v.x*s+v.y*c};
}

namespace constrain {
	static void angle(vf2d& a, vf2d& b, vf2d& c, vf2d& d, float ang) {
		vf2d ab=b-a, cd=d-c;
		float curr=std::atan2(ab.x*cd.y-ab.y*cd.x, ab.x*cd.x+ab.y*cd.y);

		float diff=(curr-ang)/2;

		vf2d amab=.5f*ab;
		vf2d mab=a+amab;
		a=mab+rotVec(-amab, diff);
		b=mab+rotVec(amab, diff);

		vf2d cmcd=.5f*cd;
		vf2d mcd=c+cmcd;
		c=mcd+rotVec(-cmcd, -diff);
		d=mcd+rotVec(cmcd, -diff);
	}

	static void dist(vf2d& a, vf2d& b, float len) {
		//separating axis
		vf2d ab=b-a;
		float mag=ab.mag();

		//safe norm
		vf2d norm=mag>1e-6f?ab/mag:vf2d(1, 0);

		//push apart
		float diff=(mag-len)/2;
		a+=diff*norm;
		b-=diff*norm;
	}

	//mid xy
	static void coincident(vf2d& a, vf2d& b) {
		vf2d m=(a+b)/2;
		a=m, b=m;
	}

	//mid y
	static void horizontal(vf2d& a, vf2d& b) {
		float m=(a.y+b.y)/2;
		a.y=m;
		b.y=m;
	}

	//mid x
	static void vertical(vf2d& a, vf2d& b) {
		float m=(a.x+b.x)/2;
		a.x=m;
		b.x=m;
	}
};
#endif