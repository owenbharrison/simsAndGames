#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include <cmath>

static constexpr float Pi=3.1415927f;

//rotation matrix
vf2d rotate(const vf2d& v, float f) {
	float c=std::cosf(f), s=std::sinf(f);
	return {
		v.x*c-v.y*s,
		v.x*s+v.y*c
	};
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

		return true;
	}
};

int main() {
	Example demo;
	if(demo.Construct(300, 300, 2, 2, false, true)) demo.Start();

	return 0;
}