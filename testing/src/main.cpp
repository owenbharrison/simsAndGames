#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include <string>
#include <vector>

void getStringSize(const std::string& str, int& w, int& h) {
	w=0, h=0;
	int ox=0, oy=0;
	for(const auto& c:str) {
		if(c==' ') ox++;
		else if(c=='\n') ox=0, oy++;
		else if(c>='!'&&c<='~') {
			ox++;
			w=std::max(w, ox);
			h=std::max(h, 1+oy);
		}
	}
};

struct Menu {
	std::string title;
	std::vector<std::string> options;
};

#include "common/aabb.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

class Example : public olc::PixelGameEngine {
	int margin=50;
	float scl_title=1.2f;

	Menu main, diff;

	vf2d pt_a, pt_b;

public:
	Example() {
		sAppName="ui maths";
	}

public:
	bool OnUserCreate() override {
		main={"Main", {
			"Start",
			"Difficulty",
			"Tutorial",
			"Settings",
			"Quit"
		}};

		diff={"Difficulty", {
			"Beginner",
			"Normal",
			"Hard"
			"Expert",
			"back"
		}};

		return true;
	}

	void renderMenu(const Menu& m, const cmn::AABB& box) {
		const vf2d sz_box=box.max-box.min;
		
		int w_title=0, h_title=0;
		getStringSize(m.title, w_title, h_title);

		float char_h=scl_title*h_title;
		for(const auto& o:m.options) {
			int w=0, h=0;
			getStringSize(o, w, h);
			char_h+=h;
		}
		float scl_y=(sz_box.y-3*margin)/(8*char_h);

		float char_w=scl_title*w_title;
		for(const auto& o:m.options) {
			int w=0, h=0;
			getStringSize(o, w, h);
			char_w=std::max(char_w, float(w));
		}
		float scl_x=(sz_box.x-2*margin)/(8*char_w);

		float scl=std::min(scl_x, scl_y);

		float cx=(box.min.x+box.max.x)/2;

		float y=box.min.y+margin;
		DrawStringDecal(
			vf2d(cx-4*w_title*scl_title*scl,
			y),
			m.title,
			olc::WHITE,
			scl_title*scl*vf2d(1, 1)
		);
		y+=8*h_title*scl_title*scl;

		y+=margin;

		for(int i=0; i<m.options.size(); i++) {
			const auto& o=m.options[i];
			int w=0, h=0;
			getStringSize(o, w, h);
			DrawStringDecal(
				vf2d(cx-4*w*scl,
				y),
				o,
				olc::WHITE,
				scl*vf2d(1, 1)
			);
			y+=8*h*scl;
		}
	}

	bool OnUserUpdate(float dt) override {
		const vf2d mouse_pos=GetMousePos();

		if(GetKey(olc::Key::M).bHeld) margin=mouse_pos.y;
		if(GetKey(olc::Key::A).bHeld) pt_a=mouse_pos;
		if(GetKey(olc::Key::B).bHeld) pt_b=mouse_pos;

		cmn::AABB box;
		box.fitToEnclose(pt_a);
		box.fitToEnclose(pt_b);

		renderMenu(main, box);

		//draw box
		DrawRectDecal(box.min, box.max-box.min, olc::RED);

		//draw margin box
		DrawRectDecal(box.min+margin, box.max-box.min-2*margin, olc::GREEN);

		return true;
	}
};

int main() {
	Example demo;
	bool fullscreen=false;
	bool vsync=true;
	if(demo.Construct(800, 600, 1, 1, fullscreen, vsync)) demo.Start();

	return 0;
}