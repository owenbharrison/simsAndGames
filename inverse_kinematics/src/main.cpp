#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

//clever default param placement:
//random()=0-1
//random(a)=0-a
//random(a, b)=a-b
float random(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=rand()/rand_max;
	return a+t*(b-a);
}

constexpr float Pi=3.1415927f;

vf2d polar(float rad, float angle) {
	return {rad*std::cosf(angle), rad*std::sinf(angle)};
}

void update(vf2d& a, vf2d& b, float len) {
	vf2d sub=a-b;
	float curr=sub.mag();

	//dont divide by 0
	vf2d norm;
	if(curr<1e-6f) norm=polar(1, random(2*Pi));
	else norm=sub/curr;

	//find change
	float delta=len-curr;
	vf2d diff=.5f*delta*norm;

	//update
	a+=diff;
	b-=diff;
}

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Example";
	}

	vf2d a, b, c, d;

	bool OnUserCreate() override {
		return true;
	}

	bool OnUserUpdate(float dt) override {
		//anchors
		a=GetMousePos();
		d=vf2d(ScreenWidth()/2, ScreenHeight()/2);

		//chain update
		update(a, b, 50);
		update(b, c, 50);
		update(c, d, 50);

		//render
		Clear(olc::GREY);

		//chain
		float rad=4;
		vf2d n_ab=(a-b).norm();
		vf2d t_ab(-n_ab.y, n_ab.x);
		DrawLine(a+rad*t_ab, b+rad*t_ab);
		DrawLine(a-rad*t_ab, b-rad*t_ab);

		vf2d n_bc=(b-c).norm();
		vf2d t_bc(-n_bc.y, n_bc.x);
		DrawLine(b+rad*t_bc, c+rad*t_bc);
		DrawLine(b-rad*t_bc, c-rad*t_bc);
		
		vf2d n_cd=(c-d).norm();
		vf2d t_cd(-n_cd.y, n_cd.x);
		DrawLine(c+rad*t_cd, d+rad*t_cd);
		DrawLine(c-rad*t_cd, d-rad*t_cd);

		//point
		DrawCircle(a, rad);
		DrawCircle(b, rad);
		DrawCircle(c, rad);
		DrawCircle(d, rad);

		//labels
		DrawString(a-vf2d(4, 4), "A", olc::RED);
		DrawString(b-vf2d(4, 4), "B", olc::BLACK);
		DrawString(c-vf2d(4, 4), "C", olc::BLACK);
		DrawString(d-vf2d(4, 4), "D", olc::RED);

		return true;
	}
};

int main() {
	Example demo;
	if(demo.Construct(300, 300, 2, 2, false, true)) demo.Start();

	return 0;
}