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

void setConsoleColorFromPerf(float ms, int good_upper, int bad_lower) {
	//red vs yellow vs green
	if(ms>bad_lower) setConsoleColor(255, 0, 0);
	else if(ms>good_upper) setConsoleColor(255, 255, 0);
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

	//ui stuff
	vf2d mouse_pos;

	float selection_radius=35;

	bool adding=false, removing=false;

	bool to_time=false;

	vf2d* bulk_start=nullptr;

	std::list<int> grab_particles;

	int num_checks=0;

	//simulation
	Solver* solver=nullptr;

	const float time_step=1/120.f;
	const int num_sub_steps=3;
	float sub_time_step=time_step/num_sub_steps;
	float update_timer=0;

	float gravity=100;

	//graphics stuff
	bool show_cells=false;
	bool show_density=false;
	bool show_energy_chart=false;
	bool show_velocity=false;

	bool show_background=true;
	olc::Sprite* background_spr=nullptr;
	olc::Sprite* gradient_spr=nullptr;

	Chart energy_chart;
	float energy_chart_timer=0;

	bool OnUserCreate() override {
		//make some "primitives" to draw with
		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);
		{
			int sz=512;
			prim_circ_spr=new olc::Sprite(sz, sz);
			SetDrawTarget(prim_circ_spr);
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2);
			SetDrawTarget(nullptr);
			prim_circ_dec=new olc::Decal(prim_circ_spr);
		}

		//initialize solver
		solver=new Solver(10000, {vf2d(0, 0), vf2d(ScreenWidth(), ScreenHeight())});

		//load background image
		background_spr=new olc::Sprite("assets/cat.png");
		
		//density gradient
		gradient_spr=new olc::Sprite("assets/gradient.png");

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
		delete prim_rect_dec;
		delete prim_rect_spr;

		delete background_spr;
		delete gradient_spr;

		delete solver;

		return true;
	}

	void handleUserInput(float dt) {
		//add random particles in radius around mouse
		adding=GetMouse(olc::Mouse::LEFT).bHeld;
		if(adding) {
			//add as many as possible(ish)
			for(int i=0; i<100; i++) {
				float pos_rad=cmn::random(selection_radius);
				vf2d offset=cmn::polar(pos_rad, cmn::random(2*cmn::Pi));

				//random size and velocity
				float ptc_rad=cmn::random(3, 6);
				float speed=dt*cmn::random(35);
				Particle temp(mouse_pos+offset, ptc_rad);
				temp.oldpos-=cmn::polar(speed, cmn::random(2*cmn::Pi));

				//random color
				temp.col=olc::Pixel(rand()%256, rand()%256, rand()%256);

				//try to add particle
				solver->addParticle(temp);
			}
		}

		//add grid of particles inside dragged rectangle
		const auto bulk_action=GetKey(olc::Key::A);
		if(bulk_action.bPressed) bulk_start=new vf2d(mouse_pos);
		if(bulk_action.bReleased) {
			if(bulk_start) {
				//determine spacing
				cmn::AABB box;
				box.fitToEnclose(*bulk_start);
				box.fitToEnclose(mouse_pos);
				float max_rad=5.5f;
				int num_x=(box.max.x-box.min.x)/max_rad/2;
				int num_y=(box.max.y-box.min.y)/max_rad/2;
				//grid of random particles
				for(int i=0; i<num_x; i++) {
					float x=map(i, 0, num_x-1, box.min.x, box.max.x);
					for(int j=0; j<num_y; j++) {
						float y=map(j, 0, num_y-1, box.min.y, box.max.y);
						Particle temp({x, y}, cmn::random(2.5f, max_rad));
						
						//random color
						temp.col=olc::Pixel(rand()%256, rand()%256, rand()%256);

						solver->addParticle(temp);
					}
				}
			}

			delete bulk_start;
			bulk_start=nullptr;
		}

		//remove particles inside selection radius
		removing=GetMouse(olc::Mouse::RIGHT).bHeld;
		if(removing) {
			grab_particles.clear();
			for(int i=0; i<solver->getNumParticles(); i++) {
				const auto& p=solver->particles[i];
				if((mouse_pos-p.pos).mag()<selection_radius) {
					solver->removeParticle(i);
				}
			}
		}

		//select all particles inside selection radius
		const auto grab_action=GetKey(olc::Key::G);
		if(grab_action.bPressed) {
			grab_particles.clear();
			for(int i=0; i<solver->getNumParticles(); i++) {
				float d_sq=(mouse_pos-solver->particles[i].pos).mag2();
				if(d_sq<selection_radius*selection_radius) {
					grab_particles.push_back(i);
				}
			}
		}
		//apply spring force
		if(grab_action.bHeld) {
			for(const auto& i:grab_particles) {
				auto& p=solver->particles[i];
				//F=kx
				float k=13*p.mass;
				vf2d sub=mouse_pos-p.pos;
				p.applyForce(k*sub);
			}
		}
		if(grab_action.bReleased) {
			grab_particles.clear();
		}

		//graphics toggles
		if(GetKey(olc::Key::C).bPressed) show_cells^=true;
		if(GetKey(olc::Key::D).bPressed) show_density^=true;
		if(GetKey(olc::Key::E).bPressed) {
			show_energy_chart^=true;
			energy_chart.reset();
		}
		if(GetKey(olc::Key::V).bPressed) show_velocity^=true;
		if(GetKey(olc::Key::B).bPressed) show_background^=true;
	}

	void handlePhysics(float dt) {
		//ensure similar update across multiple framerates
		update_timer+=dt;
		while(update_timer>time_step) {
			num_checks=0;

			for(int i=0; i<num_sub_steps; i++) {
				num_checks+=solver->solveCollisions();

				solver->applyGravity(gravity);

				solver->updateKinematics(sub_time_step);
			}

			update_timer-=time_step;
		}

		//update energy chart
		if(energy_chart_timer<0) {
			energy_chart_timer+=.05f;

			if(show_energy_chart) {
				float total_energy=0;
				for(int i=0; i<solver->getNumParticles(); i++) {
					const auto& p=solver->particles[i];

					//mgh
					float h=solver->getBounds().max.y-p.pos.y;
					total_energy+=p.mass*gravity*h;

					//1/2mv^2
					vf2d vel=p.pos-p.oldpos;
					total_energy+=.5f*p.mass*vel.dot(vel);
				}
				energy_chart.update(total_energy);
			}
		}
		energy_chart_timer-=dt;
	}

	void update(float dt) {
		//basic timestep clamping
		if(dt>1/30.f) dt=1/30.f;
		mouse_pos=GetMousePos();

		handleUserInput(dt);

		cmn::Stopwatch physics_watch;
		if(to_time) physics_watch.start();

		handlePhysics(dt);

		if(to_time) {
			physics_watch.stop();

			auto dur=physics_watch.getMicros();
			float dur_ms=dur/1000.f;
			std::cout<<"physics: ";
			//choose color based on performance
			setConsoleColorFromPerf(dur_ms, 2, 4);
			std::cout<<dur<<"us ("<<dur_ms<<" ms)";
			resetConsoleColor();
			std::cout<<'\n';

			//show difference between this method
			//  and a naive brute force approach
			int num=solver->getNumParticles();
			std::cout<<"at "<<num<<" particles:\n";
			int brute=num_sub_steps*num*(num-1)/2;
			std::cout<<"  brute force: "<<brute<<" tests\n";
			std::cout<<"  spacial hash: "<<num_checks<<" tests\n";
			int pct=100.f*(brute-num_checks)/num_checks;
			int r=0, g=0;
			std::string verdict;
			if(pct<0) pct=-pct, r=255, verdict="slower";
			else g=255, verdict="faster";
			setConsoleColor(r, g, 0);
			std::cout<<"  "<<pct<<"% "<<verdict;
			resetConsoleColor();
			std::cout<<'\n';
		}
	}

