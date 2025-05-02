#pragma once
#ifndef AABB3_STRUCT_H
#define AABB3_STRUCT_H

#include "../math/v3d.h"

struct AABB3 {
	vf3d min{INFINITY, INFINITY, INFINITY}, max=-min;

	vf3d getCenter() const {
		return (min+max)/2;
	}

	bool contains(const vf3d& p) const {
		if(p.x<min.x||p.x>max.x) return false;
		if(p.y<min.y||p.y>max.y) return false;
		if(p.z<min.z||p.z>max.z) return false;
		return true;
	}

	bool overlaps(const AABB3& o) const {
		if(min.x>o.max.x||max.x<o.min.x) return false;
		if(min.y>o.max.y||max.y<o.min.y) return false;
		if(min.z>o.max.z||max.z<o.min.z) return false;
		return true;
	}

	void fitToEnclose(const vf3d& p) {
		if(p.x<min.x) min.x=p.x;
		if(p.y<min.y) min.y=p.y;
		if(p.z<min.z) min.z=p.z;
		if(p.x>max.x) max.x=p.x;
		if(p.y>max.y) max.y=p.y;
		if(p.z>max.z) max.z=p.z;
	}
};
#endif//AABB3_STRUCT_H