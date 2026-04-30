#pragma once
#ifndef SHAPE_STRUCT_H
#define SHAPE_STRUCT_H

#include <list>

//for swap & clamp
#include <algorithm>

float sign(float x) {
	return x==0?0:x>0?1:-1;
}

olc::vf2d polar(float rad, float angle) {
	return {rad*std::cos(angle), rad*std::sin(angle)};
}

//QOL element-wise functions
olc::vf2d abs(const olc::vf2d& a) {
	return {std::abs(a.x), std::abs(a.y)};
}

olc::vf2d min(const olc::vf2d& a, const olc::vf2d& b) {
	return {std::min(a.x, b.x), std::min(a.y, b.y)};
}

olc::vf2d max(const olc::vf2d& a, const olc::vf2d& b) {
	return {std::max(a.x, b.x), std::max(a.y, b.y)};
}

struct SDFShape {
	olc::Pixel col;

	//list of pointers to shape control points
	virtual std::list<olc::vf2d*> getHandles()=0;

	virtual float signedDist(const olc::vf2d&)const=0;

	virtual void render(olc::PixelGameEngine*)const=0;
};

struct SDFRectangle : SDFShape {
	olc::vf2d p0, p1;

	std::list<olc::vf2d*> getHandles() override {
		return {&p0, &p1};
	}

	//https://www.youtube.com/watch?v=62-pRVZuS5c
	float signedDist(const olc::vf2d& p) const override {
		//half size
		auto h=abs(p0-p1)/2;
		//dist to corner
		auto ctr=(p0+p1)/2;
		auto d=abs(p-ctr)-h;

		//0 if inside on any axis
		auto dmax=max({0, 0}, d);
		float outside=dmax.mag();

		//which side is closer
		float inside=std::min(0.f, std::max(d.x, d.y));

		return outside+inside;
	}

	void render(olc::PixelGameEngine* pge) const override {
		auto h=abs(p0-p1)/2;
		auto ctr=(p0+p1)/2;
		pge->DrawRectDecal(ctr-h, 2*h, col);
	}
};

struct SDFCircle : SDFShape {
	olc::vf2d ctr, edge;
	
	std::list<olc::vf2d*> getHandles() override {
		return {&ctr, &edge};
	}

	float signedDist(const olc::vf2d& p) const override {
		float rad=(edge-ctr).mag();
		
		//d=r: 0 away
		return (p-ctr).mag()-rad;
	}

	void render(olc::PixelGameEngine* pge) const override {
		const int num=32;
		
		float rad=(edge-ctr).mag();
		
		olc::vf2d first, prev;
		for(int i=0; i<num; i++) {
			float angle=2*cmn::Pi*i/num;
			auto curr=ctr+polar(rad, angle);
			if(i==0) first=curr;
			else pge->DrawLineDecal(prev, curr, col);
			prev=curr;
		}
		pge->DrawLineDecal(prev, first, col);
	}
};

struct SDFTriangle : SDFShape {
	olc::vf2d p0, p1, p2;
	
	std::list<olc::vf2d*> getHandles() override {
		return {&p0, &p1, &p2};
	}

	//https://www.shadertoy.com/view/XsXSz4
	float signedDist(const olc::vf2d& p) const override {
		auto e0=p1-p0, e1=p2-p1, e2=p0-p2;
		auto v0=p-p0, v1=p-p1, v2=p-p2;
		auto pq0=v0-e0*std::clamp(v0.dot(e0)/e0.dot(e0), 0.f, 1.f);
		auto pq1=v1-e1*std::clamp(v1.dot(e1)/e1.dot(e1), 0.f, 1.f);
		auto pq2=v2-e2*std::clamp(v2.dot(e2)/e2.dot(e2), 0.f, 1.f);
		float s=sign(e0.x*e2.y-e0.y*e2.x);
		olc::vf2d d0(pq0.dot(pq0), s*(v0.x*e0.y-v0.y*e0.x));
		olc::vf2d d1(pq1.dot(pq1), s*(v1.x*e1.y-v1.y*e1.x));
		olc::vf2d d2(pq2.dot(pq2), s*(v2.x*e2.y-v2.y*e2.x));
		auto d=min(d0, min(d1, d2));
		return -sqrt(d.x)*sign(d.y);
	}

	void render(olc::PixelGameEngine* pge) const override {
		pge->DrawLineDecal(p0, p1, col);
		pge->DrawLineDecal(p1, p2, col);
		pge->DrawLineDecal(p2, p0, col);
	}
};
#endif