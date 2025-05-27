#include "common/3d/engine_3d.h"
using olc::vf2d;
using cmn::vf3d;
using cmn::Mat4;

constexpr float Pi=3.1415927f;

#include "phys/constraint.h"
#include "phys/spring.h"

vf3d projectOntoPlane(const vf3d& v, const vf3d& norm) {
	return v-norm.dot(v)*norm;
}

#include "shape.h"

struct Physics3DUI : cmn::Engine3D {
	Physics3DUI() {
		sAppName="3d physics";
	}

	//camera positioning
	float cam_yaw=-Pi/2;
	float cam_pitch=-.1f;

	//debug toggles
	bool show_bounds=false;
	bool update_phys=false;

	//physics stuff
	const vf3d gravity{0, -9.8f, 0};
	std::list<Shape> shapes;

	const float time_step=1/120.f;
	float update_timer=0;

	bool user_create() override {
		cam_pos={0, 0, 3.5f};
		light_pos=cam_pos;

		Shape top_box=Shape::makeBox({vf3d(-1, 2, -1), vf3d(1, 3, 1)});
		top_box.col=olc::RED;
		shapes.push_back(top_box);

		Shape btm_box=Shape::makeBox({vf3d(-2, 0, -2), vf3d(2, 1, 2)});
		btm_box.col=olc::GREEN;
		shapes.push_back(btm_box);

		Shape ground=Shape::makeBox({vf3d(-4, -2, -4), vf3d(4, -1, 4)});
		for(int i=0; i<ground.getNum(); i++) {
			ground.particles[i].locked=true;
		}
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
		if(GetKey(olc::Key::ENTER).bPressed) update_phys^=true;
	}

	void handlePhysics() {
		//collisions(triangle closept)
		for(auto& a:shapes) {
			for(auto& b:shapes) {
				if(&b==&a) continue;

				//accomodate check radius
				cmn::AABB3 a_box=a.getAABB();
				a_box.min-=Particle::rad, a_box.max+=Particle::rad;
				cmn::AABB3 b_box=b.getAABB();
				b_box.min-=Particle::rad, b_box.max+=Particle::rad;
				if(!a_box.overlaps(b_box)) continue;

				//keep a out of b
				for(int i=0; i<a.getNum(); i++) {
					auto& ap=a.particles[i];
					for(const auto& bit:b.index_tris) {
						auto& bp0=b.particles[bit.a];
						auto& bp1=b.particles[bit.b];
						auto& bp2=b.particles[bit.c];
						cmn::Triangle b_t{bp0.pos, bp1.pos, bp2.pos};
						vf3d close_pt=b_t.getClosePt(ap.pos);
						vf3d sub=ap.pos-close_pt;
						float mag2=sub.mag2();
						if(mag2<Particle::rad*Particle::rad) {
							float mag=std::sqrtf(mag2);
							vf3d norm=sub/mag;
							vf3d new_pt=close_pt+Particle::rad*norm;
							vf3d delta=new_pt-ap.pos;
							if(!ap.locked) ap.pos+=.75f*delta;
							//barycentric weights next?
							if(!bp0.locked) bp0.pos-=.25f*delta;
							if(!bp1.locked) bp1.pos-=.25f*delta;
							if(!bp2.locked) bp2.pos-=.25f*delta;
							break;
						}
					}
				}
			}
		}

		for(auto& s:shapes) {
			for(int i=0; i<s.getNum(); i++) {
				s.particles[i].accelerate(gravity);
			}
			s.update(time_step);
		}
	}

	bool user_update(float dt) override {
		handleUserInput(dt);

		if(update_phys) {
			//ensure similar update across multiple framerates
			update_timer+=dt;
			while(update_timer>time_step) {
				handlePhysics();

				update_timer-=time_step;
			}
		}

		return true;
	}

	//combine all scene geometry
	bool user_geometry() override {
		for(const auto& s:shapes) {
			for(const auto& it:s.index_tris) {
				cmn::Triangle t{
					s.particles[it.a].pos,
					s.particles[it.b].pos,
					s.particles[it.c].pos
				}; t.col=s.col;
				tris_to_project.push_back(t);
			}
		}

		if(show_bounds) {
			for(const auto& s:shapes) {
				addAABB(s.getAABB(), olc::GREY);
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

		if(!update_phys) {
			DrawRect(0, 0, ScreenWidth()-1, ScreenHeight()-1, olc::WHITE);
		}

		return true;
	}
};

int main() {
	Physics3DUI p3dui;
	bool vsync=true;
	if(p3dui.Construct(540, 360, 1, 1, false, vsync)) p3dui.Start();

	return 0;
}