#pragma once
#ifndef SHAPE_STRUCT_H
#define SHAPE_STRUCT_H

#include "cmn/math/v2d.h"

#include <list>

//for swap & clamp
#include <algorithm>

float sign(float x) {
	return x==0?0:x>0?1:-1;
}

//QOL element-wise functions
cmn::vf2d abs(const cmn::vf2d& a) {
	return {std::abs(a.x), std::abs(a.y)};
}

cmn::vf2d min(const cmn::vf2d& a, const cmn::vf2d& b) {
	return {std::min(a.x, b.x), std::min(a.y, b.y)};
}

cmn::vf2d max(const cmn::vf2d& a, const cmn::vf2d& b) {
	return {std::max(a.x, b.x), std::max(a.y, b.y)};
}

struct SDFShape {
	float r=0, g=0, b=0;

	//list of pointers to shape control points
	virtual std::list<cmn::vf2d*> getHandles()=0;

	virtual float signedDist(const cmn::vf2d&)const=0;

	virtual void render()const=0;
};

struct SDFRectangle : SDFShape {
	cmn::vf2d p0, p1;

	std::list<cmn::vf2d*> getHandles() override {
		return {&p0, &p1};
	}

	//https://www.youtube.com/watch?v=62-pRVZuS5c
	float signedDist(const cmn::vf2d& p) const override {
		cmn::vf2d sz=abs(p0-p1);
		//dist to corner
		cmn::vf2d ctr=(p0+p1)/2;
		cmn::vf2d d=abs(p-ctr)-.5f*sz;

		//0 if inside on any axis
		cmn::vf2d dmax=max({0, 0}, d);
		float outside=dmax.mag();

		//which side is closer
		float inside=std::min(0.f, std::max(d.x, d.y));

		return outside+inside;
	}

	void render() const override {
		cmn::vf2d sz=abs(p0-p1);
		cmn::vf2d ctr=.5f*(p0+p1);
		cmn::vf2d pos=ctr-.5f*sz;
		cmn::draw_rect(
			pos.x, pos.y, sz.x, sz.y,
			r, g, b
		);
	}
};

struct SDFCircle : SDFShape {
	cmn::vf2d ctr, edge;
	
	std::list<cmn::vf2d*> getHandles() override {
		return {&ctr, &edge};
	}

	float signedDist(const cmn::vf2d& p) const override {
		float rad=(edge-ctr).mag();
		
		//d=r: 0 away
		return (p-ctr).mag()-rad;
	}

	void render() const override {
		float rad=(edge-ctr).mag();
		
		cmn::draw_circle(
			ctr.x, ctr.y, rad,
			r, g, b
		);
	}
};

struct SDFTriangle : SDFShape {
	cmn::vf2d p0, p1, p2;
	
	std::list<cmn::vf2d*> getHandles() override {
		return {&p0, &p1, &p2};
	}

	//https://www.shadertoy.com/view/XsXSz4
	float signedDist(const cmn::vf2d& p) const override {
		cmn::vf2d e0=p1-p0, e1=p2-p1, e2=p0-p2;
		cmn::vf2d v0=p-p0, v1=p-p1, v2=p-p2;
		cmn::vf2d pq0=v0-e0*std::clamp(v0.dot(e0)/e0.dot(e0), 0.f, 1.f);
		cmn::vf2d pq1=v1-e1*std::clamp(v1.dot(e1)/e1.dot(e1), 0.f, 1.f);
		cmn::vf2d pq2=v2-e2*std::clamp(v2.dot(e2)/e2.dot(e2), 0.f, 1.f);
		float s=sign(e0.x*e2.y-e0.y*e2.x);
		cmn::vf2d d0(pq0.dot(pq0), s*(v0.x*e0.y-v0.y*e0.x));
		cmn::vf2d d1(pq1.dot(pq1), s*(v1.x*e1.y-v1.y*e1.x));
		cmn::vf2d d2(pq2.dot(pq2), s*(v2.x*e2.y-v2.y*e2.x));
		cmn::vf2d d=min(d0, min(d1, d2));
		return -std::sqrt(d.x)*sign(d.y);
	}

	void render() const override {
		cmn::draw_triangle(
			p0.x, p0.y, p1.x, p1.y, p2.x, p2.y,
			r, g, b
		);
	}
};
#endif