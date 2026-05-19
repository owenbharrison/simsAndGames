#pragma once
#ifndef CMN_V3D_STRUCT_H
#define CMN_V3D_STRUCT_H

//for sqrt & atan2
#include <cmath>

namespace cmn {
	//forward decl. b.s.
	template<typename T> struct v_3d;
	
	template<typename T>
	T dot(const v_3d<T>&, const v_3d<T>&);

	template<typename T>
	T length(const v_3d<T>&);

	template<typename T>
	v_3d<T> normalize(const v_3d<T>&);

	template<typename T>
	v_3d<T> cross(const v_3d<T>& a, const v_3d<T>&);
	
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

		//member wrappers...
		T dot(const v_3d& v) const { return cmn::dot(*this, v); }
		T mag_sq() const { return cmn::dot(*this, *this); }
		T mag() const { return cmn::length(*this); }
		v_3d norm() const { return cmn::normalize(*this); }
		v_3d cross(const v_3d& v) const { return cmn::cross(*this, v); }

		//binary operators
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

		//conversion
		template<typename O>
		operator v_3d<O>() const {
			return v_3d<O>(x, y, z);
		}

		//convert polar to cartesian
		//r y p => x y z
		//1 0 0 => 1 0 0
		static v_3d polar(const v_3d& ryp) {
			return v_3d(
				ryp.x*std::cos(ryp.y)*std::cos(ryp.z),
				ryp.x*std::sin(ryp.z),
				ryp.x*std::sin(ryp.y)*std::cos(ryp.z)
			);
		}

		//convert cartesian to polar
		//x y z => r y p
		//1 0 0 => 1 0 0
		static v_3d cartesian(const v_3d& xyz) {
			auto rad=cmn::length(xyz);
			//project onto xz
			auto yaw=std::atan2(xyz.z, xyz.x);
			//vertical triangle
			auto base=std::sqrt(xyz.x*xyz.x+xyz.z*xyz.z);
			auto pitch=std::atan2(xyz.y, base);
			return v_3d(rad, yaw, pitch);
		}
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

	//more binary operators
	template<typename T>
	v_3d<T> operator+(const T& s, const v_3d<T>& v) { return v+s; }
	template<typename T>
	v_3d<T> operator-(const T& s, const v_3d<T>& v) { return -v+s; }
	template<typename T>
	v_3d<T> operator*(const T& s, const v_3d<T>& v) { return v*s; }
	template<typename T>
	v_3d<T> operator/(const T& s, const v_3d<T>& v) { return {s/v.x, s/v.y, s/v.z}; }

	typedef v_3d<float> vf3d;
	typedef v_3d<double> vd3d;
	typedef v_3d<int> vi3d;
}
#endif