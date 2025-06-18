#include "common/3d/engine_3d.h"
using olc::vf2d;
using cmn::vf3d;
using cmn::Mat4;

constexpr float Pi=3.1415927f;

#include "phys/particle.h"
#include "phys/spring.h"

#include "perlin_noise.h"

struct IndexTriangle {
	int a=0, b=0, c=0;
};

struct Cloth3DUI : cmn::Engine3D {
	Cloth3DUI() {
		sAppName="3D Cloth";
	}

	//camera positioning
	float cam_yaw=-Pi/2;
	float cam_pitch=0;

	//user input stuff
	vf3d mouse_dir;

	bool update_phys=true;
	bool show_outlines=false;
	bool show_bounds=false;

	Particle* held_ptc=nullptr;
	vf3d held_plane;

	//physics stuff
	float total_dt=0;

	int length=0, height=0;
	Particle* grid=nullptr;
	int ix(int i, int j) const { return i+length*j; }

	std::list<Spring> springs;
	std::list<IndexTriangle> surface;

	const vf3d gravity{0, 0, 0};

	PerlinNoise noise_gen;

	const float time_step=1/120.f;
	float update_timer=0;

	//graphics stuff
	olc::Sprite* strain_spr=nullptr;

	olc::Sprite* flag_spr[4];
	int flag_idx=0;

	bool user_create() override {
		srand(time(0));

		cam_pos={2.5f, 1, 2.83f};
		light_pos=cam_pos;

		//allocate grid
		length=40+rand()%11;
		height=30+rand()%11;
		grid=new Particle[length*height];

		//start cloth on on xy plane
		for(int i=0; i<length; i++) {
			float u=i/(length-1.f);
			float x=3*u;
			for(int j=0; j<height; j++) {
				float v=j/(height-1.f);
				float y=2.3f*v;
				Particle p({x, y, 0});
				p.uv={u, 1-v};
				if(i==0) p.locked=true;
				grid[ix(i, j)]=p;
			}
		}

		//connect em up!
		const float k=2256.47f;
		const float d=375.9f;
		for(int i=0; i<length; i++) {
			for(int j=0; j<height; j++) {
				if(i>0) springs.emplace_back(&grid[ix(i, j)], &grid[ix(i-1, j)], k, d);
				if(j>0) springs.emplace_back(&grid[ix(i, j)], &grid[ix(i, j-1)], k, d);
				if(i>0&&j>0) {
					springs.emplace_back(&grid[ix(i-1, j-1)], &grid[ix(i, j)], k, d);
					springs.emplace_back(&grid[ix(i-1, j)], &grid[ix(i, j-1)], k, d);
				}
			}
		}

		//tesselate surface
		for(int i=1; i<length; i++) {
			for(int j=1; j<height; j++) {
				const auto& k00=ix(i-1, j-1);
				const auto& k01=ix(i-1, j);
				const auto& k10=ix(i, j-1);
				const auto& k11=ix(i, j);
				surface.push_back({k00, k01, k10});
				surface.push_back({k01, k11, k10});
			}
		}

		noise_gen=PerlinNoise(time(0));

		//load gradient
		strain_spr=new olc::Sprite("assets/strain_gradient.png");
		
		//cloth texture
		flag_spr[0]=new olc::Sprite("assets/flag_usa.png");
		flag_spr[1]=new olc::Sprite("assets/flag_colorado.png");
		flag_spr[2]=new olc::Sprite("assets/flag_brazil.png");
		flag_spr[3]=new olc::Sprite("assets/flag_djibouti.png");

		return true;
	}

	bool user_destroy() override {
		delete strain_spr;
		
		for(int i=0; i<4; i++) {
			delete flag_spr[i];
		}

		delete[] grid;

		return true;
	}

