//this is just a wrapper class
#pragma once
#ifndef MY_PIXEL_GAME_ENGINE_CLASS_H
#define MY_PIXEL_GAME_ENGINE_CLASS_H

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

class MyPixelGameEngine : public olc::PixelGameEngine {
	olc::Sprite* _prim_rect_sprite=nullptr;
	olc::Decal* _prim_rect_decal=nullptr;
	const unsigned int _prim_circle_width=1024;
	olc::Sprite* _prim_circle_sprite=nullptr;
	olc::Decal* _prim_circle_decal=nullptr;
public:

	olc::Decal* getCircleDecal() const {
		return _prim_circle_decal;
	}

	olc::Decal* getRectDecal() const {
		return _prim_rect_decal;
	}

	virtual bool on_init()=0;

	bool OnUserCreate() final {
		_prim_rect_sprite=new olc::Sprite(1, 1);
		_prim_rect_sprite->SetPixel(0, 0, olc::WHITE);
		_prim_rect_decal=new olc::Decal(_prim_rect_sprite);

		//make a circle to draw with
		const unsigned int rad_sq=_prim_circle_width*_prim_circle_width/4;
		_prim_circle_sprite=new olc::Sprite(_prim_circle_width, _prim_circle_width);
		for(int x=0; x<_prim_circle_width; x++) {
			int dx=x-_prim_circle_width/2;
			for(int y=0; y<_prim_circle_width; y++) {
				int dy=y-_prim_circle_width/2;
				_prim_circle_sprite->SetPixel(x, y, dx*dx+dy*dy<rad_sq?olc::WHITE:olc::BLANK);
			}
		}
		_prim_circle_decal=new olc::Decal(_prim_circle_sprite);

		return on_init();
	}

	virtual bool on_exit()=0;

	bool OnUserDestroy() final {
		delete _prim_rect_decal;
		delete _prim_rect_sprite;
		delete _prim_circle_decal;
		delete _prim_circle_sprite;

		return on_exit();
	}

	void DrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float rad, olc::Pixel col=olc::WHITE) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=(sub/len).perp();

		float angle=atan2f(sub.y, sub.x);
		DrawRotatedDecal(a-rad*tang, _prim_rect_decal, angle, {0, 0}, {len, 2*rad}, col);
	}

	void FillCircleDecal(const olc::vf2d& pos, float rad, olc::Pixel col=olc::WHITE) {
		olc::vf2d offset(rad, rad);
		olc::vf2d scale{2*rad/_prim_circle_width, 2*rad/_prim_circle_width};
		DrawDecal(pos-offset, _prim_circle_decal, scale, col);
	}

	virtual bool on_update(float dt)=0;
	virtual bool on_render()=0;

	bool OnUserUpdate(float dt) final {
		if(!on_update(dt)) return false;
		return on_render();
	}
};
#endif//MY_PIXEL_GAME_ENGINE_CLASS_H