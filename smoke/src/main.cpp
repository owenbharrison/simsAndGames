#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "smoke.h"

constexpr float Pi=3.1415927f;

//clever default param placement:
//random()=0-1
//random(a)=0-a
//random(a, b)=a-b
float random(const float& b=1, const float& a=0) {
	static const float rand_max=RAND_MAX;
	float t=rand()/rand_max;
	return a+t*(b-a);
}

//polar to cartesian helper
vf2d polar(const float& rad, const float& angle) {
	return {rad*std::cosf(angle), rad*std::sinf(angle)};
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
	vf2d gravity{0, -50};

	bool show_sprites=true;
	bool show_outlines=false;

	bool OnUserCreate() override {
		srand(time(0));

		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);

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

		const auto add_action=GetMouse(olc::Mouse::LEFT);
		if(add_action.bPressed) {
			//randomize col each click
			emitter_col.r=rand()%255;
			emitter_col.g=rand()%255;
			emitter_col.b=rand()%255;
		}
		//add particles at mouse
		if(add_action.bHeld) {
			vf2d dir=(mouse_pos-emitter_pos).norm();
			float speed=random(125, 325);
			vf2d vel=speed*dir;
			float rot=random(2*Pi);
			float rot_vel=random(-1.5f, 1.5f);
			float size=random(10, 20);
			float size_vel=random(15, 25);
			Smoke s(emitter_pos, vel, rot, rot_vel, size, size_vel, emitter_col);
			particles.push_back(s);
		}

		//gfx toggles
		if(GetKey(olc::Key::S).bPressed) show_sprites^=true;
		if(GetKey(olc::Key::O).bPressed) show_outlines^=true;

		//update particles
		for(auto& p:particles) {
			//drag
			p.vel.x*=1-.9f*dt;
			p.rot_vel*=1-.55f*dt;
			p.size_vel*=1-.25f*dt;

			//kinematics
			p.accelerate(gravity);
			p.update(dt);

			//aging?
			p.life-=.25f*dt;
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
		FillRectDecal(emitter_pos, {4, 4}, olc::RED);

		//draw particles
		const vf2d spr_sz(smoke_spr->width, smoke_spr->height);
		for(const auto& p:particles) {
			olc::Pixel col=p.col;
			col.a=255*p.life;
			//to show sprite or just a box?
			if(show_sprites) DrawRotatedDecal(p.pos, smoke_dec, p.rot, spr_sz/2, p.size/spr_sz, col);
			else DrawRotatedDecal(p.pos, prim_rect_dec, p.rot, {.5f, .5f}, {p.size, p.size}, col);
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
				DrawLineDecal(corners[0], corners[1], olc::WHITE);
				DrawLineDecal(corners[1], corners[2], olc::WHITE);
				DrawLineDecal(corners[2], corners[3], olc::WHITE);
				DrawLineDecal(corners[3], corners[0], olc::WHITE);
				//draw diagonal
				DrawLineDecal(corners[0], corners[2], olc::WHITE);
			}
		}

		return true;
	}
};

int main() {
	SmokeDemo sd;
	if(sd.Construct(640, 480, 1, 1, false, true)) sd.Start();

	return 0;
}