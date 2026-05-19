#pragma once
#ifndef CMN_V2D_STRUCT_H
#define CMN_V2D_STRUCT_H

//for sqrt & atan2
#include <cmath>

namespace cmn {
	template<typename T>
	struct v_2d {
		T x=0, y=0;

		v_2d() {}
		v_2d(T x_, T y_) { x=x_, y=y_; }
		v_2d(const v_2d& v) { x=v.x, y=v.y; }

		T& operator[](int i) {
			if(i==1) return y;
			return x;
		}

		const T& operator[](int i) const {
			if(i==1) return y;
			return x;
		}

		v_2d& operator=(const v_2d& v)=default;

		T dot(const v_2d& v) const { return dot(*this, v); }
		T mag_sq() const { return dot(*this, *this); }
		T mag() const { return length(*this); }

		v_2d norm() const { return normalize(*this); }

		v_2d operator-() const { return {-x, -y}; }
		v_2d operator+(const v_2d& v) const { return {x+v.x, y+v.y}; }
		v_2d operator+(const T& s) const { return operator+({s, s}); }
		v_2d operator-(const v_2d& v) const { return {x-v.x, y-v.y}; }
		v_2d operator-(const T& s) const { return operator-({s, s}); }
		v_2d operator*(const v_2d& v) const { return {x*v.x, y*v.y}; }
		v_2d operator*(const T& s) const { return operator*({s, s}); }
		v_2d operator/(const v_2d& v) const { return {x/v.x, y/v.y}; }
		v_2d operator/(const T& s) const { return operator/({s, s}); }

		//i never really use these, but whatever
		v_2d& operator+=(const v_2d& v) { *this=*this+v; return*this; }
		v_2d& operator+=(const T& v) { *this=*this+v; return*this; }
		v_2d& operator-=(const v_2d& v) { *this=*this-v; return*this; }
		v_2d& operator-=(const T& v) { *this=*this-v; return*this; }
		v_2d& operator*=(const v_2d& v) { *this=*this*v; return*this; }
		v_2d& operator*=(const T& v) { *this=*this*v; return*this; }
		v_2d& operator/=(const v_2d& v) { *this=*this/v; return*this; }
		v_2d& operator/=(const T& v) { *this=*this/v; return*this; }

		//conversion
		template<typename O>
		operator v_2d<O>() const {
			return v_2d<O>(x, y);
		}

		//convert polar to cartesian
		static v_2d polar(const v_2d& rt) {
			return v_2d(
				rt.x*std::cos(rt.y),
				rt.x*std::sin(rt.y)
			);
		}

		//convert cartesian to polar
		static v_2d cartesian(const v_2d& xy) {
			auto rad=length(xy);
			auto theta=std::atan2(xy.y, xy.x);
			return v_2d(rad, theta);
		}
	};

	//i like this syntax more

	template<typename T>
	T dot(const v_2d<T>& a, const v_2d<T>& b) {
		return a.x*b.x+a.y*b.y;
	}

	template<typename T>
	T length(const v_2d<T>& a) {
		return std::sqrt(dot(a, a));
	}

	template<typename T>
	v_2d<T> normalize(const v_2d<T>& a) {
		return a/length(a);
	}

	template<typename T> v_2d<T> operator+(const T& s, const v_2d<T>& v) { return v+s; }
	template<typename T> v_2d<T> operator-(const T& s, const v_2d<T>& v) { return -v+s; }
	template<typename T> v_2d<T> operator*(const T& s, const v_2d<T>& v) { return v*s; }
	template<typename T> v_2d<T> operator/(const T& s, const v_2d<T>& v) { return {s/v.x, s/v.y}; }

	typedef v_2d<float> vf2d;
	typedef v_2d<double> vd2d;
	typedef v_2d<int> vi2d;
}
#endif