	//mostly just camera controls!
	void handleUserInput(float dt) {
		//matrix could be singular...
		Mat4 invVP;
		bool invVP_avail=true;
		try {
			invVP=Mat4::inverse(mat_view*mat_proj);
		} catch(const std::exception& e) {
			invVP_avail=false;
		}

		//update mouse ray
		//screen -> world with inverse matrix
		if(invVP_avail) {
			float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
			float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
			vf3d clip(ndc_x, ndc_y, 1);
			vf3d world=clip*invVP;
			world/=world.w;

			mouse_dir=(world-cam_pos).norm();
		}

		//cant drag particle and move around at same time.
		if(!held_ptc) {
			//look up, down
			if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
			if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
			if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
			if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;

			//look left, right
			if(GetKey(olc::Key::LEFT).bHeld) cam_yaw-=dt;
			if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw+=dt;
		}

		//polar to cartesian
		cam_dir=vf3d(
			std::cos(cam_yaw)*std::cos(cam_pitch),
			std::sin(cam_pitch),
			std::sin(cam_yaw)*std::cos(cam_pitch)
		);

		//cant drag particle and move around at same time.
		if(!held_ptc) {
			//move up, down
			if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*dt;
			if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*dt;

			//move forward, backward
			vf3d fb_dir(std::cos(cam_yaw), 0, std::sin(cam_yaw));
			if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*dt*fb_dir;
			if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*dt*fb_dir;

			//move left, right
			vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
			if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*dt*lr_dir;
			if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*dt*lr_dir;
		}

		//set light pos
		if(GetKey(olc::Key::L).bPressed) light_pos=cam_pos;

		//debug toggles
		if(GetKey(olc::Key::ENTER).bPressed) update_phys^=true;
		if(GetKey(olc::Key::O).bPressed) show_outlines^=true;
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;

		//flag choice!
		if(GetKey(olc::Key::K1).bPressed) flag_idx=0;
		if(GetKey(olc::Key::K2).bPressed) flag_idx=1;
		if(GetKey(olc::Key::K3).bPressed) flag_idx=2;
		if(GetKey(olc::Key::K4).bPressed) flag_idx=3;

		//grab particles
		const auto hold_action=GetMouse(olc::Mouse::LEFT);
		if(hold_action.bPressed) {
			//get close particle
			float record=-1;
			held_ptc=nullptr;
			for(int i=0; i<length*height; i++) {
				//is particle under mouse?
				auto& p=grid[i];
				vf3d sub=p.pos-cam_pos;
				float perp_dist=sub.cross(mouse_dir).mag();
				if(perp_dist<.2f) {
					//is it closer than the others?
					float dist=sub.mag();
					if(record<0||dist<record) {
						record=dist;
						held_ptc=&p;
					}
				}
			}
			//where in space did drag start
			if(held_ptc) {
				held_plane=held_ptc->pos;
			}
		}
		if(hold_action.bReleased) {
			held_ptc=nullptr;
		}
	}

	vf3d getWind(vf3d pos, float time) {
		const float scl=.1f;
		float x_wind=2+3*noise_gen.noise(scl*pos.y, scl*pos.z, time);
		float z_wind=-1+2*noise_gen.noise(100+scl*pos.x, 100+scl*pos.y, 100+time);
		float y_wind=-.5f+noise_gen.noise(200+scl*pos.z, 200+scl*pos.x, 200+time);

		return {x_wind, y_wind, z_wind};
	}

	void handlePhysics() {
		//update springs
		for(auto& s:springs) {
			s.update();
		}

		//update particles
		for(int i=0; i<length*height; i++) {
			auto& p=grid[i];

			p.accelerate(getWind(p.pos, .25f*total_dt));

			p.accelerate(gravity);
			p.update(time_step);
		}
	}

	bool user_update(float dt) override {
		handleUserInput(dt);

		//update drag particle
		if(held_ptc) {
			vf3d pt=segIntersectPlane(cam_pos, cam_pos+mouse_dir, held_plane, cam_dir);
			Particle p_temp(pt);
			p_temp.locked=true;
			Spring s_temp(held_ptc, &p_temp, 570, 90);
			s_temp.rest_len=0;
			s_temp.update();
		}

		if(update_phys) {
			//ensure similar update across multiple framerates?
			update_timer+=dt;
			while(update_timer>time_step) {
				handlePhysics();

				update_timer-=time_step;
			}
		}

		//accumulate time
		total_dt+=dt;

		return true;
	}

	bool user_geometry() override {
		//realize surface
		for(const auto& pt:surface) {
			//twofaced to prevent culling
			cmn::Triangle t{
				grid[pt.a].pos, grid[pt.b].pos, grid[pt.c].pos,
				grid[pt.a].uv, grid[pt.b].uv, grid[pt.c].uv
			};
			tris_to_project.push_back(t);
			std::swap(t.p[0], t.p[1]), std::swap(t.t[0], t.t[1]);
			tris_to_project.push_back(t);
		}

		if(show_outlines) {
			//find max strain
			float max_strain=1e-3f;
			for(const auto& s:springs) {
				float strain=std::abs(s.strain);
				if(strain>max_strain) {
					max_strain=strain;
				}
			}

			//add all lines
			for(const auto& s:springs) {
				cmn::Line l{s.a->pos, s.b->pos};
				float u=std::abs(s.strain)/max_strain;
				l.col=strain_spr->Sample(u, 0);
				lines_to_project.push_back(l);
			}
		}

		//show cloth bounds
		if(show_bounds) {
			cmn::AABB3 box;
			for(int i=0; i<length*height; i++) {
				box.fitToEnclose(grid[i].pos);
			}
			addAABB(box, olc::BLACK);
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::VERY_DARK_GREY);

		resetBuffers();

		for(const auto& t:tris_to_draw) {
			FillTexturedDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
				flag_spr[flag_idx], t.col, t.id
			);
		}

		for(const auto& l:lines_to_draw) {
			DrawDepthLine(
				l.p[0].x, l.p[0].y, l.t[0].w,
				l.p[1].x, l.p[1].y, l.t[1].w,
				l.col, l.id
			);
		}

		if(!update_phys) {
			DrawRect(0, 0, ScreenWidth()-1, ScreenHeight()-1, olc::WHITE);
		}

		return true;
	}
};

int main() {
	Cloth3DUI c3dui;
	if(c3dui.Construct(480, 360, 1, 1, false, true)) c3dui.Start();

	return 0;
}