#pragma once
#ifndef AABB3_STRUCT_H
#define AABB3_STRUCT_H

#include "v3d.h"

namespace cmn {
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

	bool rayIntersectBox(const vf3d& orig, const vf3d& dir, const cmn::AABB3& box) {
		const float epsilon=1e-6f;
		float tmin=-INFINITY;
		float tmax=INFINITY;

		//x axis
		if(std::abs(dir.x)>epsilon) {
			float inv_d=1/dir.x;
			float t1=inv_d*(box.min.x-orig.x);
			float t2=inv_d*(box.max.x-orig.x);
			if(t1>t2) std::swap(t1, t2);
			tmin=std::max(tmin, t1);
			tmax=std::min(tmax, t2);
		} else if(orig.x<box.min.x||orig.x>box.max.x) return false;

		//y axis
		if(std::abs(dir.y)>epsilon) {
			float inv_d=1/dir.y;
			float t3=inv_d*(box.min.y-orig.y);
			float t4=inv_d*(box.max.y-orig.y);
			if(t3>t4) std::swap(t3, t4);
			tmin=std::max(tmin, t3);
			tmax=std::min(tmax, t4);
		} else if(orig.y<box.min.y||orig.y>box.max.y) return false;

		//z axis
		if(std::abs(dir.z)>epsilon) {
			float inv_d=1/dir.z;
			float t5=inv_d*(box.min.z-orig.z);
			float t6=inv_d*(box.max.z-orig.z);
			if(t5>t6) std::swap(t5, t6);
			tmin=std::max(tmin, t5);
			tmax=std::min(tmax, t6);
		} else if(orig.z<box.min.z||orig.z>box.max.z) return false;

		if(tmax<0||tmin>tmax) return false;

		return true;
	}
}
#endif//AABB3_STRUCT_H