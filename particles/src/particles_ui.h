#define OLC_PGE_APPLICATION
#include "olc/include/olcPixelGameEngine.h"
using olc::vf2d;

#include "solver.h"

#include "chart.h"

struct ParticlesUI : olc::PixelGameEngine {
	ParticlesUI() {
		sAppName="Particles";
	}

	//sprite stuff
	olc::Renderable prim_rect, prim_circ;

	//ui stuff
	vf2d mouse_pos;

	float selection_radius=35;

	bool adding=false, removing=false;

	vf2d* bulk_start=nullptr;

	std::list<int> grab_particles;

	int num_checks=0;

	//simulation
	bool update_phys=true;
	Solver* solver=nullptr;

	const float time_step=1/120.f;
	const int num_sub_steps=3;
	float sub_time_step=time_step/num_sub_steps;
	float update_timer=0;

	const vf2d gravity{0, 100};

	//graphics stuff
	bool show_grid=false;
	bool show_energy=false;

	bool show_image=true;
	olc::Sprite* image_spr=nullptr;
	olc::Sprite* gradient_spr=nullptr;

	Chart energy_chart;
	float energy_chart_timer=0;

	bool help_menu=false;

	bool OnUserCreate() override {
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

		//initialize solver
		solver=new Solver(10000, {vf2d(0, 0), vf2d(ScreenWidth(), ScreenHeight())});

		//load background image
		image_spr=new olc::Sprite("assets/cat.png");

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
		delete image_spr;
		delete gradient_spr;

		delete solver;

		return true;
	}

#pragma region UPDATE HELPERS
	void handleAddActionUpdate(float dt) {
		//add as many as possible(ish)
		for(int i=0; i<100; i++) {
			float offset_rad=cmn::randFloat(selection_radius);
			float offset_angle=cmn::randFloat(2*cmn::Pi);
			vf2d offset=offset_rad*vf2d(std::cos(offset_angle), std::sin(offset_angle));

			//random size and velocity
			float rad=cmn::randFloat(3, 11);
			Particle temp(mouse_pos+offset, rad);
			float speed=dt*cmn::randFloat(35);
			float vel_angle=cmn::randFloat(2*cmn::Pi);
			temp.oldpos-=speed*vf2d(std::cos(vel_angle), std::sin(vel_angle));

			//random color
			temp.col=olc::Pixel(std::rand()%256, std::rand()%256, std::rand()%256);

			//try to add particle
			solver->addParticle(temp);
		}
	}

	void handleBulkActionBegin() {
		bulk_start=new vf2d(mouse_pos);
	}

	void handleBulkActionEnd() {
		if(!bulk_start) return;

		//find bounds
		cmn::AABB box;
		box.fitToEnclose(*bulk_start);
		box.fitToEnclose(mouse_pos);
		vf2d size=box.max-box.min;

		//determine spacing
		float max_rad=5.5f;
		int num_x=1+size.x/max_rad/2;
		int num_y=1+size.y/max_rad/2;

		//grid of random particles
		for(int i=0; i<num_x; i++) {
			float u=i/(num_x-1.f);
			for(int j=0; j<num_y; j++) {
				float v=j/(num_y-1.f);
				Particle temp(box.min+size*vf2d(u, v), cmn::randFloat(2.5f, max_rad));

				//random color
				temp.col=olc::Pixel(std::rand()%256, std::rand()%256, std::rand()%256);

				solver->addParticle(temp);
			}
		}

		delete bulk_start;
		bulk_start=nullptr;
	}

	void handleGrabActionBegin() {
		grab_particles.clear();

		for(int i=0; i<solver->getNumParticles(); i++) {
			float d_sq=(mouse_pos-solver->particles[i].pos).mag2();
			if(d_sq<selection_radius*selection_radius) {
				grab_particles.push_back(i);
			}
		}
	}

	//apply spring force
	void handleGrabActionUpdate() {
		for(const auto& i:grab_particles) {
			auto& p=solver->particles[i];
			//F=kx
			float k=13*p.mass;
			vf2d sub=mouse_pos-p.pos;
			p.applyForce(k*sub);
		}
	}

