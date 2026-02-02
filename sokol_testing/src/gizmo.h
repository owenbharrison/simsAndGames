#pragma once
#ifndef GIZMO_STRUCT_H
#define GIZMO_STRUCT_H

//for clamp
#include "cmn/utils.h"

//find dist between closest points
float segSkewSeg(
	const cmn::vf3d& a0, const cmn::vf3d& a1,
	const cmn::vf3d& b0, const cmn::vf3d& b1
) {
	cmn::vf3d u=a1-a0;
	cmn::vf3d v=b1-b0;
	cmn::vf3d w=a0-b0;

	float a=u.dot(u);
	float b=u.dot(v);
	float c=v.dot(v);
	float d=u.dot(w);
	float e=v.dot(w);
	float D=a*c-b*b;

	float s, t;
	if(std::abs(D)<1e-6f) s=0, t=b>c?d/b:e/c;
	else {
		s=cmn::clamp((b*e-c*d)/D, 0.f, 1.f);
		t=cmn::clamp((a*e-b*d)/D, 0.f, 1.f);
	}

	cmn::vf3d close_a=a0+s*u;
	cmn::vf3d close_b=b0+t*v;
	return (close_a-close_b).mag();
}

static cmn::vf3d segIntersectPlane(
	const cmn::vf3d& s0, const cmn::vf3d& s1,
	const cmn::vf3d& ctr, const cmn::vf3d& norm
) {
	cmn::vf3d sub=s1-s0;
	float t=norm.dot(ctr-s0)/norm.dot(sub);
	return s0+t*sub;
}

struct Gizmo {
	static const float axis_size;
	static const float margin;
	static const float plane_size;

	enum struct Mode {
		None,
		XAxis,
		YAxis,
		ZAxis,
		XYPlane,
		YZPlane,
		ZXPlane
	} mode=Mode::None;

	void beginDrag(const cmn::vf3d& g, const cmn::vf3d& s0, const cmn::vf3d& s1) {
		endDrag();

		//is mouse over axis ends?
		if(segSkewSeg(g, g+cmn::vf3d(axis_size, 0, 0), s0, s1)<margin) mode=Mode::XAxis;
		if(segSkewSeg(g, g+cmn::vf3d(0, axis_size, 0), s0, s1)<margin) mode=Mode::YAxis;
		if(segSkewSeg(g, g+cmn::vf3d(0, 0, axis_size), s0, s1)<margin) mode=Mode::ZAxis;

		//is mouse over squares?
		float u, v;
		segIntersectTri(
			s0, s1,
			g+cmn::vf3d(margin, margin, 0),
			g+cmn::vf3d(margin+plane_size, margin, 0),
			g+cmn::vf3d(margin, margin+plane_size, 0),
			&u, &v
		);
		if(u>0&&v>0&&u<1&&v<1) mode=Mode::XYPlane;
		segIntersectTri(
			s0, s1,
			g+cmn::vf3d(0, margin, margin),
			g+cmn::vf3d(0, margin+plane_size, margin),
			g+cmn::vf3d(0, margin, margin+plane_size),
			&u, &v
		);
		if(u>0&&v>0&&u<1&&v<1) mode=Mode::YZPlane;
		segIntersectTri(
			s0, s1,
			g+cmn::vf3d(margin, 0, margin),
			g+cmn::vf3d(margin, 0, margin+plane_size),
			g+cmn::vf3d(margin+plane_size, 0, margin),
			&u, &v
		);
		if(u>0&&v>0&&u<1&&v<1) mode=Mode::ZXPlane;
	}

	void updateDrag(
		cmn::vf3d& g,
		const cmn::vf3d& orig,
		const cmn::vf3d& dir,
		const cmn::vf3d& prev_dir
	) const {
		//which plane/axis to constrain motion to?
		cmn::vf3d axis, norm;
		bool constrain=false;
		switch(mode) {
			default: return;
			case Mode::XAxis: constrain=true, axis={1, 0, 0}, norm={0, 1, 0}; break;
			case Mode::YAxis: constrain=true, axis={0, 1, 0}, norm={0, 0, 1}; break;
			case Mode::ZAxis: constrain=true, axis={0, 0, 1}, norm={1, 0, 0}; break;
			case Mode::XYPlane: norm={0, 0, 1}; break;
			case Mode::YZPlane: norm={1, 0, 0}; break;
			case Mode::ZXPlane: norm={0, 1, 0}; break;
		}

		//intersect prev & curr mouse rays to plane
		cmn::vf3d curr_pt=segIntersectPlane(orig, orig+dir, g, norm);
		cmn::vf3d prev_pt=segIntersectPlane(orig, orig+prev_dir, g, norm);

		//move by delta
		cmn::vf3d delta=curr_pt-prev_pt;

		if(constrain) g+=axis.dot(delta)*axis;
		else g+=delta;
	}

	void endDrag() {
		mode=Mode::None;
	}
};

const float Gizmo::axis_size=.8f;
const float Gizmo::margin=.2f;
const float Gizmo::plane_size=.4f;
#endif