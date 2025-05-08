#include "common/3d/engine_3d.h"

#include "particle.h"

//#include "constraint.h"

#include "spring.h"

#include "common/utils.h"

#include <unordered_map>

struct Cloth3DUI : cmn::Engine3D {
	Cloth3DUI() {
		sAppName="3D Cloth";
	}

	//camera positioning
	float cam_yaw=-cmn::Pi/2;
	float cam_pitch=-.1f;

	//scene stuff
	const AABB3 scene_bounds{{-3, -5, -3}, {3, 5, 3}};
	std::list<Particle> particles;
	std::list<Spring> springs;
	const vf3d gravity{0, -9.8f, 0};

	const float time_step=1/120.f;
	float update_timer=0;

	olc::Sprite* gradient1_spr=nullptr;
	olc::Sprite* gradient2_spr=nullptr;
	olc::Sprite* gradient3_spr=nullptr;
	int gradient_to_use=0;

	olc::Pixel sampleGradient(float u) const {
		switch(gradient_to_use) {
			case 0: return gradient1_spr->Sample(u, 0);
			case 1: return gradient2_spr->Sample(u, 0);
			case 2: return gradient3_spr->Sample(u, 0);
		}
		return olc::BLACK;
	}

	bool user_create() override {
		cam_pos={0, 0, 3};

		gradient1_spr=new olc::Sprite("assets/gradient_1.png");
		gradient2_spr=new olc::Sprite("assets/gradient_2.png");
		gradient3_spr=new olc::Sprite("assets/gradient_3.png");

		const int sz=50;
		auto ix=[] (int i, int j) {
			return i+sz*j;
		};
		Particle** grid=new Particle*[sz*sz];
		//allocate particles on top
		for(int i=0; i<sz; i++) {
			float x=cmn::map(i, 0, sz-1, scene_bounds.min.x, scene_bounds.max.x);
			for(int j=0; j<sz; j++) {
				float z=cmn::map(j, 0, sz-1, scene_bounds.min.z, scene_bounds.max.z);
				Particle p({x, scene_bounds.max.y, z});
				if(i==0||j==0||i==(sz-1)||j==(sz-1)) p.locked=true;
				particles.push_back(p);
				grid[ix(i, j)]=&particles.back();
			}
		}

		//connect em up!
		const float k=572.4f;
		const float d=89.3f;
		for(int i=0; i<sz; i++) {
			for(int j=0; j<sz; j++) {
				if(i>0) springs.emplace_back(grid[ix(i, j)], grid[ix(i-1, j)], k, d);
				if(j>0) springs.emplace_back(grid[ix(i, j)], grid[ix(i, j-1)], k, d);
			}
		}

		delete[] grid;

		return true;
	}

	void handleUserInput(float dt) {
		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>cmn::Pi/2) cam_pitch=cmn::Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-cmn::Pi/2) cam_pitch=.001f-cmn::Pi/2;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw-=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw+=dt;

		//polar to cartesian
		cam_dir=vf3d(
			std::cosf(cam_yaw)*std::cosf(cam_pitch),
			std::sinf(cam_pitch),
			std::sinf(cam_yaw)*std::cosf(cam_pitch)
		);

		//move up, down
		if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*dt;
		if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::cosf(cam_yaw), 0, std::sinf(cam_yaw));
		if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*dt*fb_dir;
		if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*dt*lr_dir;
		if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*dt*lr_dir;

		//gradient choice!
		if(GetKey(olc::Key::K1).bPressed) gradient_to_use=0;
		if(GetKey(olc::Key::K2).bPressed) gradient_to_use=1;
		if(GetKey(olc::Key::K3).bPressed) gradient_to_use=2;
	}

	bool user_update(float dt) override {
		handleUserInput(dt);
		
		//fixed timestep to ensure similar update across
		//multiple framerates?
		update_timer+=dt;
		while(update_timer>time_step) {
			for(auto& s:springs) {
				s.update();
			}

			for(auto& p:particles) {
				p.applyForce(gravity);
			
				p.update(time_step);
			}

			update_timer-=time_step;
		}

		return true;
	}

	void addAABB(const AABB3& box, const olc::Pixel& col) {
		//corner vertexes
		const vf3d& v0=box.min, & v7=box.max;
		vf3d v1(v7.x, v0.y, v0.z);
		vf3d v2(v0.x, v7.y, v0.z);
		vf3d v3(v7.x, v7.y, v0.z);
		vf3d v4(v0.x, v0.y, v7.z);
		vf3d v5(v7.x, v0.y, v7.z);
		vf3d v6(v0.x, v7.y, v7.z);
		//bottom
		Line l1{v0, v1}; l1.col=col;
		lines_to_project.push_back(l1);
		Line l2{v1, v3}; l2.col=col;
		lines_to_project.push_back(l2);
		Line l3{v3, v2}; l3.col=col;
		lines_to_project.push_back(l3);
		Line l4{v2, v0}; l4.col=col;
		lines_to_project.push_back(l4);
		//sides
		Line l5{v0, v4}; l5.col=col;
		lines_to_project.push_back(l5);
		Line l6{v1, v5}; l6.col=col;
		lines_to_project.push_back(l6);
		Line l7{v2, v6}; l7.col=col;
		lines_to_project.push_back(l7);
		Line l8{v3, v7}; l8.col=col;
		lines_to_project.push_back(l8);
		//top
		Line l9{v4, v5}; l9.col=col;
		lines_to_project.push_back(l9);
		Line l10{v5, v7}; l10.col=col;
		lines_to_project.push_back(l10);
		Line l11{v7, v6}; l11.col=col;
		lines_to_project.push_back(l11);
		Line l12{v6, v4}; l12.col=col;
		lines_to_project.push_back(l12);
	}

	bool user_geometry() override {
		//add scene bounds
		addAABB(scene_bounds, olc::BLACK);

		//particle coloring
		std::unordered_map<const Spring*, float> strains;
		for(const auto& s:springs) {
			float len=(s.a->pos-s.b->pos).mag();
			float strain=std::fabsf(s.rest_len-len)/s.rest_len;
			strains[&s]=strain;
		}

		//find max strain
		float max_strain=1e-3f;
		for(const auto& s:springs) {
			if(strains[&s]>max_strain) {
				max_strain=strains[&s];
			}
		}

		//add all lines
		for(const auto& s:springs) {
			Line l{s.a->pos, s.b->pos};
			float u=strains[&s]/max_strain;
			l.col=sampleGradient(u);
			lines_to_project.push_back(l);
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::GREY);

		render3D();

		return true;
	}
};

int main() {
	Cloth3DUI c3dui;
	if(c3dui.Construct(360, 540, 1, 1, false, true)) c3dui.Start();

	return 0;
}