#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_SOUNDWAVE
#include "olcSoundWaveEngine.h"

constexpr float Pi=3.1415927f;

class Example : public olc::PixelGameEngine {
	//"primitive" render helpers
	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;
	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;

	enum struct LeaderStage {
		Slideshow,
		Countdown,
		Burnout,
		Glitch
	} leader_stage=LeaderStage::Slideshow;

	std::vector<olc::Renderable> leader_slideshow;
	int leader_slideshow_ix=0;
	const float leader_slideshow_time=.1667f;

	int leader_count=0;
	const float leader_count_time=.75f;

	float leader_timer=leader_slideshow_time;

	olc::sound::WaveEngine sound_engine;
	olc::sound::Wave leader_low_beep, leader_high_beep;
public:
	Example() {
		sAppName="glitch demo";
	}

public:
	bool OnUserCreate() override {
		//make some "primitives" to draw with
		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);
		{
			int sz=1024;
			prim_circ_spr=new olc::Sprite(sz, sz);
			SetDrawTarget(prim_circ_spr);
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2);
			SetDrawTarget(nullptr);
			prim_circ_dec=new olc::Decal(prim_circ_spr);
		}

		//load leader slideshow
		for(int i=1; i<11; i++) {
			std::string filename="assets/leader/"+std::to_string(i)+".png";
			leader_slideshow.push_back({});
			leader_slideshow.back().Load(filename);
		}

		//load audio
		sound_engine.InitialiseAudio();
		sound_engine.SetOutputVolume(.3f);
		if(!leader_low_beep.LoadAudioWaveform("assets/leader/low.wav")) {
			std::cout<<"error loading audio\n";
			return false;
		}
		if(!leader_high_beep.LoadAudioWaveform("assets/leader/high.wav")) {
			std::cout<<"error loading audio\n";
			return false;
		}

		return true;
	}

	bool OnUserDestroy() override {
		delete prim_rect_dec;
		delete prim_rect_spr;
		delete prim_circ_dec;
		delete prim_circ_spr;
		
		return true;
	}

	bool update(float dt) {
		leader_timer-=dt;
		switch(leader_stage) {
			case LeaderStage::Slideshow:
				if(leader_timer<0) {
					leader_timer+=leader_slideshow_time;

					leader_slideshow_ix++;

					//send to countdown...
					if(leader_slideshow_ix==leader_slideshow.size()) {
						leader_stage=LeaderStage::Countdown;
						leader_timer=leader_count_time;
						leader_count=5;
					}
				}
				break;
			case LeaderStage::Countdown:
				if(leader_timer<0) {
					leader_timer+=leader_count_time;

					leader_count--;

					if(leader_count>1) sound_engine.PlayWaveform(&leader_low_beep);
					else if(leader_count==1) sound_engine.PlayWaveform(&leader_high_beep);
					//send to burnout...
					else {
						leader_stage=LeaderStage::Burnout;
					}
				}
				break;
			case LeaderStage::Burnout:
				break;
		}

		return true;
	}

#pragma region RENDER HELPERS
	void DrawThickLineDecal(const vf2d& a, const vf2d& b, float w, olc::Pixel col) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=(sub/len).perp();

		float angle=std::atan2(sub.y, sub.x);
		DrawRotatedDecal(a-w*tang, prim_rect_dec, angle, {0, 0}, {len, 2*w}, col);
	}

	void FillCircleDecal(const vf2d& pos, float rad, olc::Pixel col) {
		vf2d offset(rad, rad);
		vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	void DrawThickCircleDecal(const vf2d& pos, float rad, float w, const olc::Pixel& col) {
		const int num=32;
		vf2d first, prev;
		for(int i=0; i<num; i++) {
			float angle=2*Pi*i/num;
			vf2d curr(pos.x+rad*std::cos(angle), pos.y+rad*std::sin(angle));
			FillCircleDecal(curr, w, col);
			if(i==0) first=curr;
			else DrawThickLineDecal(prev, curr, w, col);
			prev=curr;
		}
		DrawThickLineDecal(first, prev, w, col);
	}

	void FillPieDecal(const vf2d& pos, float rad, float st, float len, const olc::Pixel& col) {
		const int num=24;
		vf2d prev;
		for(int i=0; i<=num; i++) {
			float angle=st+len*i/num;
			vf2d curr=pos+rad*vf2d(std::cos(angle), std::sin(angle));
			if(i!=0) DrawWarpedDecal(prim_rect_dec, {curr, prev, pos, pos}, col);
			prev=curr;
		}
	}
#pragma endregion

	bool render() {
		const vf2d screen_size=GetScreenSize();

		switch(leader_stage) {
			case LeaderStage::Slideshow: {
				const auto& f=leader_slideshow[leader_slideshow_ix];
				vf2d scl=screen_size/vf2d(f.Sprite()->width, f.Sprite()->height);
				DrawDecal({0, 0}, f.Decal(), scl);
				break;
			}
			case LeaderStage::Countdown: {
				FillRectDecal({0, 0}, screen_size, olc::Pixel(230, 230, 230));
				//not efficient...
				const vf2d ctr=screen_size/2;
				const float rad=10+ctr.mag();
				const float st=-.5f*Pi;
				const float len=2*Pi*(1-leader_timer/leader_count_time);
				FillPieDecal(ctr, rad, st, len, olc::GREY);

				//draw centerlines
				const float line_rad=2.5f;
				const vf2d left(0, ctr.y), right(screen_size.x, ctr.y);
				const vf2d top(ctr.x, 0), bottom(ctr.x, screen_size.y);
				DrawThickLineDecal(left, right, .5f*line_rad, olc::BLACK);
				DrawThickLineDecal(top, bottom, .5f*line_rad, olc::BLACK);

				//center to top
				DrawThickLineDecal(ctr, top, line_rad, olc::BLACK);
				//center to arc
				{
					float angle=st+len;
					vf2d arc=ctr+rad*vf2d(std::cos(angle), std::sin(angle));
					DrawThickLineDecal(ctr, arc, line_rad, olc::BLACK);
				}
				//circle to cover jagged edge
				FillCircleDecal(ctr, line_rad, olc::BLACK);

				//draw two centered circles
				DrawThickCircleDecal(ctr, 90, line_rad, olc::WHITE);
				DrawThickCircleDecal(ctr, 120, line_rad, olc::WHITE);

				//draw count
				const float scl=10;
				const vf2d offset(4*scl, 4*scl);
				DrawStringDecal(ctr-offset, std::to_string(leader_count), olc::BLACK, {scl, scl});
				break;
			}
		}

		return true;
	}

	bool OnUserUpdate(float dt) override {
		if(!update(dt)) return false;

		if(!render()) return false;

		return true;
	}
};

int main() {
	Example demo;
	bool fullscreen=false;
	bool vsync=true;
	if(demo.Construct(640, 480, 1, 1, fullscreen, vsync)) demo.Start();

	return 0;
}