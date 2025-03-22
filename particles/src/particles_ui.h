#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "aabb.h"

#include "particle.h"

#include "solver.h"

#include "stopwatch.h"

#include "chart.h"

float map(float x, float a, float b, float c, float d) {
	float t=(x-a)/(b-a);
	return c+t*(d-c);
}

struct ParticlesUI : olc::PixelGameEngine {
	ParticlesUI() {
		sAppName="Particles";
	}

	//sprite stuff
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

	//ui stuff
	vf2d mouse_pos;

	float selection_radius=45;

	bool adding=false, removing=false;

	vf2d* bulk_start=nullptr;

	std::list<Particle*> grab_particles;

	//simulation
	Solver solver;

	float gravity=100;

	//graphics stuff
	bool show_grid=false;
	bool show_density=false;
	bool show_energy_chart=false;

	Chart energy_chart;

	bool OnUserCreate() override {
		//make circle "primitive" to draw with
		{
			int sz=1024;
			prim_circ_spr=new olc::Sprite(sz, sz);
			SetDrawTarget(prim_circ_spr);
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2);
			SetDrawTarget(nullptr);
			prim_circ_dec=new olc::Decal(prim_circ_spr);
		}

		//load ball sprites
		for(int i=0; i<num_img; i++) {
			std::string filename="assets/"+filenames[i]+"_ball.png";
			ball_spr[i]=new olc::Sprite(filename);
			ball_dec[i]=new olc::Decal(ball_spr[i]);
		}

		solver.bounds={vf2d(0, 0), vf2d(ScreenWidth(), ScreenHeight())};

		//setup energy chart
		{
			float w=200;
			float h=140;
			float margin=30;
			vf2d tl(ScreenWidth()-w-margin, margin);
			vf2d br(ScreenWidth()-margin, h+margin);
			energy_chart=Chart(500, {tl, br});
		}

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
		if(dt>1/30.f) dt=1/30.f;

		mouse_pos=GetMousePos();

#pragma region USER INPUT
		const bool to_time=GetKey(olc::Key::T).bPressed;
		if(to_time) std::cout<<"\nDiagnostic Info:\n";

		const bool adding=GetMouse(olc::Mouse::LEFT).bHeld;
		if(adding) {
			float pos_rad=random(0, selection_radius);
			vf2d offset=polar(pos_rad, random(2*Pi));

			//random size and color
			float ptc_rad=random(5, 10);
			float speed=dt*random(20);
			Particle temp(mouse_pos+offset, ptc_rad);
			temp.oldpos-=polar(speed, random(2*Pi));

			const auto& p=solver.addParticle(temp);
			if(p) p->id=rand()%7;
		}

		const auto bulk_action=GetKey(olc::Key::B);//GetMouse(olc::Mouse::MIDDLE);
		if(bulk_action.bPressed) {
			bulk_start=new vf2d(mouse_pos);
		}
		if(bulk_action.bReleased) {
			if(bulk_start) {
				AABB box;
				box.fitToEnclose(*bulk_start);
				box.fitToEnclose(mouse_pos);
				float max_rad=7;
				int num_x=(box.max.x-box.min.x)/max_rad/2;
				int num_y=(box.max.y-box.min.y)/max_rad/2;
				for(int i=0; i<num_x; i++) {
					float x=map(i, 0, num_x-1, box.min.x, box.max.x);
					for(int j=0; j<num_y; j++) {
						float y=map(j, 0, num_y-1, box.min.y, box.max.y);
						Particle temp({x, y}, random(4, max_rad));

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
			grab_particles.clear();
			for(const auto& p:solver.particles) {
				if((mouse_pos-p.pos).mag()<selection_radius) {
					solver.removeParticle(p);
				}
			}
		}

		const auto grab_action=GetKey(olc::Key::G);
		if(grab_action.bPressed) {
			grab_particles.clear();
			for(auto& p:solver.particles) {
				if((mouse_pos-p.pos).mag()<selection_radius) {
					grab_particles.push_back(&p);
				}
			}
		}
		if(grab_action.bReleased) {
			grab_particles.clear();
		}

		//graphics toggles
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;
		if(GetKey(olc::Key::D).bPressed) show_density^=true;
		if(GetKey(olc::Key::E).bPressed) show_energy_chart^=true;
#pragma endregion

#pragma region PHYSICS
		Stopwatch physics_watch;
		if(to_time) physics_watch.start();

		int checks=0;
		const int sub_steps=3;
		float sub_dt=dt/sub_steps;
		for(int i=0; i<sub_steps; i++) {
			checks+=solver.solveCollisions();

			solver.applyGravity(gravity);

			solver.updateKinematics(sub_dt);
		}

		//update energy chart
		{
			float total_energy=0;
			for(const auto& p:solver.particles) {
				//mgh
				float h=solver.bounds.max.y-p.pos.y;
				total_energy+=p.mass*gravity*h;
				//1/2mv^2
				vf2d vel=p.pos-p.oldpos;
				total_energy+=.5f*p.mass*vel.dot(vel);
			}
			energy_chart.update(total_energy);
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
			float pct=(brute-checks)/brute;
			std::cout<<"  "<<int(100*pct)<<"% faster\n";
		}
#pragma endregion

#pragma region RENDER
		Stopwatch render_watch;
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

		//show energy chart
		if(show_energy_chart) {
			const auto& box=energy_chart.bounds;
			vf2d size=box.max-box.min;
			FillRectDecal(box.min-2, size+4, olc::BLACK);
			FillRectDecal(box.min, size, olc::GREY);
			float max=energy_chart.getMaxValue();
			for(int i=0; i<energy_chart.getNum();i++) {
				float x=map(i, 0, energy_chart.getNum()-1, box.min.x, box.max.x);
				float y=map(energy_chart.values[i], 0, max, box.max.y, box.min.y);
				FillCircleDecal({x, y}, 1, olc::BLUE);
			}
			DrawStringDecal(box.min, "Energy", olc::WHITE);
		}

		if(to_time) {
			render_watch.stop();
			auto dur=render_watch.getMicros();
			std::cout<<"render: "<<dur<<"us ("<<(dur/1000.f)<<" ms)\n";
		}
#pragma endregion

		return true;
	}
};