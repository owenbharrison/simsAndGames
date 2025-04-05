#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "smoke.h"

#include "common/utils.h"
namespace cmn {
	vf2d polar(float rad, float angle) {
		return polar_generic<vf2d>(rad, angle);
	}
}

struct SmokeDemo : olc::PixelGameEngine {
	SmokeDemo() {
		sAppName="Smoke Demo";
	}

	//sprites
	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;
	olc::Sprite* smoke_spr=nullptr;
	olc::Decal* smoke_dec=nullptr;

	//ui stuff
	vf2d mouse_pos;
	vf2d prev_mouse_pos;

	//make emitter class!
	vf2d emitter_pos;
	olc::Pixel emitter_col;

	//simulation
	std::list<Smoke> particles;
	vf2d buoyancy{0, -120};
	bool update_emitter=false;
	bool update_physics=true;

	//graphics toggles
	bool show_sprites=true;
	bool show_outlines=false;

	bool OnUserCreate() override {
		srand(time(0));

		//"primitives" to draw with
		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);
		
		//load smoke sprite
		smoke_spr=new olc::Sprite("assets/smoke.png");
		smoke_dec=new olc::Decal(smoke_spr);

		emitter_pos=vf2d(.2f*ScreenWidth(), .75f*ScreenHeight());

		return true;
	}

	bool OnUserDestroy() override {
		delete prim_rect_dec;
		delete prim_rect_spr;

		delete smoke_dec;
		delete smoke_spr;

		return true;
	}

	float total_dt=0;

	bool OnUserUpdate(float dt) override {
		prev_mouse_pos=mouse_pos;
		mouse_pos=GetMousePos();

		const auto emit_action=GetMouse(olc::Mouse::LEFT);
		bool emit_rainbow=GetMouse(olc::Mouse::RIGHT).bHeld;
		if(emit_action.bPressed||emit_rainbow) {
			//randomize col each click
			emitter_col.r=rand()%255;
			emitter_col.g=rand()%255;
			emitter_col.b=rand()%255;
		}
		//add particles at mouse
		if(emit_action.bHeld||emit_rainbow) {
			vf2d sub=mouse_pos-emitter_pos;
			float dist=sub.mag();
			vf2d dir=sub/dist;
			float speed=std::max(0.f, dist+cmn::random(-100, 100));
			vf2d vel=speed*dir;
			float rot=cmn::random(2*cmn::Pi);
			float rot_vel=cmn::random(-1.5f, 1.5f);
			float size=cmn::random(10, 20);
			float size_vel=cmn::random(15, 25);
			particles.push_back(Smoke(emitter_pos, vel, emitter_col, rot, rot_vel, size, size_vel));
		}

		if(GetMouse(olc::Mouse::MIDDLE).bPressed) {
			//randomize col each click
			olc::Pixel debris_col(
				rand()%255,
				rand()%255,
				rand()%255
			);
			olc::Pixel smoke_col(
				rand()%255,
				rand()%255,
				rand()%255
			);
			//spawn a random number
			int num=20+rand()%20;
			for(int i=0; i<num; i++) {
				float speed=cmn::random(30, 70);
				vf2d vel=cmn::polar(speed, cmn::random(2*cmn::Pi));

				float rot=cmn::random(2*cmn::Pi);
				float rot_vel=cmn::random(-1.5f, 1.5f);
				float size=cmn::random(10, 20);
				float size_vel=cmn::random(15, 25);
				particles.push_back(Smoke(mouse_pos, vel, smoke_col, rot, rot_vel, size, size_vel));
			}
		}

		//set emitter pos
		if(update_emitter) emitter_pos=mouse_pos;

		//gfx toggles
		if(GetKey(olc::Key::E).bPressed) update_emitter^=true;
		if(GetKey(olc::Key::S).bPressed) show_sprites^=true;
		if(GetKey(olc::Key::O).bPressed) show_outlines^=true;
		if(GetKey(olc::Key::SPACE).bPressed) update_physics^=true;

		//update particles
		if(update_physics) {
			for(auto& p:particles) {
				//drag
				p.vel*=1-.85f*dt;
				p.rot_vel*=1-.55f*dt;
				p.size_vel*=1-.25f*dt;

				//kinematics
				p.accelerate(buoyancy);
				p.update(dt);

				//aging?
				p.life-=.25f*dt;
			}
		}

		//remove offscreen particles
		for(auto it=particles.begin(); it!=particles.end();) {
			if(it->isDead()) {
				it=particles.erase(it);
			} else it++;
		}

		//render
		Clear(olc::DARK_GREY);

		//show emitter
		if(!update_emitter) {
			float rad=4.7f;
			vf2d norm=rad*(mouse_pos-emitter_pos).norm();
			vf2d tang(-norm.y, norm.x);
			DrawLineDecal(emitter_pos-norm, emitter_pos+2*norm, olc::WHITE);
			DrawLineDecal(emitter_pos-tang, emitter_pos+tang, olc::WHITE);
		}

		//draw particles
		const vf2d spr_sz(smoke_spr->width, smoke_spr->height);
		for(const auto& p:particles) {
			//to show sprite or just a box?
			olc::Pixel fill=p.col;
			fill.a=255*p.life;
			if(show_sprites) DrawRotatedDecal(p.pos, smoke_dec, p.rot, spr_sz/2, p.size/spr_sz, fill);
			else DrawRotatedDecal(p.pos, prim_rect_dec, p.rot, {.5f, .5f}, {p.size, p.size}, fill);
					
			if(show_outlines) {
				//for each corner
				vf2d corners[4]{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
				float c=std::cosf(p.rot), s=std::sinf(p.rot);
				for(int i=0; i<4; i++) {
					//scale corners
					corners[i]*=p.size/2;
					//rotate and translate
					corners[i]=p.pos+vf2d(
						c*corners[i].x-s*corners[i].y,
						s*corners[i].x+c*corners[i].y
					);
				}
				//draw box
				olc::Pixel outline=olc::WHITE;
				outline.a=255*p.life;
				DrawLineDecal(corners[0], corners[1], outline);
				DrawLineDecal(corners[1], corners[2], outline);
				DrawLineDecal(corners[2], corners[3], outline);
				DrawLineDecal(corners[3], corners[0], outline);
				//draw diagonal
				DrawLineDecal(corners[0], corners[2], outline);
			}
		}

		//show red border to indicate physics not updating
		if(!update_physics) {
			float rad=4.7f;
			FillRectDecal({0, 0}, vf2d(rad, ScreenHeight()), olc::RED);
			FillRectDecal({0, 0}, vf2d(ScreenWidth(), rad), olc::RED);
			vf2d btm(ScreenWidth(), ScreenHeight());
			FillRectDecal(vf2d(0, ScreenHeight()-rad), btm, olc::RED);
			FillRectDecal(vf2d(ScreenWidth()-rad, 0), btm, olc::RED);
		}

		return true;
	}
};

int main() {
	SmokeDemo sd;
	if(sd.Construct(640, 480, 1, 1, false, true)) sd.Start();

	return 0;
}