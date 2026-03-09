#define OLC_PGE_APPLICATION
#include "olc/include/olcPixelGameEngine.h"
namespace olc {
	static const Pixel ORANGE(255, 127, 0);
}
using olc::vf2d;

#include "cmn/utils.h"

#include <cmath>
#include <string>
#include <list>
#include <vector>

#include "constraints.h"

static float degToRad(float deg) {
	return deg/180*cmn::Pi;
}

static float radToDeg(float rad) {
	return rad/cmn::Pi*180;
}

static float fract(float x) {
	return x-std::floor(x);
}

//https://www.shadertoy.com/view/4djSRW
void hash31(float i, float& a, float& b, float& c) {
	float x=fract(.1031*i);
	float y=fract(.1030*i);
	float z=fract(.0973*i);
	float d=x*(33.33+y)+y*(33.33+z)+z*(33.33+x);
	x+=d, y+=d, z+=d;
	a=fract(z*(x+y));
	b=fract(y*(z+x));
	c=fract(x*(y+z));
}

struct SketcherUI : olc::PixelGameEngine {
	SketcherUI() {
		sAppName="Sketcher Demo";
	}

	std::list<vf2d> pts;
	vf2d* held_pt=nullptr;

	struct Link {
		vf2d* a, * b, * c;
		float len, angle;
	};
	std::list<Link> links;

	void reset(int num) {
		held_pt=nullptr;
		
		const vf2d ctr=GetScreenSize()/2;

		//depth=3?
		auto ix=[] (int i, int j) { return 3*i+j; };

		float len=50;

		float inc_angle=2*cmn::Pi/num;

		pts.clear();
		vf2d** grid=new vf2d*[3*num];
		for(int i=0; i<num; i++) {
			float base_angle=i*inc_angle;
			vf2d pos=ctr;
			for(int j=0; j<3; j++) {
				float angle=base_angle+j*inc_angle;
				pos+=cmn::polar<vf2d>(len, angle);
				pts.push_back(pos);
				grid[ix(i, j)]=&pts.back();
			}
		}

		//going right & left
		links.clear();
		for(int i=0; i<num; i++) {
			auto& c=grid[ix(i, 0)];

			auto& r1=grid[ix(i, 1)], & r2=grid[ix(i, 2)];
			links.push_back({c, r1, r2, len, inc_angle});

			auto& l1=grid[ix((i+num-1)%num, 1)], & l2=grid[ix((i+num-2)%num, 2)];
			links.push_back({c, l1, l2, len, -inc_angle});
		}

		delete[] grid;
	}

	bool OnUserCreate() override {
		reset(6);

		return true;
	}

	void handlePoints() {
		const vf2d mouse_pos=GetMousePos();
		
		const auto action=GetMouse(olc::Mouse::LEFT);
		if(action.bPressed) {
			held_pt=nullptr;

			float record=-1;
			for(auto& p:pts) {
				float d=(p-mouse_pos).mag();
				if(d<10){
					if(record<0||d<record) {
						held_pt=&p;
					}
				}
			}
		}
		if(action.bHeld) {
			if(held_pt) *held_pt=mouse_pos;
		}
		if(action.bReleased) held_pt=nullptr;
	}

	//iterate randomly?
	void updateLinks() {
		//accumulate references
		std::vector<Link*> ptrs;
		for(auto& l:links) ptrs.push_back(&l);

		//fisher-yates shuffle
		for(int i=ptrs.size()-1; i>=1; i--) {
			int j=std::rand()%(i+1);
			std::swap(ptrs[i], ptrs[j]);
		}

		//constrain
		for(auto& l:ptrs) {
			auto& a=*l->a, & b=*l->b;
			auto& c=*l->b, & d=*l->c;
			constrain::dist(a, b, l->len);
			constrain::dist(c, d, l->len);
			constrain::angle(a, b, c, d, l->angle);
		}
	}

	void renderLinks() {
		int i=0;
		for(const auto& l:links) {
			float r, g, b;
			hash31(1000+7*i, r, g, b);

			auto col=olc::PixelF(r, g, b);
			DrawLine(*l.a, *l.b, col);
			DrawLine(*l.b, *l.c, col);

			i++;
		}
	}

	void renderPoints() {
		int i=0;
		for(const auto& p:pts) {
			auto str=std::to_string(i);
			vf2d c=p-vf2d(4*str.length(), 4);
			DrawString(c+vf2d(1, 0), str, olc::BLACK);
			DrawString(c+vf2d(0, 1), str, olc::BLACK);
			DrawString(c+vf2d(-1, 0), str, olc::BLACK);
			DrawString(c+vf2d(0, -1), str, olc::BLACK);
			
			float r, g, b;
			hash31(7*i, r, g, b);
			DrawString(c, str, olc::PixelF(r, g, b));

			i++;
		}
	}

	bool OnUserUpdate(float dt) {
		if(GetKey(olc::Key::K3).bPressed) reset(3);
		if(GetKey(olc::Key::K4).bPressed) reset(4);
		if(GetKey(olc::Key::K5).bPressed) reset(5);
		if(GetKey(olc::Key::K6).bPressed) reset(6);
		if(GetKey(olc::Key::K7).bPressed) reset(7);
		if(GetKey(olc::Key::K8).bPressed) reset(8);
		if(GetKey(olc::Key::K9).bPressed) reset(9);
		if(GetKey(olc::Key::K0).bPressed) reset(10);
		if(GetKey(olc::Key::K1).bPressed) reset(11);
		if(GetKey(olc::Key::K2).bPressed) reset(12);
		
		handlePoints();
		
		for(int i=0; i<100; i++) updateLinks();
		
		Clear(olc::VERY_DARK_GREY);

		renderLinks();

		renderPoints();

		return true;
	}
};

int main() {
	SketcherUI sui;
	bool fullscreen=false;
	bool vsync=true;
	if(sui.Construct(640, 480, 1, 1, fullscreen, vsync)) sui.Start();

	return 0;
}