	void handleGrabActionEnd() {
		grab_particles.clear();
	}

	void handleRemoveActionUpdate() {
		grab_particles.clear();

		for(int i=0; i<solver->getNumParticles(); i++) {
			const auto& p=solver->particles[i];
			if((mouse_pos-p.pos).mag()<selection_radius) {
				solver->removeParticle(i);
			}
		}
	}

	void handleUserInput(float dt) {
		//add random particles in radius around mouse
		adding=GetKey(olc::Key::A).bHeld;
		if(adding) handleAddActionUpdate(dt);

		//add grid of particles inside dragged rectangle
		const auto bulk_action=GetKey(olc::Key::B);
		if(bulk_action.bPressed) handleBulkActionBegin();
		if(bulk_action.bReleased) handleBulkActionEnd();

		//remove particles inside selection radius
		removing=GetKey(olc::Key::X).bHeld;
		if(removing) handleRemoveActionUpdate();

		//select all particles inside selection radius
		const auto grab_action=GetMouse(olc::Mouse::LEFT);
		if(grab_action.bPressed) handleGrabActionBegin();
		if(grab_action.bHeld) handleGrabActionUpdate();
		if(grab_action.bReleased) handleGrabActionEnd();

		//debug toggles
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;
		if(GetKey(olc::Key::E).bPressed) {
			show_energy^=true;

			energy_chart.reset();
		}
		if(GetKey(olc::Key::I).bPressed) show_image^=true;
		if(GetKey(olc::Key::P).bPressed) update_phys^=true;
		if(GetKey(olc::Key::H).bPressed) help_menu^=true;
	}

	void handlePhysics(float dt) {
		//ensure similar update across multiple framerates
		update_timer+=dt;
		while(update_timer>time_step) {
			num_checks=0;

			for(int i=0; i<num_sub_steps; i++) {
				num_checks+=solver->solveCollisions();

				solver->accelerate(gravity);

				solver->updateKinematics(sub_time_step);
			}

			update_timer-=time_step;
		}

		//update energy chart
		if(energy_chart_timer<0) {
			energy_chart_timer+=.05f;

			if(show_energy) {
				float total_energy=0;
				for(int i=0; i<solver->getNumParticles(); i++) {
					const auto& p=solver->particles[i];

					//mgh
					float h=solver->getBounds().max.y-p.pos.y;
					total_energy+=p.mass*gravity.mag()*h;

					//1/2mv^2
					vf2d vel=p.pos-p.oldpos;
					total_energy+=.5f*p.mass*vel.dot(vel);
				}
				energy_chart.update(total_energy);
			}
		}
		energy_chart_timer-=dt;
	}
#pragma endregion

	void update(float dt) {
		//basic timestep clamping
		if(dt>1/30.f) dt=1/30.f;
		mouse_pos=GetMousePos();

		handleUserInput(dt);

		if(update_phys) handlePhysics(dt);
	}

#pragma region RENDER HELPERS
	void DrawThickLineDecal(const vf2d& a, const vf2d& b, float w, olc::Pixel col) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=sub.perp()/len;

