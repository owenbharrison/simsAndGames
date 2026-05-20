#pragma once
#ifndef CMN_AABB3_STRUCT_H
#define CMN_AABB3_STRUCT_H

#include "../math/v3d.h"

//for swap
#include <algorithm>

namespace cmn {
	template<typename T>
	struct AABB_3 {
		v_3d<T> min, max;

		v_3d<T> getCenter() const {
			return (min+max)/2;
		}

		bool contains(const v_3d<T>& p) const {
			if(p.x<min.x||p.x>max.x) return false;
			if(p.y<min.y||p.y>max.y) return false;
			if(p.z<min.z||p.z>max.z) return false;
			return true;
		}

		bool overlaps(const AABB_3& o) const {
			if(min.x>o.max.x||max.x<o.min.x) return false;
			if(min.y>o.max.y||max.y<o.min.y) return false;
			if(min.z>o.max.z||max.z<o.min.z) return false;
			return true;
		}

		void fitToEnclose(const v_3d<T>& p) {
			if(p.x<min.x) min.x=p.x;
			if(p.y<min.y) min.y=p.y;
			if(p.z<min.z) min.z=p.z;
			if(p.x>max.x) max.x=p.x;
			if(p.y>max.y) max.y=p.y;
			if(p.z>max.z) max.z=p.z;
		}

		double intersectRay(const v_3d<T>& orig, const v_3d<T>& dir) const {
			const double eps=1e-6;
			double tmin=-1e300;
			double tmax=1e300;

			//x axis
			if(std::abs(dir.x)>eps) {
				double inv_d=1./dir.x;
				double t1=inv_d*(min.x-orig.x);
				double t2=inv_d*(max.x-orig.x);
				if(t1>t2) std::swap(t1, t2);
				if(t1>tmin) tmin=t1;
				if(t2<tmax) tmax=t2;
			}
			//parallel
			else if(orig.x<min.x||orig.x>max.x) return -1;

			//y axis
			if(std::abs(dir.y)>eps) {
				double inv_d=1./dir.y;
				double t3=inv_d*(min.y-orig.y);
				double t4=inv_d*(max.y-orig.y);
				if(t3>t4) std::swap(t3, t4);
				if(t3>tmin) tmin=t3;
				if(t4<tmax) tmax=t4;
			}
			//parallel
			else if(orig.y<min.y||orig.y>max.y) return -1;

			//z axis
			if(std::abs(dir.z)>eps) {
				double inv_d=1./dir.z;
				double t5=inv_d*(min.z-orig.z);
				double t6=inv_d*(max.z-orig.z);
				if(t5>t6) std::swap(t5, t6);
				if(t5>tmin) tmin=t5;
				if(t6<tmax) tmax=t6;
			}
			//parallel
			else if(orig.z<min.z||orig.z>max.z) return -1;

			if(tmax<0||tmin>tmax) return -1;

			return tmin;
		}
	};

	using AABBf3=AABB_3<float>;
	using AABBd3=AABB_3<double>;
}
#endif