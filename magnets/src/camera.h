#pragma once
#ifndef CAMERA_CLASS_H
#define CAMERA_CLASS_H

#include "cmn/math/v2d.h"

class Camera {
	//world space
	cmn::vf2d offset;
	//pixels per unit
	float scale=1;

	cmn::vf2d prev_pan;

	void scaleBy(float f);

public:
	cmn::vf2d scr_sz;

	cmn::vf2d wld2scr(const cmn::vf2d& w) const {
		return scr_sz/2+scale*(w-offset);
	}

	cmn::vf2d scr2wld(const cmn::vf2d& s) const {
		return offset+(s-scr_sz/2)/scale;
	}

	float wld2scr(float w) const {
		return scale*w;
	}

	float scr2wld(float s) const {
		return s/scale;
	}

	//fit world space box into screen with spacing
	void zoomToFit(const cmn::vf2d& min, const cmn::vf2d& max, float margin) {
		//in screen space
		float w_box=wld2scr(max.x-min.x);
		float h_box=wld2scr(max.y-min.y);

		//actual screen box
		float w_scr=scr_sz.x-2*margin;
		float h_scr=scr_sz.y-2*margin;

		//find aspect ratio
		float asp_x=w_scr/w_box;
		float asp_y=h_scr/h_box;
		
		//scale based on limiting dimension
		scaleBy(asp_x<asp_y?asp_x:asp_y);

		//world space box center
		cmn::vf2d box_ctr=(min+max)/2;
		//world space screen center
		cmn::vf2d scr_ctr=scr2wld(scr_sz/2);

		//make them overlap
		cmn::vf2d delta=scr_ctr-box_ctr;
		offset-=delta;
	}

	void begin_pan(const cmn::vf2d& p) {
		prev_pan=p;
	}

	void update_pan(const cmn::vf2d& p) {
		cmn::vf2d delta=scr2wld(p)-scr2wld(prev_pan);
		offset-=delta;

		prev_pan=p;
	}

	void update_zoom(const cmn::vf2d& z, float f) {
		//wld mouse before zoom
		cmn::vf2d before=scr2wld(z);

		//apply zoom
		scaleBy(f);

		//wld mouse after zoom
		cmn::vf2d after=scr2wld(z);

		//pan back so wld mouse stays fixed
		cmn::vf2d delta=after-before;
		offset-=delta;
	}
};

void Camera::scaleBy(float f) {
	const float min_scl=1e-6f;
	const float max_scl=1e6f;

	scale*=f;

	if(scale<min_scl) scale=min_scl;
	if(scale>max_scl) scale=max_scl;
}
#endif