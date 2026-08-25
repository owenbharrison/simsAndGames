//todo: 
// generalize vector type
// single transform locks
#pragma once
#ifndef CAMERA_CLASS_H
#define CAMERA_CLASS_H

#include "cmn/math/v2d.h"

class Camera {
	//camera position in screen space
	cmn::vf2d offset;
	//pixels per world unit
	float zoom=1;
	//camera orientation in screen space
	float rot=0;
	//precomputed "rotation matrix"
	float sc[2]{0, 1};

	//world space transform anchors
	cmn::vf2d zoom_ctr;
	cmn::vf2d rotate_ctr;

public:
	//distance

	float wld2scr_d(float w) const {
		return w*zoom;
	}

	float scr2wld_d(float s) const {
		return s/zoom;
	}
	
	//vector

	cmn::vf2d wld2scr_v(const cmn::vf2d& w) const {
		//rotation matrix
		cmn::vf2d x(sc[1], sc[0]);
		cmn::vf2d y(-sc[0], sc[1]);
		return zoom*(w.x*x+w.y*y);
	}

	cmn::vf2d scr2wld_v(const cmn::vf2d& s) const {
		//inverse rotation matrix
		cmn::vf2d x(sc[1], -sc[0]);
		cmn::vf2d y(sc[0], sc[1]);
		return (s.x*x+s.y*y)/zoom;
	}

	//point

	cmn::vf2d wld2scr_p(const cmn::vf2d& w) const {
		return offset+wld2scr_v(w);
	}

	cmn::vf2d scr2wld_p(const cmn::vf2d& s) const {
		return scr2wld_v(s-offset);
	}

	//pass in screen delta
	void updatePan(const cmn::vf2d& s) {
		offset+=s;
	}

	//pass in screen start
	void beginZoom(const cmn::vf2d& s) {
		zoom_ctr=scr2wld_p(s);
	}

	//pass in factor
	void updateZoom(float z) {
		cmn::vf2d before=wld2scr_p(zoom_ctr);

		//apply
		zoom*=z;

		cmn::vf2d after=wld2scr_p(zoom_ctr);

		//undo shift due to zoom
		cmn::vf2d delta=after-before;
		offset-=delta;
	}

	//pass in screen start
	void beginRotate(const cmn::vf2d& s) {
		rotate_ctr=scr2wld_p(s);
	}

	//pass in delta
	void updateRotate(float r) {
		cmn::vf2d before=wld2scr_p(rotate_ctr);

		//apply
		rot+=r;
		sc[0]=std::sin(rot);
		sc[1]=std::cos(rot);

		cmn::vf2d after=wld2scr_p(rotate_ctr);

		//undo shift due to rotate
		cmn::vf2d delta=after-before;
		offset-=delta;
	}

	//screen boxes: fit a into b
	void zoomToFit(
		const cmn::vf2d& min_a, const cmn::vf2d& max_a,
		const cmn::vf2d& min_b, const cmn::vf2d& max_b
	) {
		//determine limiting dimension
		cmn::vf2d scl=(max_b-min_b)/(max_a-min_a);
		float fac=scl.x<scl.y?scl.x:scl.y;

		//box centers
		cmn::vf2d ctr_a=(min_a+max_a)/2;
		cmn::vf2d ctr_b=(min_b+max_b)/2;

		cmn::vf2d anchor=scr2wld_p(ctr_a);

		zoom*=fac;

		cmn::vf2d after=wld2scr_p(anchor);

		//make anchor(ctr_a) land on ctr_b
		offset+=ctr_b-after;
	}
};
#endif