#pragma once
#ifndef CMN_V3D_STRUCT_H
#define CMN_V3D_STRUCT_H

//for sqrt
#include <cmath>

namespace cmn {
	template<typename T>
	struct v_3d {
		T x=0, y=0, z=0;

		v_3d() {}
		v_3d(T x_, T y_, T z_) { x=x_, y=y_, z=z_; }
		v_3d(const v_3d& v) { x=v.x, y=v.y, z=v.z; }

		T& operator[](int i) {
			if(i==1) return y;
			if(i==2) return z;
			return x;
		}

		const T& operator[](int i) const {
			if(i==1) return y;
			if(i==2) return z;
			return x;
		}

		v_3d& operator=(const v_3d& v)=default;

		//migrate away from these
		T dot(const v_3d& v) const { return x*v.x+y*v.y+z*v.z; }
		T mag_sq() const { return dot(*this); }
		T mag() const { return std::sqrt(mag_sq()); }

		v_3d norm() const { return operator/(mag()); }
		v_3d cross(const v_3d& v) const {
			return {
				y*v.z-z*v.y,
				z*v.x-x*v.z,
				x*v.y-y*v.x
			};
		}

		v_3d operator-() const { return {-x, -y, -z}; }
		v_3d operator+(const v_3d& v) const { return {x+v.x, y+v.y, z+v.z}; }
		v_3d operator+(const T& s) const { return operator+({s, s, s}); }
		v_3d operator-(const v_3d& v) const { return {x-v.x, y-v.y, z-v.z}; }
		v_3d operator-(const T& s) const { return operator-({s, s, s}); }
		v_3d operator*(const v_3d& v) const { return {x*v.x, y*v.y, z*v.z}; }
		v_3d operator*(const T& s) const { return operator*({s, s, s}); }
		v_3d operator/(const v_3d& v) const { return {x/v.x, y/v.y, z/v.z}; }
		v_3d operator/(const T& s) const { return operator/({s, s, s}); }

		//i never really use these, but whatever
		v_3d& operator+=(const v_3d& v) { *this=*this+v; return*this; }
		v_3d& operator+=(const T& v) { *this=*this+v; return*this; }
		v_3d& operator-=(const v_3d& v) { *this=*this-v; return*this; }
		v_3d& operator-=(const T& v) { *this=*this-v; return*this; }
		v_3d& operator*=(const v_3d& v) { *this=*this*v; return*this; }
		v_3d& operator*=(const T& v) { *this=*this*v; return*this; }
		v_3d& operator/=(const v_3d& v) { *this=*this/v; return*this; }
		v_3d& operator/=(const T& v) { *this=*this/v; return*this; }
	};

	//i like this syntax more

	template<typename T>
	T dot(const v_3d<T>& a, const v_3d<T>& b) {
		return a.x*b.x+a.y*b.y+a.z*b.z;
	}

	template<typename T>
	T length(const v_3d<T>& a) {
		return std::sqrt(dot(a, a));
	}

	template<typename T>
	v_3d<T> normalize(const v_3d<T>& a) {
		return a/length(a);
	}

	template<typename T>
	v_3d<T> cross(const v_3d<T>& a, const v_3d<T>& b) {
		return {
			a.y*b.z-a.z*b.y,
			a.z*b.x-a.x*b.z,
			a.x*b.y-a.y*b.x
		};
	}

	template<typename T> v_3d<T> operator+(const T& s, const v_3d<T>& v) { return v+s; }
	template<typename T> v_3d<T> operator-(const T& s, const v_3d<T>& v) { return -v+s; }
	template<typename T> v_3d<T> operator*(const T& s, const v_3d<T>& v) { return v*s; }
	template<typename T> v_3d<T> operator/(const T& s, const v_3d<T>& v) { return {s/v.x, s/v.y, s/v.z}; }

	typedef v_3d<float> vf3d;
	typedef v_3d<double> vd3d;
	typedef v_3d<int> vi3d;
}
#endif