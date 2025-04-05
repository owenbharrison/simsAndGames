#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

static constexpr float Pi=3.1415927f;

//polar to cartesian helper
vf2d polar(float rad, float angle) {
	return {rad*std::cosf(angle), rad*std::sinf(angle)};
}

//clever default param placement:
//random()=0-1
//random(a)=0-a
//random(a, b)=a-b
float random(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=rand()/rand_max;
	return a+t*(b-a);
}

float clamp(float x, float a, float b) {
	if(x<a) return a;
	if(x>b) return b;
	return x;
}

//thanks wikipedia + pattern recognition
//NOTE: this returns t and u, NOT the point.
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

struct Example :olc::PixelGameEngine {
	Example() {
		sAppName="jelly car testing";
	}

	vf2d pt_orig, pt_copy;
	std::vector<vf2d> shape_orig, shape_copy;

	bool collide=false;

	bool OnUserCreate() override {
		return true;
	}

	bool OnUserUpdate(float dt) override {
		const vf2d mouse_pos=GetMousePos();

		//set point
		if(GetKey(olc::Key::P).bHeld) {
			pt_orig=mouse_pos;
			collide=false;
		}

		//draw shape
		if(GetMouse(olc::Mouse::LEFT).bPressed) {
			shape_orig.emplace_back(mouse_pos);
			collide=false;
		}

		//clear shape
		if(GetKey(olc::Key::HOME).bPressed) {
			shape_orig.clear();
			collide=false;
		}

		//exit collide
		if(GetKey(olc::Key::ESCAPE).bPressed) collide=false;

		//RENDER
		Clear(olc::GREY);

		//show collide background
		if(collide) DrawRect(0, 0, ScreenWidth()-1, ScreenHeight()-1, olc::RED);

		auto& said_pt=collide?pt_copy:pt_orig;
		auto& said_shape=collide?shape_copy:shape_orig;

		//show point
		DrawString(said_pt.x-4, said_pt.y-4, "P");

		//shape outlines
		for(int i=0; i<said_shape.size(); i++) {
			DrawLine(said_shape[i], said_shape[(i+1)%said_shape.size()]);
		}

		//check if inside
		bool inside=false;
		{
			vf2d dir=polar(1, random(2*Pi));
			int num_ix=0;
			for(int i=0; i<said_shape.size(); i++) {
				const auto& a=said_shape[i];
				const auto& b=said_shape[(i+1)%said_shape.size()];
				vf2d tu=lineLineIntersection(a, b, said_pt, said_pt+dir);
				if(tu.x>0&&tu.x<1&&tu.y>0) num_ix++;
			}
			inside=num_ix%2;
		}
		if(inside&&!collide) {
			//find close segment
			int idx=-1;
			float record;
			for(int i=0; i<said_shape.size(); i++) {
				const auto& a=said_shape[i];
				const auto& b=said_shape[(i+1)%said_shape.size()];
				vf2d pa=said_pt-a, ba=b-a;
				float t=clamp(pa.dot(ba)/ba.dot(ba), 0, 1);
				vf2d close_pt=a+t*ba;
				float dist=(close_pt-said_pt).mag();
				if(idx<0||dist<record) {
					record=dist;
					idx=i;
				}
			}

			//show close segment
			const auto& a=said_shape[idx];
			const auto& b=said_shape[(idx+1)%said_shape.size()];
			DrawLine(said_shape[idx], said_shape[(idx+1)%said_shape.size()], olc::GREEN);

			//show close pt
			vf2d pa=said_pt-a, ba=b-a;
			float t=clamp(pa.dot(ba)/ba.dot(ba), 0, 1);
			vf2d close_pt=a+t*ba;
			DrawString(close_pt.x-4, close_pt.y-4, "C", olc::BLUE);
		}

		//show vertexes
		for(int i=0; i<said_shape.size(); i++) {
			const auto& s=said_shape[i];
			auto str=std::to_string(i);
			DrawString(s.x-4*str.size(), s.y-4, str, olc::BLACK);
		}

		//run collision
		if(inside&&GetKey(olc::Key::ENTER).bPressed) {
			collide=true;

			//copy config
			pt_copy=pt_orig;
			shape_copy=shape_orig;

			{//ensure winding
				float area=0;
				for(int i=0; i<shape_copy.size(); i++) {
					const auto& a=shape_copy[i];
					const auto& b=shape_copy[(i+1)%shape_copy.size()];
					area+=a.x*b.y-a.y*b.x;
				}
				if(area<0) std::reverse(shape_copy.begin(), shape_copy.end());
			}

			//find closest segment
			float record;
			int said_idx=-1;
			float said_t;
			vf2d said_sub;
			for(int i=0; i<shape_copy.size(); i++) {
				const auto& a=shape_copy[i];
				const auto& b=shape_copy[(i+1)%shape_copy.size()];
				vf2d pa=pt_copy-a, ba=b-a;
				float t=clamp(pa.dot(ba)/ba.dot(ba), 0, 1);
				vf2d close_pt=a+t*ba;
				vf2d sub=close_pt-pt_copy;
				float dist=sub.mag();
				if(said_idx<0||dist<record) {
					record=dist;
					said_idx=i;
					said_t=t;
					said_sub=sub;
				}
			}

			//get close segment
			auto& a=shape_copy[said_idx];
			float ma=1, mb=ma;
			auto& b=shape_copy[(said_idx+1)%shape_copy.size()];
			float mp=1;

			//inverse mass weighting
			float m1s=1/(ma+mb);
			float m1p=1/mp;
			float m1t=m1s+m1p;

			//find contributions
			float a_cont=(1-said_t)*m1s/m1t;
			float b_cont=said_t*m1s/m1t;
			float p_cont=m1p/m1t;

			//updating
			a-=a_cont*said_sub;
			b-=b_cont*said_sub;
			pt_copy+=p_cont*said_sub;
		}

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(320, 240, 2, 2, false, true)) e.Start();

	return 0;
}