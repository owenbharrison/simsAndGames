#pragma once
#ifndef AABB_STRUCT_H
#define AABB_STRUCT_H

#include "common/olcPixelGameEngine.h"

struct AABB {
	olc::vf2d min{INFINITY, INFINITY}, max=-min;

	olc::vf2d getCenter() const {
		return (min+max)/2;
	}

	void fitToEnclose(const olc::vf2d& v) {
		if(v.x<min.x) min.x=v.x;
		if(v.y<min.y) min.y=v.y;
		if(v.x>max.x) max.x=v.x;
		if(v.y>max.y) max.y=v.y;
	}

	bool contains(const olc::vf2d& v) const {
		if(v.x<min.x||v.x>max.x) return false;
		if(v.y<min.y||v.y>max.y) return false;
		return true;
	}

	bool overlaps(const AABB& o) const {
		if(min.x>o.max.x||max.x<o.min.x) return false;
		if(min.y>o.max.y||max.y<o.min.y) return false;
		return true;
	}
};
#endif//AABB_STRUCT_H