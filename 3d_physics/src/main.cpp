#include "common/3d/engine_3d.h"
using olc::vf2d;

constexpr float Pi=3.1415927f;

#include "phys/constraint.h"
#include "phys/spring.h"

vf3d projectOntoPlane(const vf3d& v, const vf3d& norm) {
	return v-norm.dot(v)*norm;
}

#include "shape.h"

struct Example : cmn::Engine3D {
	Example() {
		sAppName="3d physics";
	}

	//camera positioning
	float cam_yaw=-Pi/2;
	float cam_pitch=-.1f;

	//debug toggles
	bool show_bounds=false;

	//physics stuff
	const vf3d gravity{0, -9.8f, 0};
	std::list<Shape> shapes;

	const float time_step=1/120.f;
	float update_timer=0;

	bool user_create() override {
		cam_pos={0, 0, 3.5f};

		Shape box=Shape::makeBox({vf3d(-1, -2, -1), vf3d(1, 2, 1)});
		box.particles[0].locked=true;
		box.col=olc::GREEN;
		shapes.push_back(box);

		Shape ground=Shape::makeBox({vf3d(-4, -4, -4), vf3d(4, -3, 4)});
		ground.particles[0].locked=true;
		ground.particles[1].locked=true;
		ground.col=olc::BLUE;
		shapes.push_back(ground);

		return true;
	}

	//mostly camera controls
	void handleUserInput(float dt) {
		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;

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

		//set light pos
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//debug toggles
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
	}

	void handlePhysics() {
		for(auto& s:shapes) {
			for(int i=0; i<s.getNum(); i++) {
				s.particles[i].accelerate(gravity);
			}
			s.update(time_step);
		}
	}

	bool user_update(float dt) override {
		handleUserInput(dt);

		//ensure similar update across multiple framerates
		update_timer+=dt;
		while(update_timer>time_step) {
			handlePhysics();

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
		Line l1{v0, v1}; l1.col=col; lines_to_project.push_back(l1);
		Line l2{v1, v3}; l2.col=col; lines_to_project.push_back(l2);
		Line l3{v3, v2}; l3.col=col; lines_to_project.push_back(l3);
		Line l4{v2, v0}; l4.col=col; lines_to_project.push_back(l4);
		//sides
		Line l5{v0, v4}; l5.col=col; lines_to_project.push_back(l5);
		Line l6{v1, v5}; l6.col=col; lines_to_project.push_back(l6);
		Line l7{v2, v6}; l7.col=col; lines_to_project.push_back(l7);
		Line l8{v3, v7}; l8.col=col; lines_to_project.push_back(l8);
		//top
		Line l9{v4, v5}; l9.col=col; lines_to_project.push_back(l9);
		Line l10{v5, v7}; l10.col=col; lines_to_project.push_back(l10);
		Line l11{v7, v6}; l11.col=col; lines_to_project.push_back(l11);
		Line l12{v6, v4}; l12.col=col; lines_to_project.push_back(l12);
	}

	//combine all scene geometry
	bool user_geometry() override {
		for(const auto& s:shapes) {
			for(const auto& it:s.index_tris) {
				Triangle t{
					s.particles[it.a].pos,
					s.particles[it.b].pos,
					s.particles[it.c].pos
				}; t.col=s.col;
				tris_to_project.push_back(t);
			}
		}

		if(show_bounds) {
			for(const auto& s:shapes) {
				addAABB(s.getAABB(), olc::WHITE);
			}
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::BLACK);

		resetBuffers();

		for(const auto& t:tris_to_draw) {
			FillDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].w,
				t.col, t.id
			);
		}

		for(const auto& l:lines_to_draw) {
			DrawDepthLine(
				l.p[0].x, l.p[0].y, l.t[0].w,
				l.p[1].x, l.p[1].y, l.t[1].w,
				l.col, l.id
			);
		}

		return true;
	}
};

int main() {
	Example e;
	bool vsync=true;
	if(e.Construct(540, 360, 1, 1, false, vsync)) e.Start();

	return 0;
}