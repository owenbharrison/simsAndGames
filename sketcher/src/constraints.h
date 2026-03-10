#pragma once
#ifndef CONSTRAINTS_UTIL_H
#define CONSTRAINTS_UTIL_H

static cmn::vf2d rotVec(const cmn::vf2d& v, float f) {
	//rotation matrix
	float c=std::cos(f), s=std::sin(f);
	return {v.x*c-v.y*s, v.x*s+v.y*c};
}

namespace constrain {
	static void angle(cmn::vf2d& a, cmn::vf2d& b, cmn::vf2d& c, cmn::vf2d& d, float ang) {
		cmn::vf2d ab=b-a, cd=d-c;
		float curr=std::atan2(ab.x*cd.y-ab.y*cd.x, ab.x*cd.x+ab.y*cd.y);

		float diff=(curr-ang)/2;

		cmn::vf2d amab=.5f*ab;
		cmn::vf2d mab=a+amab;
		a=mab+rotVec(-amab, diff);
		b=mab+rotVec(amab, diff);

		cmn::vf2d cmcd=.5f*cd;
		cmn::vf2d mcd=c+cmcd;
		c=mcd+rotVec(-cmcd, -diff);
		d=mcd+rotVec(cmcd, -diff);
	}

	static void dist(cmn::vf2d& a, cmn::vf2d& b, float len) {
		//separating axis
		cmn::vf2d ab=b-a;
		float mag=ab.mag();

		//safe norm
		cmn::vf2d norm=mag>1e-6f?ab/mag:cmn::vf2d(1, 0);

		//push apart
		float diff=(mag-len)/2;
		a+=diff*norm;
		b-=diff*norm;
	}

	//mid xy
	static void coincident(cmn::vf2d& a, cmn::vf2d& b) {
		cmn::vf2d m=(a+b)/2;
		a=m, b=m;
	}

	//mid y
	static void horizontal(cmn::vf2d& a, cmn::vf2d& b) {
		float m=(a.y+b.y)/2;
		a.y=m;
		b.y=m;
	}

	//mid x
	static void vertical(cmn::vf2d& a, cmn::vf2d& b) {
		float m=(a.x+b.x)/2;
		a.x=m;
		b.x=m;
	}
};
#endif