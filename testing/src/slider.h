#pragma once
#ifndef SLIDER_CLASS_H
#define SLIDER_CLASS_H

#include <functional>

struct Slider {
	olc::vf2d a, b;
	float rad=1;
	std::function<void(float)> callback;
	
	float t=0;
	bool holding=false;

	void startSlide(const olc::vf2d& p) {
		//get close pt
		olc::vf2d ba=b-a, pa=p-a;
		float new_t=ba.dot(pa)/ba.dot(ba);
		new_t=std::max(0.f, std::min(1.f, new_t));
		olc::vf2d close_pt=a+new_t*ba;
		if((close_pt-p).mag()<rad) {
			t=new_t;
			callback(t);
			holding=true;
		}
	}

	void updateSlide(const olc::vf2d& p) {
		//get close pt
		olc::vf2d ba=b-a, pa=p-a;
		float new_t=ba.dot(pa)/ba.dot(ba);
		new_t=std::max(0.f, std::min(1.f, new_t));
		if(holding) {
			t=new_t;
			callback(t);
		}
	}

	void endSlide() {
		holding=false;
	}

	void draw(olc::PixelGameEngine* pge) const {
		pge->DrawLine(a, b, olc::BLACK);
		olc::vf2d ba=b-a, pt=a+t*ba;
		pge->FillCircle(pt, rad, olc::WHITE);
		pge->DrawCircle(pt, rad, olc::BLACK);
	}
};
#endif