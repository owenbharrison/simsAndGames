#define OLC_PGE_APPLICATION
#include "olc/include/olcPixelGameEngine.h"
using olc::vf2d;

#include "smoke.h"

#include "cmn/utils.h"

struct SmokeDemo : olc::PixelGameEngine {
	SmokeDemo() {
		sAppName="Smoke Demo";
	}

	//sprites
	olc::Renderable prim_rect;
	olc::Renderable smoke_tex;

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

	float total_dt=0;

	bool help_menu=false;

	bool OnUserCreate() override {
		std::srand(std::time(0));

		//"primitives" to draw with
		prim_rect.Create(1, 1);
		prim_rect.Sprite()->SetPixel(0, 0, olc::WHITE);
		prim_rect.Decal()->Update();

		//load smoke sprite
		smoke_tex.Load("assets/smoke.png");

		emitter_pos=vf2d(.2f*ScreenWidth(), .75f*ScreenHeight());

		return true;
	}

#pragma region UPDATE HELPERS
	void handleEmitBegin() {
		//randomize emitter col
		emitter_col=olc::Pixel(
			cmn::randInt(0, 255),
			cmn::randInt(0, 255),
			cmn::randInt(0, 255)
		);
	}

	//add particles at mouse
	void handleEmitUpdate() {
		vf2d sub=mouse_pos-emitter_pos;
		float dist=sub.mag();
		vf2d dir=sub/dist;
		float speed=std::max(0.f, dist+cmn::randFloat(-100, 100));
		vf2d vel=speed*dir;
		float rot=cmn::randFloat(2*cmn::Pi);
		float rot_vel=cmn::randFloat(-1.5f, 1.5f);
		float size=cmn::randFloat(10, 20);
		float size_vel=cmn::randFloat(15, 25);
		particles.push_back(Smoke(emitter_pos, vel, emitter_col, rot, rot_vel, size, size_vel));
	}

	void handleParticleBomb() {
		//randomize col each click
		olc::Pixel smoke_col(
			cmn::randInt(0, 255),
			cmn::randInt(0, 255),
			cmn::randInt(0, 255)
		);

		//spawn a random number
		int num=cmn::randInt(20, 40);
		for(int i=0; i<num; i++) {
			float speed=cmn::randFloat(30, 70);
			vf2d vel=cmn::polar<vf2d>(speed, cmn::randFloat(2*cmn::Pi));

			float rot=cmn::randFloat(2*cmn::Pi);
			float rot_vel=cmn::randFloat(-1.5f, 1.5f);
			float size=cmn::randFloat(10, 20);
			float size_vel=cmn::randFloat(15, 25);
			particles.push_back(Smoke(mouse_pos, vel, smoke_col, rot, rot_vel, size, size_vel));
		}
	}

	void handleUserInput(float dt) {
		const auto emit_action=GetMouse(olc::Mouse::LEFT);
		bool emit_rainbow=GetMouse(olc::Mouse::RIGHT).bHeld;
		if(emit_action.bPressed||emit_rainbow) handleEmitBegin();
		if(emit_action.bHeld||emit_rainbow) handleEmitUpdate();

		if(GetMouse(olc::Mouse::MIDDLE).bPressed) {
			handleParticleBomb();
		}

		//set emitter pos
		if(update_emitter) emitter_pos=mouse_pos;

		//gfx toggles
		if(GetKey(olc::Key::E).bPressed) update_emitter^=true;
		if(GetKey(olc::Key::S).bPressed) show_sprites^=true;
		if(GetKey(olc::Key::O).bPressed) show_outlines^=true;
		if(GetKey(olc::Key::P).bPressed) update_physics^=true;
		if(GetKey(olc::Key::H).bPressed) help_menu^=true;
	}
#pragma endregion

	void update(float dt) {
		prev_mouse_pos=mouse_pos;
		mouse_pos=GetMousePos();

		handleUserInput(dt);

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
	}

#pragma region RENDER HELPERS
	void renderEmitter() {
		float rad=4.7f;
		vf2d norm=rad*(mouse_pos-emitter_pos).norm();
		vf2d tang(-norm.y, norm.x);
		DrawLineDecal(emitter_pos-norm, emitter_pos+2*norm, olc::WHITE);
		DrawLineDecal(emitter_pos-tang, emitter_pos+tang, olc::WHITE);
	}

	void renderOutline(const Smoke& sm) {
		//precompute rotation matrix
		float c=std::cos(sm.rot), s=std::sin(sm.rot);

		//for each corner
		vf2d corners[4]{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
		for(int i=0; i<4; i++) {
			//rotate, scale, & translate
			corners[i]=sm.pos+sm.size/2*vf2d(
				c*corners[i].x-s*corners[i].y,
				s*corners[i].x+c*corners[i].y
			);
		}

		//alpha based on smoke "age"
		olc::Pixel outline=olc::WHITE;
		outline.a=255*sm.life;
		
		//draw box
		DrawLineDecal(corners[0], corners[1], outline);
		DrawLineDecal(corners[1], corners[2], outline);
		DrawLineDecal(corners[2], corners[3], outline);
		DrawLineDecal(corners[3], corners[0], outline);

		//draw diagonal
		DrawLineDecal(corners[0], corners[2], outline);
	}

	void renderHelpHints() {
		int cx=ScreenWidth()/2;
		if(help_menu) {
			DrawStringDecal(vf2d(8, 8), "Mouse Controls");
			DrawStringDecal(vf2d(8, 16), "Left for emitter");
			DrawStringDecal(vf2d(8, 24), "Right for rainbow");
			DrawStringDecal(vf2d(8, 32), "Middle for bomb");

			DrawStringDecal(vf2d(ScreenWidth()-8*19, 8), "Toggleable Options");
			DrawStringDecal(vf2d(ScreenWidth()-8*14, 16), "E for emitter", update_emitter?olc::WHITE:olc::RED);
			DrawStringDecal(vf2d(ScreenWidth()-8*14, 24), "S for sprites", show_sprites?olc::WHITE:olc::RED);
			DrawStringDecal(vf2d(ScreenWidth()-8*15, 32), "O for outlines", show_outlines?olc::WHITE:olc::RED);
			DrawStringDecal(vf2d(ScreenWidth()-8*17, 40), "P for pause/play", update_physics?olc::WHITE:olc::RED);

			DrawStringDecal(vf2d(cx-4*18, ScreenHeight()-8), "[Press H to close]");
		} else {
			DrawStringDecal(vf2d(cx-4*18, ScreenHeight()-8), "[Press H for help]");
		}
	}
#pragma endregion

	void render() {
		//render
		Clear(olc::DARK_GREY);

		//show emitter
		if(!update_emitter) renderEmitter();

		//draw particles
		const vf2d spr_sz(smoke_tex.Sprite()->width, smoke_tex.Sprite()->height);
		for(const auto& p:particles) {
			//to show sprite or just a box?
			olc::Pixel fill=p.col;
			fill.a=255*p.life;
			if(show_sprites) DrawRotatedDecal(p.pos, smoke_tex.Decal(), p.rot, spr_sz/2, p.size/spr_sz, fill);
			else DrawRotatedDecal(p.pos, prim_rect.Decal(), p.rot, {.5f, .5f}, {p.size, p.size}, fill);

			if(show_outlines) renderOutline(p);
		}

		renderHelpHints();
	}

	bool OnUserUpdate(float dt) {
		update(dt);

		render();

		return true;
	}
};

int main() {
	SmokeDemo sd;
	if(sd.Construct(640, 480, 1, 1, false, true)) sd.Start();

	return 0;
}