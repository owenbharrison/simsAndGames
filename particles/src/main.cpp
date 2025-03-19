#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "aabb.h"

#include "particle.h"
#include "solver.h"

#include "stopwatch.h"

float map(float x, float a, float b, float c, float d) {
	float t=(x-a)/(b-a);
	return c+t*(d-c);
}

struct ParticleUI : olc::PixelGameEngine {
	ParticleUI() {
		sAppName="Particles";
	}

	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;

	float add_timer=0;

	float addition_radius=30;

	Solver solver;

	bool OnUserCreate() override {
		//make some "primitives" to draw with
		int sz=1024;
		prim_circ_spr=new olc::Sprite(sz, sz);
		SetDrawTarget(prim_circ_spr);
		Clear(olc::BLANK);
		FillCircle(sz/2, sz/2, sz/2);
		SetDrawTarget(nullptr);
		prim_circ_dec=new olc::Decal(prim_circ_spr);

		solver.bounds={vf2d(0, 0), vf2d(ScreenWidth(), ScreenHeight())};

		solver.gravity=vf2d(0, 24);

		return true;
	}

	bool OnUserDestroy() override {
		delete prim_circ_dec;
		delete prim_circ_spr;

		return true;
	}

	void FillCircleDecal(const olc::vf2d& pos, float rad, olc::Pixel col) {
		olc::vf2d offset(rad, rad);
		olc::vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	bool OnUserUpdate(float dt) override {
		//if(dt>1/30.f) dt=1/30.f;

		dt=1/60.f;

		const vf2d mouse_pos=GetMousePos();

		//user input
		const bool to_time=GetKey(olc::Key::T).bPressed;
		Stopwatch physics_watch, render_watch;

		const bool adding=GetMouse(olc::Mouse::LEFT).bHeld;
		if(adding) {
			float pos_rad=random(0, addition_radius);
			vf2d offset=polar(pos_rad, random(2*Pi));

			//random size and color
			float ptc_rad=random(3, 10);
			olc::Pixel col(
				rand()%255,
				rand()%255,
				rand()%255
			);
			float speed=dt*random(20);
			Particle temp(mouse_pos+offset, ptc_rad, col);
			temp.oldpos-=polar(speed, random(2*Pi));

			solver.addParticle(temp);
		}

		//physics
		if(to_time) physics_watch.start();

		const int sub_steps=3;
		float sub_dt=dt/sub_steps;
		for(int i=0; i<sub_steps; i++) {
			solver.solveCollisions();

			solver.updateKinematics(dt);
		}

		if(to_time) {
			physics_watch.stop();
			auto dur=physics_watch.getMicros();
			std::cout<<"physics: "<<dur<<"us ("<<(dur/1000.f)<<" ms)\n";
		}

		//render
		if(to_time) render_watch.start();

		Clear(olc::BLACK);

		//show addition radius
		if(adding) {
			FillCircleDecal(mouse_pos, addition_radius, olc::GREY);
			FillCircleDecal(mouse_pos, addition_radius-2, olc::BLACK);
		}

		//draw solver hash grid
		if(GetKey(olc::Key::H).bHeld) {
			//vertical lines
			for(int i=0; i<=solver.getNumCellX(); i++) {
				float x=solver.getCellSize()*i;
				vf2d top(x, 0), btm(x, ScreenHeight());
				DrawLineDecal(top, btm, olc::CYAN);
			}

			//horizontal lines
			for(int j=0; j<=solver.getNumCellY(); j++) {
				float y=solver.getCellSize()*j;
				vf2d lft(0, y), rgt(ScreenWidth(), y);
				DrawLineDecal(lft, rgt, olc::CYAN);
			}
		}

		//show particles
		for(const auto& p:solver.particles) {
			FillCircleDecal(p.pos, p.rad, p.col);
		}

		//show values(debug)
		if(GetKey(olc::Key::G).bHeld) {
			for(int i=0; i<solver.getNumCellX(); i++) {
				for(int j=0; j<solver.getNumCellY(); j++) {
					float x=solver.getCellSize()*(.5f+i);
					float y=solver.getCellSize()*(.5f+j);
					const auto& cell=solver.cells[solver.hashIX(i, j)];
					auto str=std::to_string(cell.size());
					DrawStringDecal(vf2d(x-4*str.length(), y-4), str, olc::BLACK);
				}
			}
		}

		if(to_time) {
			render_watch.stop();
			auto dur=render_watch.getMicros();
			std::cout<<"render: "<<dur<<"us ("<<(dur/1000.f)<<" ms)\n";
		}

		return true;
	}
};

int main() {
	ParticleUI pui;
	if(pui.Construct(720, 640, 1, 1, false, true)) pui.Start();

	return 0;
}