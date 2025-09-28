#define OLC_GFX_OPENGL33
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_SOUNDWAVE
#include "olcSoundWaveEngine.h"

#define OLC_PGEX_SHADERS
#include "olcPGEX_Shaders.h"

olc::EffectConfig loadEffect(const std::string& filename) {
	//get file
	std::ifstream file(filename);
	if(file.fail()) throw std::runtime_error("invalid filename: "+filename);

	//dump contents into str stream
	std::stringstream mid;
	mid<<file.rdbuf();

	return {
		DEFAULT_VS,
		mid.str(),
		1,
		1
	};
}

#include "common/utils.h"

struct Shape {
	vf2d pos, vel;
	float rad=0;
	int num=0;

	float rot=0, rot_vel=0;
	
	olc::Pixel col=olc::WHITE;
	bool to_fill=false;

	Shape() {}

	Shape(const olc::vf2d& p, float ra, int n, float ro, const olc::Pixel& c) {
		pos=p;
		rad=ra;
		num=n;
		rot=ro;
		col=c;
	}

	void update(float dt) {
		pos+=vel*dt;

		rot+=rot_vel*dt;
	}
};

#include "common/aabb.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

class Example : public olc::PixelGameEngine {
	//"primitive" render helpers
	olc::Renderable prim_rect, prim_circ;

	enum struct Stage {
		Wait,
		Slideshow,
		Countdown,
		Finished
	} stage=Stage::Wait;

	static const int slideshow_num=11;
	olc::Renderable slideshow[slideshow_num];
	int slideshow_ix=0;
	const float slideshow_time=.1667f;

	int countdown=0;
	const float countdown_time=.75f;

	std::vector<Shape> shapes;
	const vf2d gravity{0, 100};
	cmn::AABB screen_bounds;

	//some delay for things to load??
	float timer=.5f;

	//sound stuff
	olc::sound::WaveEngine sound_engine;
	olc::sound::Wave low_beep, high_beep;

	//post processing stuff
	olc::Renderable source;
	olc::Renderable target;
	olc::Shade shader;
	float total_dt=0;
	std::vector<olc::Effect> effects;
	float effect_timer=0;
	int effect_index=0;

	bool paused=false;

public:
	Example() {
		sAppName="glitch demo";
	}

public:
	bool OnUserCreate() override {
		std::srand(std::time(0));

		//make some "primitives" to draw with
		prim_rect.Create(1, 1);
		prim_rect.Sprite()->SetPixel(0, 0, olc::WHITE);
		prim_rect.Decal()->Update();
		{
			int sz=1024;
			prim_circ.Create(sz, sz);
			SetDrawTarget(prim_circ.Sprite());
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2);
			SetDrawTarget(nullptr);
			prim_circ.Decal()->Update();
		}

		//load leader slideshow
		for(int l=0; l<slideshow_num; l++) {
			//load image
			std::string filename="assets/img/leader"+std::to_string(1+l)+".png";
			olc::Sprite* spr=new olc::Sprite(filename);

			//downsample image
			slideshow[l].Create(ScreenWidth(), ScreenHeight());
			for(int i=0; i<ScreenWidth(); i++) {
				for(int j=0; j<ScreenHeight(); j++) {
					float u=(.5f+i)/ScreenWidth();
					float v=(.5f+j)/ScreenHeight();
					const auto& col=spr->SampleBL(u, v);
					slideshow[l].Sprite()->SetPixel(i, j, col);
				}
			}

			//update decal
			slideshow[l].Decal()->Update();

			//free sprite
			delete spr;
		}

		//initialize audio & load sounds
		{
			sound_engine.InitialiseAudio();
			sound_engine.SetOutputVolume(.3f);
			struct SoundLoader { olc::sound::Wave* wav; std::string str; };
			const std::vector<SoundLoader> sound_loaders{
				{&low_beep, "assets/sound/leader_low.wav"},
				{&high_beep, "assets/sound/leader_high.wav"}
			};
			for(const auto& sl:sound_loaders) {
				if(!sl.wav->LoadAudioWaveform(sl.str)) {
					std::cout<<"  err loading: "<<sl.str<<'\n';
					return false;
				}
			}
		}

		//allocate source and target buffers
		source.Create(ScreenWidth(), ScreenHeight());
		target.Create(ScreenWidth(), ScreenHeight());

		//load effects
		try {
			const std::vector<std::string> effect_files{
				"assets/fx/glitch.glsl",
				"assets/fx/crosshatch.glsl",
				"assets/fx/rgb_ht.glsl",
				"assets/fx/ascii6.glsl",
				"assets/fx/rainbow_sobel.glsl",
				"assets/fx/crt.glsl",
				"assets/fx/cmyk_ht.glsl",
				"assets/fx/mlv.glsl"
			};
			for(auto& str:effect_files) {
				olc::Effect eff=shader.MakeEffect(loadEffect(str));
				if(!eff.IsOK()) {
					std::cerr<<"  err loading: "<<str<<":\n"<<eff.GetStatus();
					return false;
				}
				effects.push_back(eff);
			}
		} catch(const std::exception& e) {
			std::cerr<<"  "<<e.what()<<'\n';
			return false;
		}

