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

	static const int num_img=7;
	const std::string filenames[num_img]{
		"base",
		"basket",
		"beach",
		"dodge",
		"soccer",
		"tennis",
		"volley"
	};
	olc::Sprite* ball_spr[num_img];
	olc::Decal* ball_dec[num_img];

	float selection_radius=45;

	vf2d* bulk_start=nullptr;

	Solver solver;

	bool show_grid=false;
	bool show_density=false;

	bool OnUserCreate() override {
		//make some "primitives" to draw with
		int sz=1024;
		prim_circ_spr=new olc::Sprite(sz, sz);
		SetDrawTarget(prim_circ_spr);
		Clear(olc::BLANK);
		FillCircle(sz/2, sz/2, sz/2);
		SetDrawTarget(nullptr);
		prim_circ_dec=new olc::Decal(prim_circ_spr);

		//load ball sprites
		for(int i=0; i<num_img; i++) {
			std::string filename="assets/"+filenames[i]+"_ball.png";
			ball_spr[i]=new olc::Sprite(filename);
			ball_dec[i]=new olc::Decal(ball_spr[i]);
		}

		solver.bounds={vf2d(0, 0), vf2d(ScreenWidth(), ScreenHeight())};

		solver.gravity=vf2d(0, 24);

		return true;
	}

	bool OnUserDestroy() override {
		delete prim_circ_dec;
		delete prim_circ_spr;

		for(int i=0; i<num_img; i++) {
			delete ball_dec[i];
			delete ball_spr[i];
		}

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
		Stopwatch physics_watch, render_watch;
		const bool to_time=GetKey(olc::Key::T).bPressed;
		if(to_time) std::cout<<"\nDiagnostic Info:\n";

		const bool adding=GetMouse(olc::Mouse::LEFT).bHeld;
		if(adding) {
			float pos_rad=random(0, selection_radius);
			vf2d offset=polar(pos_rad, random(2*Pi));

			//random size and color
			float ptc_rad=random(3, 7);
			float speed=dt*random(20);
			Particle temp(mouse_pos+offset, ptc_rad);
			temp.oldpos-=polar(speed, random(2*Pi));

			const auto& p=solver.addParticle(temp);
			if(p) p->id=rand()%7;
		}

		const auto bulk_action=GetMouse(olc::Mouse::MIDDLE);
		if(bulk_action.bPressed) {
			bulk_start=new vf2d(mouse_pos);
		}
		if(bulk_action.bReleased) {
			if(bulk_start) {
				AABB box;
				box.fitToEnclose(*bulk_start);
				box.fitToEnclose(mouse_pos);
				float max_rad=10;
				int num_x=(box.max.x-box.min.x)/max_rad/2;
				int num_y=(box.max.y-box.min.y)/max_rad/2;
				for(int i=0; i<num_x; i++) {
					float x=map(i, 0, num_x-1, box.min.x, box.max.x);
					for(int j=0; j<num_y; j++) {
						float y=map(j, 0, num_y-1, box.min.y, box.max.y);
						Particle temp({x, y}, random(5, max_rad));

						const auto& p=solver.addParticle(temp);
						if(p) p->id=rand()%7;
					}
				}
			}

			delete bulk_start;
			bulk_start=nullptr;
		}

		const bool removing=GetMouse(olc::Mouse::RIGHT).bHeld;
		if(removing) {
			for(const auto& p:solver.particles) {
				if((mouse_pos-p.pos).mag()<selection_radius) {
					solver.removeParticle(p);
				}
			}
		}

		//graphics toggles
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;
		if(GetKey(olc::Key::D).bPressed) show_density^=true;

		//physics
		if(to_time) physics_watch.start();

		int checks=0;
		const int sub_steps=3;
		float sub_dt=dt/sub_steps;
		for(int i=0; i<sub_steps; i++) {
			checks+=solver.solveCollisions();

			solver.updateKinematics(dt);
		}

		if(to_time) {
			physics_watch.stop();
			auto dur=physics_watch.getMicros();
			std::cout<<"physics: "<<dur<<"us ("<<(dur/1000.f)<<" ms)\n";
			int num=solver.particles.size();
			std::cout<<"at "<<num<<" particles:\n";
			int brute=num*(num-1)/2;
			std::cout<<"  brute force: "<<brute<<" tests\n";
			std::cout<<"  spacial hash: "<<checks<<" tests\n";
			int pct=100.f*(brute-checks)/brute;
			std::cout<<"  "<<pct<<"% faster\n";
		}

		//render
		if(to_time) render_watch.start();

		const olc::Pixel background_color(190, 255, 255);
		Clear(background_color);

		//show addition radius
		if(adding) {
			FillCircleDecal(mouse_pos, selection_radius, olc::GREEN);
			FillCircleDecal(mouse_pos, selection_radius-2, background_color);
		}

		//show deletion radius
		if(removing) {
			FillCircleDecal(mouse_pos, selection_radius, olc::RED);
			FillCircleDecal(mouse_pos, selection_radius-2, background_color);
		}

		//show bulk action
		if(bulk_start) {
			AABB box;
			box.fitToEnclose(*bulk_start);
			box.fitToEnclose(mouse_pos);
			vf2d tr(box.max.x, box.min.y);
			vf2d bl(box.min.x, box.max.y);
			DrawLineDecal(box.min, tr, olc::BLUE);
			DrawLineDecal(tr, box.max, olc::BLUE);
			DrawLineDecal(box.max, bl, olc::BLUE);
			DrawLineDecal(bl, box.min, olc::BLUE);
		}

		//show particles
		if(!show_density) {
			for(const auto& p:solver.particles) {
				const auto& s=ball_spr[p.id];
				vf2d scale=2*p.rad/vf2d(s->width, s->height);
				DrawDecal(p.pos-vf2d(p.rad, p.rad), ball_dec[p.id], scale);
			}
		}

		//draw solver hash grid
		if(show_grid) {
			//vertical lines
			for(int i=0; i<=solver.getNumCellX(); i++) {
				float x=solver.getCellSize()*i;
				vf2d top(x, 0), btm(x, ScreenHeight());
				DrawLineDecal(top, btm, olc::BLACK);
			}

			//horizontal lines
			for(int j=0; j<=solver.getNumCellY(); j++) {
				float y=solver.getCellSize()*j;
				vf2d lft(0, y), rgt(ScreenWidth(), y);
				DrawLineDecal(lft, rgt, olc::BLACK);
			}
		}

		//show values(debug)
		if(show_density) {
			//find max cell num
			int max=0;
			for(int i=0; i<solver.getNumCellX(); i++) {
				for(int j=0; j<solver.getNumCellY(); j++) {
					int num=solver.getCell(i, j).size();
					if(num>max) max=num;
				}
			}
			//draw squares showing "density"
			if(max!=0) {
				for(int i=0; i<solver.getNumCellX(); i++) {
					for(int j=0; j<solver.getNumCellY(); j++) {
						int num=solver.getCell(i, j).size();
						if(num==0) continue;

						int shade=map(num, 0, max, 0, 255);
						olc::Pixel col(shade, shade, shade);

						float x=solver.getCellSize()*i;
						float y=solver.getCellSize()*j;
						FillRectDecal({x, y}, {solver.getCellSize(), solver.getCellSize()}, col);

						float cx=.5f*solver.getCellSize()+x;
						float cy=.5f*solver.getCellSize()+y;
						std::string str=std::to_string(num);
						DrawStringDecal({cx-4, cy-4}, str);
					}
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