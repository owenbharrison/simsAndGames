#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#include <string>

#include <vector>

struct Slider {
	int x=0, y=0;
	std::string title;
	int w=0;
	float val=0, min=0, max=0;
	olc::Pixel col=olc::WHITE;

	bool contains(int px, int py) const {
		int nx=x+2+8*title.length();
		px-=nx;
		py-=y+1;
		return px>=0&&py>=0&&px<w&&py<8;
	};

	void render(olc::PixelGameEngine* pge) const {
		//background
		pge->FillRect(x, y, 3+8*title.length()+w, 10, olc::BLACK);

		//draw slid portion
		int nx=x+2+8*title.length();
		float t=(val-min)/(max-min);
		pge->FillRect(nx, y+1, std::round(t*w), 8, col);
		pge->DrawString(x+1, y+1, title, col);

		//draw value in center
		//  w/ 2 digits after decimal
		int i_part=val;
		int f_part=100*(val-i_part);
		auto i_str=std::to_string(i_part);
		auto val_str=i_str+'.'+char('0'+f_part/10)+char('0'+f_part%10);
		pge->DrawString(nx+w/2-4*val_str.length(), y+1, val_str, olc::WHITE);
	}
};

class Example : public olc::PixelGameEngine {
	std::list<Slider> sliders;
	Slider* selected_slider=nullptr;

	bool OnUserCreate() override {
		sliders.push_back({5, 5, "size x(mm)", 50, 7.8f, 4, 15.6f, olc::RED});
		sliders.push_back({5, 20, "size y(mm)", 50, 9.6f, 4, 15.6f, olc::BLUE});
		sliders.push_back({5, 35, "size z(mm)", 50, 7.8f, 4, 15.6f, olc::GREEN});

		return true;
	}

	void handleSlideActionBegin() {
		handleSlideActionEnd();

		int mx=GetMouseX(), my=GetMouseY();
		for(auto& s:sliders) {
			if(s.contains(mx, my)) {
				selected_slider=&s;
				break;
			}
		}
	}

	void handleSlideActionUpdate() {
		if(!selected_slider) return;

		auto& s=*selected_slider;
		int nx=s.x+2+8*s.title.length();
		float t=(GetMouseX()-nx)/(s.w-1.f);
		if(t<0) t=0;
		if(t>1) t=1;
		s.val=s.min+t*(s.max-s.min);
	}

	void handleSlideActionEnd() {
		selected_slider=nullptr;
	}

	void update(float dt) {
		//slider detection
		const auto slide_action=GetMouse(olc::Mouse::LEFT);
		if(slide_action.bPressed) handleSlideActionBegin();
		if(slide_action.bHeld) handleSlideActionUpdate();
		if(slide_action.bReleased) handleSlideActionEnd();
	}

	void render() {
		//checkerboard background
		for(int i=0; i<ScreenWidth(); i++) {
			for(int j=0; j<ScreenHeight(); j++) {
				Draw(i, j, (i+j)%2?olc::GREY:olc::WHITE);
			}
		}

		for(const auto& s:sliders) {
			s.render(this);
		}
	}

	bool OnUserUpdate(float dt) override {
		update(dt);
		
		render();

		return true;
	}
};

int main() {
	Example demo;
	bool fullscreen=false;
	bool vsync=true;
	if(demo.Construct(200, 150, 4, 4, fullscreen, vsync)) demo.Start();

	return 0;
}