		//initialize screen_bounds
		screen_bounds={vf2d(0, 0), GetScreenSize()};

		return true;
	}

	bool update(float dt) {
		//exit on escape
		if(GetKey(olc::Key::ESCAPE).bPressed) return false;
		
		//pause on p
		if(GetKey(olc::Key::P).bPressed) paused^=true;

		//pause EVERYTHING
		if(!paused) {
			switch(stage) {
				case Stage::Wait:
					//send to slideshow
					if(GetKey(olc::Key::SPACE).bHeld) stage=Stage::Slideshow;

					break;
				case Stage::Slideshow:
					timer-=dt;
					if(timer<0) {
						timer+=slideshow_time;

						slideshow_ix++;

						//send to countdown...
						if(slideshow_ix==slideshow_num) {
							stage=Stage::Countdown;
							timer=countdown_time;
							countdown=7;
						}
					}
					break;
				case Stage::Countdown:
					timer-=dt;
					if(timer<0) {
						timer+=countdown_time;

						countdown--;

						if(countdown>1) sound_engine.PlayWaveform(&low_beep);
						else if(countdown==1) sound_engine.PlayWaveform(&high_beep);
						//send to finished...
						else stage=Stage::Finished;
					}
					break;
				case Stage::Finished:
					//every now and then...
					timer-=dt;
					if(timer<0) {
						//increment timer by random amount
						timer+=cmn::randFloat(.5f, 1.5f);

						//make new shape w/ random pos, size, orientation, color
						vf2d pos(cmn::randFloat(ScreenWidth()), cmn::randFloat(ScreenHeight()));
						float rad=cmn::randFloat(20, 100);
						int num=cmn::randInt(3, 8);
						float rot=cmn::randFloat(2*cmn::Pi);
						olc::Pixel col(cmn::randInt(0, 255), cmn::randInt(0, 255), cmn::randInt(0, 255));
						Shape s(pos, rad, num, rot, col);
					
						//random velocities
						float speed=cmn::randFloat(30, 60);
						float angle=cmn::randFloat(2*cmn::Pi);
						s.vel=cmn::polar<vf2d>(speed, angle);
						s.rot_vel=cmn::randFloat(-2, 2);
					
						//add it
						shapes.push_back(s);

						//randomize each shapes fill
						for(auto& s:shapes) s.to_fill=cmn::randFloat()>.5f;
					}

					//update & "sanitize" shapes
					for(auto it=shapes.begin(); it!=shapes.end();) {
						auto& s=*it;
						s.update(dt);
					
						//remove if offscreen
						if(!screen_bounds.overlaps({s.pos-s.rad, s.pos+s.rad})) {
							it=shapes.erase(it);
						} else it++;
					}

					break;
			}

			//every now and then...
			effect_timer-=dt;
			if(effect_timer<0) {
				//increment timer by random amount
				effect_timer+=cmn::randFloat(.25f, .5f);
			
				//choose new random shader
				effect_index=cmn::randInt(0, effects.size()-1);
			}
			
			total_dt+=dt;
		}

		return true;
	}

#pragma region RENDER HELPERS
	void DrawGrid(float size, const olc::Pixel& col) {
		const int num_x=1+ScreenWidth()/size;
		const int num_y=1+ScreenHeight()/size;
		for(int i=0; i<=num_x; i++) {
			float x=size*i;
			vf2d top(x, 0), btm(x, ScreenHeight());
			DrawLine(top, btm, col);
		}
		for(int j=0; j<=num_y; j++) {
			float y=size*j;
			vf2d lft(0, y), rgt(ScreenWidth(), y);
			DrawLine(lft, rgt, col);
		}
	}
	
	void DrawThickLine(const vf2d& st, const vf2d& en, float w, olc::Pixel col) {
		vf2d norm=(en-st).norm(), tang(-norm.y, norm.x);
		
		//position the vertexes
		vf2d a=st+w/2*tang, b=st-w/2*tang;
		vf2d c=en+w/2*tang, d=en-w/2*tang;

		//draw the triangles
		FillTriangle(a, b, c, col);
		FillTriangle(b, d, c, col);
	}

	void DrawThickCircle(const vf2d& pos, float rad, float w, const olc::Pixel& col) {
		static const int num=32;
		vf2d verts[2*num];

		//position the vertexes
		for(int i=0; i<num; i++) {
			float angle=2*cmn::Pi*i/num;
			vf2d dir=cmn::polar<vf2d>(1, angle);
			verts[2*i]=pos+(rad-w/2)*dir;
			verts[1+2*i]=pos+(rad+w/2)*dir;
		}

		//draw the triangles
		for(int i=0; i<num; i++) {
			int a=2*i, b=1+a;
			int c=2*((1+i)%num), d=1+c;
			FillTriangle(verts[a], verts[b], verts[c], col);
			FillTriangle(verts[b], verts[d], verts[c], col);
		}
	}

	void FillPie(const vf2d& pos, float rad, float st, float len, const olc::Pixel& col) {
		const int num=32;
		vf2d prev;
		for(int i=0; i<=num; i++) {
			float angle=st+len*i/num;
			vf2d curr=pos+cmn::polar<vf2d>(rad, angle);
			if(i!=0) FillTriangle(pos, prev, curr, col);
			prev=curr;
		}
	}
