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

struct Joint {
	Shape* shp_a=nullptr;
	std::list<int> ix_a;
	Shape* shp_b=nullptr;
	std::list<int> ix_b;

	void update() {
		//find side centers
		vf3d avg_a;
		for(const auto& ia:ix_a) avg_a+=shp_a->particles[ia].pos;
		avg_a/=ix_a.size();
		vf3d avg_b;
		for(const auto& ib:ix_b) avg_b+=shp_b->particles[ib].pos;
		avg_b/=ix_b.size();
		
		//update accordingly
		vf3d axis=avg_b-avg_a;
		vf3d delta=.5f*axis;
		for(const auto& ia:ix_a) {
			auto& a=shp_a->particles[ia];
			if(!a.locked) a.pos+=delta;
		}
		for(const auto& ib:ix_b) {
			auto& b=shp_b->particles[ib];
			if(!b.locked) b.pos-=delta;
		}
	}
};

struct Physics3DUI : cmn::Engine3D {
	Physics3DUI() {
		sAppName="3d physics";
	}

	//camera positioning
	float cam_yaw=-Pi/2;
	float cam_pitch=-.3f;

	//debug toggles
	bool show_bounds=false;
	bool show_verts=false;
	bool update_phys=false;

	//physics stuff
	const vf3d gravity{0, -9.8f, 0};
	std::list<Shape> shapes;
	std::list<Joint> joints;

	const float time_step=1/120.f;
	float update_timer=0;

	bool user_create() override {
		cam_pos={.5f, 0, 8};
		light_pos={0, 5, 0};

		Shape chain0=Shape::makeBox({vf3d(0, -.5f, -.5f), vf3d(2, .5f, .5f)});
		chain0.particles[2].locked=true;
		chain0.col=olc::RED;
		shapes.push_back(chain0);
		auto chain0_ptr=&shapes.back();

		Shape chain1=Shape::makeBox({vf3d(2, -.5f, -.5f), vf3d(4, .5f, .5f)});
		chain1.col=olc::YELLOW;
		shapes.push_back(chain1);
		auto chain1_ptr=&shapes.back();

		Shape chain2=Shape::makeBox({vf3d(4, -.5f, -.5f), vf3d(6, .5f, .5f)});
		chain2.col=olc::BLUE;
		shapes.push_back(chain2);
		auto chain2_ptr=&shapes.back();
		
		joints.push_back({
			chain0_ptr,
			{1, 3, 5, 7},
			chain1_ptr,
			{0, 2, 4, 6}
			});

		joints.push_back({
			chain1_ptr,
			{1, 3, 5, 7},
			chain2_ptr,
			{0, 2, 4, 6}
			});

		Shape stack0=Shape::makeBox({vf3d(-4, 0, -1), vf3d(-2, 1, 1)});
		stack0.col=olc::WHITE;
		shapes.push_back(stack0);

		Shape stack1=Shape::makeBox({vf3d(-5, -2, -2), vf3d(-1, -1, 2)});
		stack1.col=olc::MAGENTA;
		shapes.push_back(stack1);

		Shape ground=Shape::makeBox({vf3d(-6, -4, -4), vf3d(7, -3, 4)});
		ground.col=olc::GREEN;
		for(int i=0; i<ground.getNum(); i++) ground.particles[i].locked=true;
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
		if(GetKey(olc::Key::V).bPressed) show_verts^=true;
		if(GetKey(olc::Key::ENTER).bPressed) update_phys^=true;
	}

	void handlePhysics() {
		//collisions
		for(auto& a:shapes) {
			for(auto& b:shapes) {
				//dont check self
				if(&b==&a) continue;

				//find jointed indexes
				const std::list<int>* skip_ix=nullptr;
				for(const auto& j:joints) {
					if(j.shp_a==&a&&j.shp_b==&b) {
						skip_ix=&j.ix_a;
						break;
					}
					if(j.shp_a==&b&&j.shp_b==&a) {
						skip_ix=&j.ix_b;
						break;
					}
				}

				//bounding box optimization
				cmn::AABB3 a_box=a.getAABB();
				a_box.min-=Particle::rad, a_box.max+=Particle::rad;
				cmn::AABB3 b_box=b.getAABB();
				b_box.min-=Particle::rad, b_box.max+=Particle::rad;
				if(!a_box.overlaps(b_box)) continue;

				//keep a pts out of b tris
				for(int i=0; i<a.getNum(); i++) {
					//skip jointed indexes
					if(skip_ix) {
						bool to_skip=false;
						for(const auto& j:*skip_ix) {
							if(j==i) {
								to_skip=true;
								break;
							}
						}
						if(to_skip) continue;
					}
					auto& ap=a.particles[i];
					for(const auto& sit:b.index_tris) {
						auto& bp0=b.particles[sit.a];
						auto& bp1=b.particles[sit.b];
						auto& bp2=b.particles[sit.c];
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
		
		//update joints
		for(auto& j:joints) {
			j.update();
		}

		//integration
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

		if(GetKey(olc::Key::Z).bPressed) {
			std::cout<<cam_pos.x<<' '<<cam_pos.y<<' '<<cam_pos.z<<'\n';
			std::cout<<cam_pitch<<' '<<cam_yaw<<'\n';
		}

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

		if(show_verts) {
			for(const auto& s:shapes) {
				for(int i=0; i<s.getNum(); i++) {
					vf3d ndc=s.particles[i].pos*mat_view*mat_proj;
					ndc/=ndc.w;
					float scr_x=(1-ndc.x)*ScreenWidth()/2;
					float scr_y=(1-ndc.y)*ScreenHeight()/2;
					auto str=std::to_string(i);
					DrawString(scr_x-4, scr_y-4, str);
				}
			}
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