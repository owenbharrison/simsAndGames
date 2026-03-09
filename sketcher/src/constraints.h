#pragma once
#ifndef CONSTRAINTS_UTIL_H
#define CONSTRAINTS_UTIL_H

static vf2d rotVec(const vf2d& v, float f) {
	float c=std::cos(f), s=std::sin(f);
	return {v.x*c-v.y*s, v.x*s+v.y*c};
}

namespace constrain {
	void angle(vf2d& a, vf2d& b, vf2d& c, vf2d& d, float ang) {
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
		vf2d ab=b-a;
		float curr=ab.mag();
		//safe norm
		vf2d norm=curr>1e-6f?ab/curr:vf2d(1, 0);
		float diff=(curr-len)/2;
		a+=diff*norm;
		b-=diff*norm;
	}

	static void coincident(vf2d& a, vf2d& b) {
		dist(a, b, 0);
	}

	//set to middle
	static void horizontal(vf2d& a, vf2d& b) {
		float m=(a.y+b.y)/2;
		a.y=m;
		b.y=m;
	}

	//set to middle
	static void vertical(vf2d& a, vf2d& b) {
		float m=(a.x+b.x)/2;
		a.x=m;
		b.x=m;
	}

	//come back to this.
	//use principal component analysis
	/*linear regression
	static void colinear(vf2d& a, vf2d& b, vf2d& c) {
		float x=(a.x+b.x+c.x)/3;
		float y=(a.y+b.y+c.y)/3;
		float dax=a.x-x;
		float dbx=b.x-x;
		float dcx=c.x-x;
		float num=dax*(a.y-y)+dbx*(b.y-y)+dcx*(c.y-y);
		float den=dax*dax+dbx*dbx+dcx*dcx;
		if(den==0) return;//vertical
		float m=num/den;
		float b=y-m*x;
	}
	*/
}
#endif