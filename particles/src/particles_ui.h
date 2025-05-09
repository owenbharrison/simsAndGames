#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "solver.h"

#include "common/stopwatch.h"

#include "chart.h"

float map(float x, float a, float b, float c, float d) {
	float t=(x-a)/(b-a);
	return c+t*(d-c);
}

//this assumes a true color terminal
void setConsoleColor(int r, int g, int b) {
	std::cout<<"\033[38;2;"<<r<<';'<<g<<';'<<b<<'m';
}

void setConsoleColorFromPerf(float ms) {
	//red vs yellow vs green
	if(ms>4) setConsoleColor(255, 0, 0);
	else if(ms>2) setConsoleColor(255, 255, 0);
	else setConsoleColor(0, 255, 0);
}

void resetConsoleColor() {
	std::cout<<"\033[0m";
}

struct ParticlesUI : olc::PixelGameEngine {
	ParticlesUI() {
		sAppName="Particles";
	}

	//sprite stuff
	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;
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

	bool to_time=false;

	vf2d* bulk_start=nullptr;

	std::list<Particle*> grab_particles;

	//simulation
	Solver solver;

	float gravity=100;

	//graphics stuff
	bool show_grid=false;
	bool show_density=false;
	bool show_energy_chart=false;
	bool show_velocity=false;

	Chart energy_chart;
	float energy_chart_timer=0;

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
			energy_chart=Chart(250, {tl, br});
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

	void handleUserInput(float dt) {
		//add random particles in radius around mouse
		adding=GetMouse(olc::Mouse::LEFT).bHeld;
		if(adding) {
			float pos_rad=cmn::random(selection_radius);
			vf2d offset=cmn::polar(pos_rad, cmn::random(2*cmn::Pi));

			//random size and color
			float ptc_rad=cmn::random(5, 10);
			float speed=dt*cmn::random(20);
			Particle temp(mouse_pos+offset, ptc_rad);
			temp.oldpos-=cmn::polar(speed, cmn::random(2*cmn::Pi));

			const auto& p=solver.addParticle(temp);
			if(p) p->id=rand()%7;
		}

		//add grid of particles inside dragged rectangle
		const auto bulk_action=GetKey(olc::Key::B);
		if(bulk_action.bPressed) {
			bulk_start=new vf2d(mouse_pos);
		}
		if(bulk_action.bReleased) {
			if(bulk_start) {
				cmn::AABB box;
				box.fitToEnclose(*bulk_start);
				box.fitToEnclose(mouse_pos);
				float max_rad=7;
				int num_x=(box.max.x-box.min.x)/max_rad/2;
				int num_y=(box.max.y-box.min.y)/max_rad/2;
				for(int i=0; i<num_x; i++) {
					float x=map(i, 0, num_x-1, box.min.x, box.max.x);
					for(int j=0; j<num_y; j++) {
						float y=map(j, 0, num_y-1, box.min.y, box.max.y);
						Particle temp({x, y}, cmn::random(4, max_rad));

						const auto& p=solver.addParticle(temp);
						if(p) p->id=rand()%7;
					}
				}
			}

			delete bulk_start;
			bulk_start=nullptr;
		}

		removing=GetMouse(olc::Mouse::RIGHT).bHeld;
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
		if(GetKey(olc::Key::E).bPressed) {
			show_energy_chart^=true;
			energy_chart.reset();
		}
		if(GetKey(olc::Key::V).bPressed) show_velocity^=true;
	}

