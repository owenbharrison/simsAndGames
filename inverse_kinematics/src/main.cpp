#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "common/utils.h"
namespace cmn {
	vf2d polar(float rad, float angle) {
		return polar_generic<vf2d>(rad, angle);
	}
}

void update(vf2d& a, vf2d& b, float len) {
	vf2d sub=a-b;
	float curr=sub.mag();

	//dont divide by 0
	vf2d norm;
	if(curr<1e-6f) norm=cmn::polar(1, cmn::random(2*cmn::Pi));
	else norm=sub/curr;

	//find change
	float delta=len-curr;
	vf2d diff=.5f*delta*norm;

	//update
	a+=diff;
	b-=diff;
}

#include "arm.h"

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Example";
	}

	vf2d mouse_pos;

	Arm arm;

	bool OnUserCreate() override {
		vf2d ctr(ScreenWidth()/2, ScreenHeight()/2);
		arm=Arm(4, ctr, mouse_pos, 50, 0);
		
		return true;
	}

	bool OnUserUpdate(float dt) override {
		mouse_pos=GetMousePos();
		
		arm.update(mouse_pos);

		//render
		Clear(olc::GREY);

		//chain
		float rad=4;
		for(int i=0; i<arm.getNum(); i++) {
			//draw circle
			vf2d& curr=arm.pts[i].pos;
			DrawCircle(curr, rad);
			
			//tangent lines
			if(i>0) {
				vf2d& prev=arm.pts[i-1].pos;
				vf2d norm=(curr-prev).norm();
				vf2d tang(-norm.y, norm.x);
				DrawLine(curr+rad*tang, prev+rad*tang);
				DrawLine(curr-rad*tang, prev-rad*tang);
			}

			//label
			auto str=std::string(1, 'A'+i);
			DrawString(curr-vf2d(4, 4), str, olc::RED);
		}

		return true;
	}
};

int main() {
	Example demo;
	if(demo.Construct(300, 300, 2, 2, false, true)) demo.Start();

	return 0;
}