		float angle=std::atan2(sub.y, sub.x);
		DrawRotatedDecal(a-.5f*w*tang, prim_rect.Decal(), angle, {0, 0}, {len, w}, col);
	}

	void FillCircleDecal(const vf2d& pos, float rad, olc::Pixel col) {
		vf2d offset(rad, rad);
		vf2d scale{2*rad/prim_circ.Sprite()->width, 2*rad/prim_circ.Sprite()->height};
		DrawDecal(pos-offset, prim_circ.Decal(), scale, col);
	}

	void DrawThickCircleDecal(const vf2d& pos, float rad, float w, const olc::Pixel& col) {
		const int num=32;
		vf2d first, prev;
		for(int i=0; i<num; i++) {
			float angle=2*cmn::Pi*i/num;
			vf2d curr(pos.x+rad*std::cos(angle), pos.y+rad*std::sin(angle));
			FillCircleDecal(curr, .5f*w, col);
			if(i==0) first=curr;
			else DrawThickLineDecal(prev, curr, w, col);
			prev=curr;
		}
		DrawThickLineDecal(first, prev, w, col);
	}

	//show solver hash grid
	void renderGrid() {
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

	//addition box
	void renderBulkAction() {
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
	void renderGrabAction() {
		for(const auto& i:grab_particles) {
			const auto& p=solver->particles[i];
			DrawLineDecal(mouse_pos, p.pos, olc::WHITE);
		}
	}

	void renderParticles() {
		for(int i=0; i<solver->getNumParticles(); i++) {
			const auto& p=solver->particles[i];
			olc::Pixel col=p.col;
			if(show_image) {
				//particle position to sample texture
				float u=p.pos.x/ScreenWidth();
				float v=p.pos.y/ScreenHeight();
				col=image_spr->Sample(u, v);
			}
			FillCircleDecal(p.pos, p.rad, col);
		}
	}

	void renderEnergyChart() {
		const auto& box=energy_chart.bounds;
		const vf2d size=box.max-box.min;
		FillRectDecal(box.min-2, size+4, olc::BLACK);
		FillRectDecal(box.min, size, olc::GREY);

		//how to draw triangles?
		float max=energy_chart.getMaxValue();
		vf2d prev;
		for(int i=0; i<energy_chart.getNum(); i++) {
			float u=i/(energy_chart.getNum()-1.f);
			float v=energy_chart.values[i]/max;
			vf2d curr=box.min+size*vf2d(u, v);
			if(i!=0) DrawThickLineDecal(curr, prev, 1, olc::BLUE);
			prev=curr;
		}
		FillRectDecal(box.min-vf2d(1, 1), vf2d(50, 10), olc::BLACK);
		DrawStringDecal(box.min, "Energy", olc::WHITE);
	}

	void renderHelpHints() {
		int cx=ScreenWidth()/2;
		if(help_menu) {
			DrawStringDecal(vf2d(8, 8), "General Controls");
			DrawStringDecal(vf2d(8, 16), "A for addition");
			DrawStringDecal(vf2d(8, 24), "B for bulk addition");
			DrawStringDecal(vf2d(8, 32), "X for removal");
			DrawStringDecal(vf2d(8, 40), "MouseLeft for grabbing");

			DrawStringDecal(vf2d(ScreenWidth()-8*19, 8), "Toggleable Options");
			DrawStringDecal(vf2d(ScreenWidth()-8*11, 16), "G for grid", show_grid?olc::WHITE:olc::RED);
			DrawStringDecal(vf2d(ScreenWidth()-8*13, 24), "E for energy", show_energy?olc::WHITE:olc::RED);
			DrawStringDecal(vf2d(ScreenWidth()-8*12, 32), "I for image", show_image?olc::WHITE:olc::RED);
			DrawStringDecal(vf2d(ScreenWidth()-8*17, 40), "P for pause/play", update_phys?olc::WHITE:olc::RED);

			DrawStringDecal(vf2d(cx-4*18, ScreenHeight()-8), "[Press H to close]");
		} else {
			DrawStringDecal(vf2d(cx-4*18, ScreenHeight()-8), "[Press H for help]");
		}
	}
#pragma endregion

	void render() {
		Clear(olc::BLACK);

		if(show_grid) renderGrid();

		//show addition radius
		if(adding) {
			DrawThickCircleDecal(mouse_pos, selection_radius, 2, olc::GREEN);
		}

		//show deletion radius
		if(removing) {
			DrawThickCircleDecal(mouse_pos, selection_radius, 2, olc::RED);
		}

		if(bulk_start) renderBulkAction();

		renderGrabAction();

		renderParticles();

		if(show_energy) renderEnergyChart();

		renderHelpHints();
	}

	bool OnUserUpdate(float dt) override {
		update(dt);

		render();

		return true;
	}
};