#define OLC_PGE_APPLICATION
#include "olc/include/olcPixelGameEngine.h"
namespace olc {
	static const Pixel ORANGE(255, 127, 0);
}
using olc::vf2d;

#include "cmn/utils.h"

//for time
#include <ctime>

//for sqrt
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

//fisher-yates shuffle
template<typename T>
void shuffle(std::vector<T>& vec) {
	for(int i=vec.size()-1; i>=1; i--) {
		int j=std::rand()%(i+1);
		std::swap(vec[i], vec[j]);
	}
}

struct SketcherUI : olc::PixelGameEngine {
	SketcherUI() {
		sAppName="Sketcher Demo";
	}

	struct {
		std::list<vf2d> arr;
		vf2d* held_ref=nullptr;
		const float rad=11;

		bool to_render=false;
	} pts;

	struct Link {
		vf2d* a, * b, * c;
		float len, angle;
	};
	std::list<Link> links;

	//render stuff
	olc::Renderable prim_rect, prim_circ;

#pragma region SETUP_HELPERS
	//generalized hoberman linkage construction
	void makeHoberman(int num) {
		pts.held_ref=nullptr;

		const vf2d ctr=GetScreenSize()/2;

		//depth=3?
		auto ix=[] (int i, int j) { return 3*i+j; };

		//asymptotically decrease len w/ increasing num 
		//starting from minimum num of 4
		float min_len=60, max_len=125;
		float t=std::exp(-.4f*(num-4));
		float len=min_len+t*(max_len-min_len);

		float inc_angle=2*cmn::Pi/num;

		//allocate and insert into lookup
		pts.arr.clear();
		vf2d** grid=new vf2d*[3*num];
		for(int i=0; i<num; i++) {
			float base_angle=i*inc_angle;
			vf2d pos=ctr;
			for(int j=0; j<3; j++) {
				float angle=base_angle+j*inc_angle;
				pos+=cmn::polar<vf2d>(len, angle);
				pts.arr.push_back(pos);
				grid[ix(i, j)]=&pts.arr.back();
			}
		}

		//bottom then top
		links.clear();
		for(int i=0; i<num; i++) {
			auto& r1=grid[ix(i, 1)], & r2=grid[ix(i, 2)];
			links.push_back({grid[ix(i, 0)], r1, r2, len, inc_angle});
		}
		for(int i=0; i<num; i++) {
			auto& l1=grid[ix((i+num-1)%num, 1)], & l2=grid[ix((i+num-2)%num, 2)];
			links.push_back({grid[ix(i, 0)], l1, l2, len, -inc_angle});
		}

		delete[] grid;
	}
	
	//make some "primitives" to help draw with
	void setupRendering() {
		prim_rect.Create(1, 1);
		prim_rect.Sprite()->SetPixel(0, 0, olc::WHITE);
		prim_rect.Decal()->Update();
		
		int sz=1024;
		prim_circ.Create(sz, sz);
		SetDrawTarget(prim_circ.Sprite());
		Clear(olc::BLANK);
		FillCircle(sz/2, sz/2, sz/2);
		SetDrawTarget(nullptr);
		prim_circ.Decal()->Update();
	}
#pragma endregion

	bool OnUserCreate() override {
		std::srand(std::time(0));
		
		//dont choose 6.
		int num;
		do num=cmn::randInt(4, 10);
		while(num==6);
		makeHoberman(num);

		setupRendering();

		return true;
	}

#pragma region UPDATE HELPERS
	void handlePointMovement() {
		const vf2d mouse_pos=GetMousePos();

		const auto action=GetMouse(olc::Mouse::LEFT);
		if(action.bPressed) {
			pts.held_ref=nullptr;

			float record=-1;
			for(auto& p:pts.arr) {
				float d=(p-mouse_pos).mag();
				if(d<10) {
					if(record<0||d<record) {
						pts.held_ref=&p;
					}
				}
			}
		}
		if(action.bHeld) {
			if(pts.held_ref) *pts.held_ref=mouse_pos;
		}
		if(action.bReleased) pts.held_ref=nullptr;
	}

	void handleUserInput() {
		if(GetKey(olc::Key::K4).bPressed) makeHoberman(4);
		if(GetKey(olc::Key::K5).bPressed) makeHoberman(5);
		if(GetKey(olc::Key::K7).bPressed) makeHoberman(7);
		if(GetKey(olc::Key::K8).bPressed) makeHoberman(8);
		if(GetKey(olc::Key::K9).bPressed) makeHoberman(9);
		if(GetKey(olc::Key::K0).bPressed) makeHoberman(10);

		handlePointMovement();

		if(GetKey(olc::Key::P).bPressed) pts.to_render^=true;
	}