#pragma region RENDER HELPERS
	void DrawThickLineDecal(const olc::vf2d& a, const olc::vf2d& b, float w, olc::Pixel col) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=sub.perp()/len;

		float angle=std::atan2f(sub.y, sub.x);
		DrawRotatedDecal(a-.5f*w*tang, prim_rect_dec, angle, {0, 0}, {len, w}, col);
	}

	void FillCircleDecal(const olc::vf2d& pos, float rad, olc::Pixel col) {
		olc::vf2d offset(rad, rad);
		olc::vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	void DrawThickCircleDecal(const olc::vf2d& pos, float rad, float w, const olc::Pixel& col) {
		const int num=32;
		olc::vf2d first, prev;
		for(int i=0; i<num; i++) {
			float angle=2*cmn::Pi*i/num;
			olc::vf2d curr(pos.x+rad*std::cos(angle), pos.y+rad*std::sin(angle));
			FillCircleDecal(curr, .5f*w, col);
			if(i==0) first=curr;
			else DrawThickLineDecal(prev, curr, w, col);
			prev=curr;
		}
		DrawThickLineDecal(first, prev, w, col);
	}
#pragma endregion

	void render(float dt) {
		Clear(olc::BLACK);

		//show addition radius
		if(adding) {
			DrawThickCircleDecal(mouse_pos, selection_radius, 2, olc::GREEN);
		}

		//show deletion radius
		if(removing) {
			DrawThickCircleDecal(mouse_pos, selection_radius, 2, olc::RED);
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

		//show grab particle links
		for(const auto& i:grab_particles) {
			const auto& p=solver->particles[i];
			DrawLineDecal(mouse_pos, p.pos, olc::WHITE);
		}

		//show particles
		if(!show_density&&!show_velocity) {
			for(int i=0; i<solver->getNumParticles(); i++) {
				const auto& p=solver->particles[i];
				olc::Pixel col=p.col;
				if(show_background) {
					//particle position to sample texture
					float u=p.pos.x/ScreenWidth();
					float v=p.pos.y/ScreenHeight();
					col=background_spr->Sample(u, v);
				}
				FillCircleDecal(p.pos, p.rad, col);
			}
		}

		//draw solver hash grid
		if(show_cells) {
			//vertical lines
			for(int i=0; i<=solver->getNumCellX(); i++) {
				float x=solver->getCellSize()*i;
				vf2d top(x, 0), btm(x, ScreenHeight());
				if(i%5==0) DrawThickLineDecal(top, btm, 2, olc::GREY);
				else DrawLineDecal(top, btm, olc::GREY);
			}

			//horizontal lines
			for(int j=0; j<=solver->getNumCellY(); j++) {
				float y=solver->getCellSize()*j;
				vf2d lft(0, y), rgt(ScreenWidth(), y);
				if(j%5==0) DrawThickLineDecal(lft, rgt, 2, olc::GREY);
				else DrawLineDecal(lft, rgt, olc::GREY);
			}
		}

		//draw squares showing "density"
		//  or number of particles per cell
		if(show_density) {
			solver->fillCells();
			
			//find max cell "mass"
			float record=0;
			for(int i=0; i<solver->getNumCellX()*solver->getNumCellY(); i++) {
				float sum=0;
				int st=solver->grid_heads[i];
				for(int j=st; j!=-1; j=solver->particle_next[j]) {
					sum+=solver->particles[j].mass;
				}
				if(sum>record) record=sum;
			}

			if(record>0) {
				//for each cell
				const float sz=solver->getCellSize();
				for(int i=0; i<solver->getNumCellX(); i++) {
					for(int j=0; j<solver->getNumCellY(); j++) {
						//get cell "mass"
						float sum=0;
						int st=solver->grid_heads[solver->cellIX(i, j)];
						for(int k=st; k!=-1; k=solver->particle_next[k]) {
							sum+=solver->particles[k].mass;
						}
						if(sum==0) continue;

						//sample gradient texture
						float u=sum/record;
						olc::Pixel col=gradient_spr->Sample(u, 0);

						//draw cell as rect
						float x=sz*i, y=sz*j;
						FillRectDecal({x, y}, {sz, sz}, col);
					}
				}
			}
		}

		if(show_velocity) {
			const float sz=solver->getCellSize();
			for(int i=0; i<solver->getNumCellX(); i++) {
				float x=sz*(.5f+i);
				for(int j=0; j<solver->getNumCellY(); j++) {
					float y=sz*(.5f+j);

					//get weighted avg cell velocity
					vf2d v_sum;
					float m_sum=0;
					int st=solver->grid_heads[solver->cellIX(i, j)];
					for(int i=st; i!=-1; i=solver->particle_next[i]) {
						const auto& p=solver->particles[i];
						v_sum+=p.mass*(p.pos-p.oldpos);
						m_sum+=p.mass;
					}
					vf2d vel=v_sum/m_sum;

					//actual movement in a given frame
					vel/=dt;

					//color based on speed?
					vf2d pos(x, y);
					DrawThickLineDecal(pos, pos+vel, 1, olc::BLUE);
				}
			}
		}

		//show energy chart
		if(show_energy_chart) {
			const auto& box=energy_chart.bounds;
			vf2d size=box.max-box.min;
			FillRectDecal(box.min-2, size+4, olc::BLACK);
			FillRectDecal(box.min, size, olc::GREY);

			//how to draw triangles?
			float max=energy_chart.getMaxValue();
			float px, py;
			for(int i=0; i<energy_chart.getNum(); i++) {
				float x=map(i, 0, energy_chart.getNum()-1, box.min.x, box.max.x);
				float y=map(energy_chart.values[i], 0, max, box.max.y, box.min.y);
				if(i!=0) DrawThickLineDecal({x, y}, {px, py}, 1, olc::BLUE);
				px=x, py=y;
			}
			FillRectDecal(box.min-vf2d(1, 1), vf2d(50, 10), olc::BLACK);
			DrawStringDecal(box.min, "Energy", olc::WHITE);
		}
	}

	//encapsulation for timing certain aspects of this program
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
			setConsoleColorFromPerf(dur_ms, 7, 15);
			std::cout<<dur<<"us ("<<dur_ms<<" ms)";
			resetConsoleColor();
			std::cout<<'\n';
		}

		return true;
	}
};