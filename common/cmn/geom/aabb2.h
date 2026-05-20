#pragma once
#ifndef CMN_AABB2_STRUCT_H
#define CMN_AABB2_STRUCT_H

#include "../math/v2d.h"

namespace cmn {
	template<typename T>
	struct AABB_2 {
		v_2d<T> min, max;

		v_2d<T> getCenter() const {
			return (min+max)/2;
		}

		bool contains(const v_2d<T>& p) const {
			if(p.x<min.x||p.x>max.x) return false;
			if(p.y<min.y||p.y>max.y) return false;
			return true;
		}

		bool overlaps(const AABB_2& o) const {
			if(min.x>o.max.x||max.x<o.min.x) return false;
			if(min.y>o.max.y||max.y<o.min.y) return false;
			return true;
		}

		void fitToEnclose(const v_2d<T>& p) {
			if(p.x<min.x) min.x=p.x;
			if(p.y<min.y) min.y=p.y;
			if(p.x>max.x) max.x=p.x;
			if(p.y>max.y) max.y=p.y;
		}
	};

	using AABBf2=AABB_2<float>;
	using AABBd2=AABB_2<double>;
}
#endif