	//keep from overlapping
	void updatePoints() {
		//accumulate references
		std::vector<vf2d*> pts_ref;
		for(auto& p:pts.arr) pts_ref.push_back(&p);

		shuffle(pts_ref);
		
		//check against every other
		float min_dist=2*pts.rad;
		for(int i=0; i<pts_ref.size(); i++) {
			auto& a=*pts_ref[i];
			for(int j=1+i; j<pts_ref.size(); j++) {
				auto& b=*pts_ref[j];

				//push apart if too close
				float mag_sq=(b-a).mag2();
				if(mag_sq<min_dist*min_dist) {
					constrain::dist(a, b, min_dist);
				}
			}
		}
	}

	void updateLinks() {
		//accumulate references
		std::vector<Link*> links_ref;
		for(auto& l:links) links_ref.push_back(&l);

		shuffle(links_ref);

		//constrain
		for(auto& l:links_ref) {
			auto& a=*l->a, & b=*l->b;
			auto& c=*l->b, & d=*l->c;
			constrain::dist(a, b, l->len);
			constrain::dist(c, d, l->len);
			constrain::angle(a, b, c, d, l->angle);
		}
	}
#pragma endregion

	void update() {
		handleUserInput();

		for(int i=0; i<10; i++) {
			updatePoints();
			updateLinks();
		}
	}

#pragma region RENDER HELPERS
	void DrawThickLineDecal(const vf2d& a, const vf2d& b, float w, olc::Pixel col) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=(sub/len).perp();

		float angle=std::atan2(sub.y, sub.x);
		DrawRotatedDecal(a-w/2*tang, prim_rect.Decal(), angle, {0, 0}, {len, w}, col);
	}

	void FillCircleDecal(const vf2d& pos, float rad, olc::Pixel col) {
		vf2d offset(rad, rad);
		vf2d scale{2*rad/prim_circ.Sprite()->width, 2*rad/prim_circ.Sprite()->width};
		DrawDecal(pos-offset, prim_circ.Decal(), scale, col);
	}

	void renderGrid(const olc::Pixel& col) {
		const float res=25;
		const float w=3;
		const int ratio=5;

		//vertical lines
		int num_x=1+ScreenWidth()/res;
		for(int i=0; i<num_x; i++) {
			float x=res*i;
			vf2d top(x, 0), btm(x, ScreenHeight());
			if(i%ratio==0) DrawThickLineDecal(top, btm, w, col);
			else DrawLineDecal(top, btm, col);
		}

		//horizontal lines
		int num_y=1+ScreenHeight()/res;
		for(int j=0; j<num_y; j++) {
			float y=res*j;
			vf2d lft(0, y), rgt(ScreenWidth(), y);
			if(j%ratio==0) DrawThickLineDecal(lft, rgt, w, col);
			else DrawLineDecal(lft, rgt, col);
		}
	}

	void renderLinks() {
		auto renderLink=[&] (const Link& l, float w, const olc::Pixel& col) {
			FillCircleDecal(*l.a, w/2, col);
			DrawThickLineDecal(*l.a, *l.b, w, col);
			FillCircleDecal(*l.b, w/2, col);
			DrawThickLineDecal(*l.b, *l.c, w, col);
			FillCircleDecal(*l.c, w/2, col);
		};

		int i=0;
		const float w=2*pts.rad;
		for(const auto& l:links) {
			renderLink(l, w, olc::BLACK);
			
			float r, g, b;
			hash31(1000+7*(i++), r, g, b);
			renderLink(l, .8f*w, olc::PixelF(r, g, b));
		}
	}

	void renderPoints() {
		const vf2d scl=1.3f*vf2d(1, 1);

		int i=0;
		for(const auto& p:pts.arr) {
			auto str=std::to_string(i++);
			vf2d c=p-scl*vf2d(4*str.length(), 4);
			DrawStringDecal(c+scl*vf2d(1, 0), str, olc::BLACK, scl);
			DrawStringDecal(c+scl*vf2d(0, 1), str, olc::BLACK, scl);
			DrawStringDecal(c+scl*vf2d(-1, 0), str, olc::BLACK, scl);
			DrawStringDecal(c+scl*vf2d(0, -1), str, olc::BLACK, scl);

			DrawStringDecal(c, str, olc::WHITE, scl);
		}
	}
#pragma endregion

	void render() {
		//background
		FillRectDecal({0, 0}, GetScreenSize(), olc::Pixel(51, 51, 51));

		renderGrid(olc::Pixel(89, 89, 89));

		renderLinks();

		if(pts.to_render) renderPoints();
	}

	bool OnUserUpdate(float dt) {
		update();

		render();

		return true;
	}
};

int main() {
	SketcherUI sui;
	bool fullscreen=false;
	bool vsync=true;
	if(sui.Construct(720, 540, 1, 1, fullscreen, vsync)) sui.Start();

	return 0;
}