	int handlePhysics(float dt) {
		int checks=0;
		const int sub_steps=3;
		float sub_dt=dt/sub_steps;
		for(int i=0; i<sub_steps; i++) {
			checks+=solver.solveCollisions();

			solver.applyGravity(gravity);

			solver.updateKinematics(sub_dt);
		}

		//update energy chart
		if(energy_chart_timer<0) {
			energy_chart_timer+=.05f;

			if(show_energy_chart) {
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
		}
		energy_chart_timer-=dt;

		return checks;
	}

	void update(float dt) {
		//basic timestep clamping
		if(dt>1/30.f) dt=1/30.f;
		mouse_pos=GetMousePos();

		handleUserInput(dt);

		cmn::Stopwatch physics_watch;
		if(to_time) physics_watch.start();

		int phys_checks=handlePhysics(dt);

		if(to_time) {
			physics_watch.stop();

			auto dur=physics_watch.getMicros();
			float dur_ms=dur/1000.f;
			std::cout<<"physics: ";
			//choose color based on performance
			setConsoleColorFromPerf(dur_ms);
			std::cout<<dur<<"us ("<<dur_ms<<" ms)";
			resetConsoleColor();
			std::cout<<'\n';

			int num=solver.particles.size();
			std::cout<<"at "<<num<<" particles:\n";
			int brute=num*(num-1)/2;
			std::cout<<"  brute force: "<<brute<<" tests\n";
			std::cout<<"  spacial hash: "<<phys_checks<<" tests\n";
			int pct=100.f*(brute-phys_checks)/brute;
			std::cout<<"  "<<int(pct)<<"% faster\n";
		}
	}

	void DrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float rad, olc::Pixel col) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=sub.perp()/len;

		float angle=std::atan2f(sub.y, sub.x);
		DrawRotatedDecal(a-rad*tang, prim_rect_dec, angle, {0, 0}, {len, 2*rad}, col);
	}

	void FillCircleDecal(const olc::vf2d& pos, float rad, olc::Pixel col) {
		olc::vf2d offset(rad, rad);
		olc::vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	void render(float dt) {
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
			cmn::AABB box;
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
		if(!show_density&&!show_velocity) {
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

		if(show_velocity) {
			for(int i=0; i<solver.getNumCellX(); i++) {
				float x=solver.getCellSize()*(.5f+i);
				for(int j=0; j<solver.getNumCellY(); j++) {
					float y=solver.getCellSize()*(.5f+j);

					//get weighted avg cell velocity
					vf2d v_sum;
					float m_sum=0;
					const auto& particles=solver.getCell(i, j);
					for(const auto& k:particles) {
						const auto& p=solver.particles[k];
						v_sum+=p.mass*(p.pos-p.oldpos);
						m_sum+=p.mass;
					}
					vf2d vel=v_sum/m_sum;
					//actual movement in a given frame
					vel/=dt;
					
					//color based on speed?
					vf2d pos(x, y);
					DrawThickLine(pos, pos+vel, 1, olc::BLUE);
				}
			}
		}

		//show energy chart
		if(show_energy_chart) {
			const auto& box=energy_chart.bounds;
			vf2d size=box.max-box.min;
			FillRectDecal(box.min-2, size+4, olc::BLACK);
			FillRectDecal(box.min, size, olc::GREY);
			
			//how can i draw triangles?
			float max=energy_chart.getMaxValue();
			float px, py;
			for(int i=0; i<energy_chart.getNum();i++) {
				float x=map(i, 0, energy_chart.getNum()-1, box.min.x, box.max.x);
				float y=map(energy_chart.values[i], 0, max, box.max.y, box.min.y);
				if(i!=0) DrawThickLine({x, y}, {px, py}, 1, olc::BLUE);
				px=x, py=y;
			}
			FillRectDecal(box.min-vf2d(1,1), vf2d(50, 10), olc::BLACK);
			DrawStringDecal(box.min, "Energy", olc::WHITE);
		}
	}

	bool OnUserUpdate(float dt) override {
		to_time=to_time=GetKey(olc::Key::T).bPressed;
		if(to_time) {
			std::cout<<"\n\033[1;4m";
			std::cout<<"Diagnostic Info:";
			resetConsoleColor();
			std::cout<<'\n';
		}

		update(dt);

		cmn::Stopwatch render_watch;
		if(to_time) render_watch.start();

		render(dt);

		if(to_time) {
			render_watch.stop();
			auto dur=render_watch.getMicros();
			float dur_ms=dur/1000.f;
			std::cout<<"render: ";
			//choose color based on performance
			setConsoleColorFromPerf(dur_ms);
			std::cout<<dur<<"us ("<<dur_ms<<" ms)";
			resetConsoleColor();
			std::cout<<'\n';
		}

		return true;
	}
};