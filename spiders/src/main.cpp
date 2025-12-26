#define OLC_PGE_APPLICATION
#include "olc/include/olcPixelGameEngine.h"
namespace olc {
	static const Pixel PURPLE(144, 0, 255);
	static const Pixel ORANGE(255, 115, 0);
}
using olc::vf2d;

#include "cmn/utils.h"

static vf2d safeNorm(vf2d n) {
	float mag=n.mag();
	if(mag!=0) return n/mag;

	float angle=2*cmn::Pi*cmn::randFloat();
	return cmn::polar<vf2d>(1, angle);
}

struct Leg {
	vf2d start, mid, end;

	vf2d target;
};

class SpidersUI : public olc::PixelGameEngine {
	olc::Renderable prim_rect;
	
	//player stuff
	vf2d player_pos;
	vf2d player_fwd, player_rgt;
	float player_rot=0;
	
	float width=43;
	olc::Pixel width_col=olc::YELLOW;

	float length=85;
	olc::Pixel length_col=olc::RED;

	float offset=48;
	olc::Pixel offset_col=olc::WHITE;
	
	//leg stuff
	static const int num_sections=4;
	Leg legs[2*num_sections];
	
	float len1=25, len2=25;
	olc::Pixel len1_col=olc::BLUE;
	olc::Pixel len2_col=olc::PURPLE;
	
	float r_target=40;
	olc::Pixel r_target_col=olc::CYAN;

	olc::Renderable slider_tex;

	bool OnUserCreate() override {
		//setup rendering "primitive"
		prim_rect.Create(1, 1);
		prim_rect.Sprite()->SetPixel(0, 0, olc::WHITE);
		prim_rect.Decal()->Update();
		
		player_pos=.5f*GetScreenSize();

		slider_tex.Create(ScreenWidth(), ScreenHeight());

		return true;
	}

#pragma region UPDATE HELPERS
	void handleUserInput(float dt) {
		const vf2d mouse_pos=GetMousePos();

		//set pos with p key
		if(GetKey(olc::Key::P).bHeld) player_pos=mouse_pos;

		//walking
		if(GetKey(olc::Key::W).bHeld) player_pos+=75*dt*player_fwd;
		if(GetKey(olc::Key::S).bHeld) player_pos-=50*dt*player_fwd;

		//strafing
		if(GetKey(olc::Key::A).bHeld) player_pos-=60*dt*player_rgt;
		if(GetKey(olc::Key::D).bHeld) player_pos+=60*dt*player_rgt;
		
		//turning
		float turn_speed=1.4f;
		if(GetKey(olc::Key::LEFT).bHeld) player_rot-=turn_speed*dt;
		if(GetKey(olc::Key::RIGHT).bHeld) player_rot+=turn_speed*dt;
	}

	void handleLeg(Leg& l) {
		//if end too far away, goto target
		if((l.target-l.end).mag2()>r_target*r_target) l.end=l.target;

		//position mid from end given l2 constraint
		l.mid=l.end+len2*safeNorm(l.mid-l.end);

		//position mid from start given l1 constraint
		l.mid=l.start+len1*safeNorm(l.mid-l.start);
	}

	void handlePlayer() {
		player_fwd=cmn::polar<vf2d>(1, player_rot);
		player_rgt={-player_fwd.y, player_fwd.x};

		for(int i=0; i<num_sections; i++) {
			float y=length*(i/(num_sections-1.f)-.5f);
			vf2d ctr=player_pos+player_fwd*y;
			auto& left=legs[2*i], & right=legs[1+2*i];
			left.start=ctr-width/2*player_rgt;
			left.target=ctr-(width/2+offset)*player_rgt;
			right.start=ctr+width/2*player_rgt;
			right.target=ctr+(width/2+offset)*player_rgt;
		}
	}
#pragma endregion

	void update(float dt) {
		handleUserInput(dt);
		
		for(int i=0; i<2*num_sections; i++) {
			handleLeg(legs[i]);
		}

		handlePlayer();
	}

#pragma region RENDER HELPERS
	void DrawThickLineDecal(const vf2d& a, const vf2d& b, float w, olc::Pixel col) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=sub.perp()/len;

