#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include <cmath>

static constexpr float Pi=3.1415927f;

#include <sstream>
#include <string>
#include <iomanip>

//rotation matrix
vf2d rotate(const vf2d& v, float f) {
	float c=std::cosf(f), s=std::sinf(f);
	return {
		v.x*c-v.y*s,
		v.x*s+v.y*c
	};
}

//thanks wikipedia + pattern recognition
vf2d lineLineIntersection(
	const vf2d& a, const vf2d& b,
	const vf2d& c, const vf2d& d) {
	//get segments
	vf2d ab=a-b, ac=a-c, cd=c-d;

	//parallel
	float denom=ab.cross(cd);
	if(std::abs(denom)<1e-6f) return {-1, -1};

	//find interpolators
	return vf2d(
		ac.cross(cd),
		ac.cross(ab)
	)/denom;
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
		vf2d mouse_pos=GetMousePos();

		if(GetKey(olc::Key::A).bHeld) a=mouse_pos;
		if(GetKey(olc::Key::B).bHeld) b=mouse_pos;
		if(GetKey(olc::Key::C).bHeld) c=mouse_pos;
		if(GetKey(olc::Key::D).bHeld) d=mouse_pos;

		if(GetKey(olc::Key::ENTER).bPressed) {
			//get curr angle
			vf2d ab=b-a;
			vf2d cd=d-c;
			float curr=std::acosf(ab.dot(cd)/ab.mag()/cd.mag());

			//find difference
			static const float angle=Pi/2;//90deg
			float diff=(angle-curr)/2;

			//rot a,b about their mid
			vf2d m_ab=(a+b)/2;
			a=m_ab+rotate(a-m_ab, -diff);
			b=m_ab+rotate(b-m_ab, -diff);

			//rot c,d about their mid
			vf2d m_cd=(c+d)/2;
			c=m_cd+rotate(c-m_cd, diff);
			d=m_cd+rotate(d-m_cd, diff);
		}

		Clear(olc::GREY);

		DrawLine(a, b, olc::CYAN);
		DrawLine(c, d, olc::MAGENTA);

		static const vf2d offset(3, 3);
		FillCircle(a, 7, olc::RED);
		DrawCircle(a, 7, olc::BLACK);
		DrawString(a-offset, "A", olc::BLACK);

		FillCircle(b, 7, olc::GREEN);
		DrawCircle(b, 7, olc::BLACK);
		DrawString(b-offset, "B", olc::BLACK);

		FillCircle(c, 7, olc::BLUE);
		DrawCircle(c, 7, olc::BLACK);
		DrawString(c-offset, "C", olc::BLACK);

		FillCircle(d, 7, olc::YELLOW);
		DrawCircle(d, 7, olc::BLACK);
		DrawString(d-offset, "D", olc::BLACK);

		{
			//draw ix pt
			vf2d tu=lineLineIntersection(a, b, c, d);
			vf2d ix=a+tu.x*(b-a);

			//draw t diagram
			FillCircle(ScreenWidth()-75, ScreenHeight()-25, 25, tu.x>=0&&tu.x<=1?olc::GREEN:olc::RED);
			DrawCircle(ScreenWidth()-75, ScreenHeight()-25, 25, olc::BLACK);
			DrawString(ScreenWidth()-83, ScreenHeight()-33, "t:", olc::BLACK);
			auto t_str=std::to_string(int(100*tu.x))+"%";
			DrawString(ScreenWidth()-75-4*t_str.length(), ScreenHeight()-25, t_str, olc::BLACK);

			//draw u diagram
			FillCircle(ScreenWidth()-25, ScreenHeight()-25, 25, tu.y>=0&&tu.y<=1?olc::GREEN:olc::RED);
			DrawCircle(ScreenWidth()-25, ScreenHeight()-25, 25, olc::BLACK);
			DrawString(ScreenWidth()-33, ScreenHeight()-33, "u:", olc::BLACK);
			auto u_str=std::to_string(int(100*tu.y))+"%";
			DrawString(ScreenWidth()-25-4*u_str.length(), ScreenHeight()-25, u_str, olc::BLACK);

			//draw arc
			DrawLine(ix.x-3, ix.y, ix.x+3, ix.y, olc::BLACK);
			DrawLine(ix.x, ix.y-3, ix.x, ix.y+3, olc::BLACK);
			float sum=0;
			int num=2;
			if(tu.x<0||tu.x>1) sum+=(a-ix).mag()+(b-ix).mag(), num++;
			else if(tu.x<.5) sum+=(b-ix).mag();
			else sum+=(a-ix).mag();
			if(tu.y<0||tu.y>1) sum+=(c-ix).mag()+(d-ix).mag(), num++;
			else if(tu.y<.5) sum+=(c-ix).mag();
			else sum+=(d-ix).mag();
			float rad=sum/num;
			DrawCircle(ix, rad);

			/*
			float angle_ab=std::atan2f(a.y-ix.y, a.x-ix.x);
			float angle_cd=std::atan2f(c.y-ix.y, c.x-ix.x);
			int res=48;
			float angle=angle_ab, step=(angle_cd-angle_ab)/res;
			vf2d prev;
			for(int i=0; i<res; i++) {
				vf2d curr=ix+rad*vf2d(std::cosf(angle), std::sinf(angle));
				if(i!=0) DrawLine(prev, curr);
				prev=curr, angle+=step;
			}*/
		}

		if(false) {
			vf2d ab=b-a, cd=d-c;
			bool left=ab.x*cd.y-cd.x*ab.y>0;
			FillCircle(ScreenWidth()-50, ScreenHeight()-50, 50, left?olc::GREEN:olc::RED);
		}

		return true;
	}
};

int main() {
	Example demo;
	if(demo.Construct(300, 300, 2, 2, false, true)) demo.Start();

	return 0;
}