#pragma endregion

	bool render() {
		const vf2d screen_size=GetScreenSize();
		const vf2d ctr=screen_size/2;
		const vf2d mouse_pos=GetMousePos();

		SetDrawTarget(source.Sprite());
		Clear(olc::BLACK);

		switch(stage) {
			case Stage::Wait: {
				DrawGrid(48, olc::GREY);

				const int scl=3;
				std::string str="[Press Space]";
				DrawString(ctr.x-4*scl*str.length(), ctr.y-4*scl, str, olc::WHITE, scl);
				break;
			}
			case Stage::Slideshow: {
				const auto& f=slideshow[slideshow_ix];
				DrawSprite(0, 0, f.Sprite());
				break;
			}
			case Stage::Countdown: {
				FillRect({0, 0}, screen_size, olc::Pixel(230, 230, 230));
				//not efficient...
				const float pie_rad=10+ctr.mag();
				const float pie_st=-.5f*cmn::Pi;
				const float pie_len=2*cmn::Pi*(1-timer/countdown_time);
				FillPie(ctr, pie_rad, pie_st, pie_len, olc::GREY);

				//draw centerlines
				const vf2d left(0, ctr.y), right(screen_size.x, ctr.y);
				const vf2d top(ctr.x, 0), bottom(ctr.x, screen_size.y);
				DrawThickLine(left, right, 2, olc::BLACK);
				DrawThickLine(top, bottom, 2, olc::BLACK);

				//center to top
				DrawThickLine(ctr, top, 4, olc::BLACK);
				//center to arc
				{
					float angle=pie_st+pie_len;
					vf2d arc=ctr+cmn::polar<vf2d>(pie_rad, angle);
					DrawThickLine(ctr, arc, 4, olc::BLACK);
				}
				//circle to cover jagged edge
				FillCircle(ctr, 4, olc::BLACK);

				//draw two centered circles
				DrawThickCircle(ctr, 60, 4, olc::WHITE);
				DrawThickCircle(ctr, 90, 4, olc::WHITE);

				//draw count
				const int scl=6;
				auto str=std::to_string(countdown);
				DrawString(ctr.x-4*scl*str.length(), ctr.y-4*scl, str, olc::BLACK, scl);
				break;
			}
			case Stage::Finished:
				//background
				Clear(olc::GREY);
				DrawGrid(32, olc::WHITE);
				
				//render shapes
				for(const auto& s:shapes) {
					vf2d first, prev;
					for(int i=0; i<s.num; i++) {
						float angle=s.rot+2*cmn::Pi*i/s.num;
						vf2d curr=s.pos+cmn::polar<vf2d>(s.rad, angle);
						if(i==0) first=curr;
						else {
							if(!s.to_fill) DrawThickLine(prev, curr, 3, s.col);
							else FillTriangle(s.pos, prev, curr, s.col);
						}
						prev=curr;
					}
					if(!s.to_fill) DrawThickLine(prev, first, 3, s.col);
					else FillTriangle(s.pos, prev, first, s.col);
				}
				break;
		}

		//update frame info
		{
			int ms_count=1000*total_dt;
			Draw(0, 0, olc::Pixel(
				0xFF&ms_count,
				0xFF&(ms_count>>8),
				0xFF&(ms_count>>16),
				0xFF&(ms_count>>24)
			));
		}

		SetDrawTarget(nullptr);
		source.Decal()->Update();

		//apply random shader...
		shader.SetTargetDecal(target.Decal());
		shader.Start(&effects[effect_index]);
		shader.SetSourceDecal(source.Decal());
		shader.DrawQuad({-1, -1}, {2, 2});
		shader.End();

		//display it
		DrawDecal({0,0}, target.Decal());

		//show exit controls
		{
			std::string str="ESC to exit";
			vf2d offset(str.size(), 1);
			DrawStringDecal(GetScreenSize()-8*offset, str, olc::RED);
		}

		//show pause controls
		{
			std::string str="P to ";
			if(paused) str+="unpause";
			else str+="pause";
			DrawStringDecal(vf2d(0, ScreenHeight()-8), str, olc::BLUE);
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
	bool vsync=false;
	if(demo.Construct(640, 480, 1, 1, fullscreen, vsync)) demo.Start();

	return 0;
}