		float angle=std::atan2(sub.y, sub.x);
		DrawRotatedDecal(a-w/2*tang, prim_rect.Decal(), angle, {0, 0}, {len, w}, col);
	}
	
	void DrawCircleDecal(const vf2d& pos, float rad, const olc::Pixel& col) {
		const int num=32;
		vf2d first, prev;
		for(int i=0; i<num; i++) {
			float angle=2*cmn::Pi*i/num;
			vf2d curr=pos+cmn::polar<vf2d>(rad, angle);
			if(i==0) first=curr;
			else DrawLineDecal(prev, curr, col);
			prev=curr;
		}
		DrawLineDecal(prev, first, col);
	}
	
	void DrawArrowDecal(const vf2d& a, const vf2d& b, float sz, const olc::Pixel& col) {
		//main arm
		vf2d sub=b-a;
		vf2d c=b-sz*sub;
		DrawLineDecal(a, c, col);
		//arrow triangle
		vf2d d=.5f*sz*sub.perp();
		vf2d l=c-d, r=c+d;
		DrawLineDecal(l, r, col);
		DrawLineDecal(l, b, col);
		DrawLineDecal(r, b, col);
	}

	//handle & render a generic type slider.
	template<typename T>
	void DrawSlider(
		int x, int y, std::string title, const olc::Pixel& col,
		int width, T* val, T min, T max
	) {
		int nx=x+2+8*title.length();

		//update
		if(GetMouse(olc::Mouse::LEFT).bHeld) {
			int rx=GetMouseX()-nx;
			int ry=GetMouseY()-(y+1);
			if(rx>=0&&ry>=0&&rx<width&&ry<8) {
				float t=rx/(width-1.f);
				if(t<0) t=0;
				if(t>1) t=1;
				*val=min+t*(max-min);
			}
		}

		//render
		{
			//background
			FillRect(x, y, 3+8*title.length()+width, 10, olc::Pixel(80, 138, 255));

			//draw slid portion
			float t=float(*val-min)/(max-min);
			FillRect(nx, y+1, std::round(t*width), 8, col);
			DrawString(x+1, y+1, title, col);

			//draw value in center with at most 2 dig precision
			std::ostringstream oss;
			oss<<std::fixed<<std::setprecision(2)<<*val;
			auto val_str=oss.str();
			DrawString(nx+width/2-4*val_str.length(), y+1, val_str, olc::Pixel(35, 35, 35));
		}
	}

	//checkerboard pattern
	void renderBackground(float sz) {
		const vf2d screen_size=GetScreenSize();
		
		//approximate number of cells
		const olc::vi2d num=1+screen_size/sz;

		//true cell size
		const vf2d cell_sz=screen_size/num;

		const olc::Pixel grey1(112, 112, 112);
		const olc::Pixel grey2(138, 138, 138);
		for(int i=0; i<num.x; i++) {
			for(int j=0; j<num.y; j++) {
				vf2d pos=cell_sz*olc::vi2d(i, j);
				FillRectDecal(pos, cell_sz, (i+j)%2?grey1:grey2);
			}
		}
	}

	void renderLeg(const Leg& l) {
		DrawThickLineDecal(l.start, l.mid, 2, len1_col);
		DrawThickLineDecal(l.mid, l.end, 2, len2_col);
		DrawCircleDecal(l.target, r_target, r_target_col);
	}

	void renderPlayer() {
		DrawString(player_pos-vf2d(4, 4), "P", olc::ORANGE);
		DrawArrowDecal(
			player_pos-20*player_fwd,
			player_pos+20*player_fwd,
			.3f, olc::WHITE
		);
		vf2d fl=player_pos-width/2*player_rgt+length/2*player_fwd;
		vf2d fr=player_pos+width/2*player_rgt+length/2*player_fwd;
		vf2d bl=player_pos-width/2*player_rgt-length/2*player_fwd;
		vf2d br=player_pos+width/2*player_rgt-length/2*player_fwd;
		DrawThickLineDecal(fl, fr, 3, width_col);
		DrawThickLineDecal(bl, br, 3, width_col);
		DrawThickLineDecal(fl, bl, 3, length_col);
		DrawThickLineDecal(fr, br, 3, length_col);
	}

	void renderPlayerSliders() {
		DrawSlider<float>(
			5, 5,
			"width:", width_col, 80,
			&width, 20, 50
		);
		DrawSlider<float>(
			5, 20,
			"length:", length_col, 80,
			&length, 50, 100
		);
		DrawSlider<float>(
			5, 35,
			"offset:", offset_col, 80,
			&offset, 40, 80
		);
	}

	void renderLegSliders() {
		DrawSlider<float>(
			5, 60,
			"len1:", len1_col, 80,
			&len1, 20, 40
		);
		DrawSlider<float>(
			5, 75,
			"len2:", len2_col, 80,
			&len2, 20, 40
		);
		DrawSlider<float>(
			5, 90,
			"r_target:", r_target_col, 80,
			&r_target, 15, 80
		);
	}

	void renderSliders() {
		//render to slider_tex
		SetDrawTarget(slider_tex.Sprite());

		//transparent background
		Clear(olc::BLANK);

		renderPlayerSliders();
		
		renderLegSliders();
		
		SetDrawTarget(nullptr);

		//update decal
		slider_tex.Decal()->Update();
		
		//overlay ontop
		DrawDecal({0, 0}, slider_tex.Decal());
	}
#pragma endregion

	void render() {
		renderBackground(20);
		
		for(int i=0; i<2*num_sections; i++){
			renderLeg(legs[i]);
		}

		renderPlayer();

		renderSliders();
	}

	bool OnUserUpdate(float dt) override {
		update(dt);

		render();
		
		return true;
	}

public:
	SpidersUI() {
		sAppName="Spider Testing";
	}
};

int main() {
	SpidersUI sui;
	bool fullscreen=false;
	bool vsync=true;
	if(sui.Construct(600, 480, 1, 1, fullscreen, vsync)) sui.Start();

	return 0;
}