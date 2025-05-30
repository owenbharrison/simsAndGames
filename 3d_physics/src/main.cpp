#include "common/3d/engine_3d.h"
namespace olc {
	static const Pixel PURPLE(144, 0, 255);
	static const Pixel ORANGE(255, 115, 0);
}
using olc::vf2d;
using cmn::vf3d;
using cmn::Mat4;

constexpr float Pi=3.1415927f;

#include "phys/constraint.h"
//#include "phys/spring.h"

vf3d projectOntoPlane(const vf3d& v, const vf3d& norm) {
	return v-norm.dot(v)*norm;
}

#include "shape.h"

#include "common/stopwatch.h"

//basically a weighted constraint
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

		//update each side halfway
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
	float cam_pitch=-.36f;

	//debug toggles
	bool show_bounds=false;
	bool show_verts=false;
	bool show_edges=false;
	bool update_phys=false;

	bool to_time=false;
	cmn::Stopwatch update_watch, geom_watch, pc_watch, render_watch;

	//physics stuff
	const vf3d gravity{0, -9.8f, 0};
	std::list<Shape> shapes;
	std::list<Joint> joints;

	const float time_step=1/120.f;
	float update_timer=0;

	bool user_create() override {
		srand(time(0));
		
		cam_pos={0, 5.5f, 8};
		light_pos={0, 12, 0};

		Shape ground({vf3d(-7, -1, -3), vf3d(7, 0, 4)});
		for(int i=0; i<ground.getNum(); i++) {
			ground.particles[i].locked=true;
		}
		ground.col=olc::WHITE;
		shapes.push_back(ground);

		Shape stack0({vf3d(-6, 1, -2), vf3d(-1, 2, 3)});
		stack0.col=olc::PURPLE;
		shapes.push_back(stack0);

		Shape stack1({vf3d(-5, 3, -1), vf3d(-2, 4, 2)});
		stack1.col=olc::GREEN;
		shapes.push_back(stack1);

		Shape stack2({vf3d(-4, 5, 0), vf3d(-3, 6, 1)});
		stack2.col=olc::ORANGE;
		shapes.push_back(stack2);

		Shape chain0({vf3d(0, 6, 0), vf3d(2, 7, 1)});
		chain0.particles[2].locked=true;
		chain0.particles[6].locked=true;
		chain0.col=olc::RED;
		shapes.push_back(chain0);
		auto chain0_ptr=&shapes.back();

		Shape chain1({vf3d(2, 6, 0), vf3d(4, 7, 1)});
		chain1.col=olc::YELLOW;
		shapes.push_back(chain1);
		auto chain1_ptr=&shapes.back();

		Shape chain2({vf3d(4, 6, -1), vf3d(6, 7, 2)});
		chain2.col=olc::BLUE;
		shapes.push_back(chain2);
		auto chain2_ptr=&shapes.back();

		joints.push_back({
			chain0_ptr,
			{1, 3, 5, 7},
			chain1_ptr,
			{0, 2, 4, 6},
			});

		joints.push_back({
			chain1_ptr,
			{1, 3, 5, 7},
			chain2_ptr,
			{0, 2, 4, 6},
			});

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
		if(GetKey(olc::Key::E).bPressed) show_edges^=true;
		if(GetKey(olc::Key::ENTER).bPressed) update_phys^=true;
	}

	void handlePhysics(float dt) {
		//find potential matches
		std::list<std::pair<Shape*, Shape*>> checks;
		for(auto ait=shapes.begin(); ait!=shapes.end(); ait++) {
			for(auto bit=std::next(ait); bit!=shapes.end(); bit++) {
				//predicated by bounding box
				cmn::AABB3 a_box=ait->getAABB();
				a_box.min-=Particle::rad, a_box.max+=Particle::rad;
				cmn::AABB3 b_box=bit->getAABB();
				b_box.min-=Particle::rad, b_box.max+=Particle::rad;
				if(a_box.overlaps(b_box)) checks.push_back({&*ait, &*bit});
			}
		}

		//corner-surface collisions
		for(const auto& c:checks) {
			const Shape* shapes[2]{c.first, c.second};
			for(int i=0; i<2; i++) {
				auto& a=*shapes[i], & b=*shapes[1-i];

				//find joint conflicts
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

				//keep a pts out of b tris
				for(int i=0; i<a.getNum(); i++) {
					//skip jointed particles
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

					//check particle against each tri
					auto& ap=a.particles[i];
					for(const auto& sit:b.tris) {
						//realize triangle
						auto& bp0=b.particles[sit.a];
						auto& bp1=b.particles[sit.b];
						auto& bp2=b.particles[sit.c];
						cmn::Triangle b_t{bp0.pos, bp1.pos, bp2.pos};
						vf3d close_pt=b_t.getClosePt(ap.pos);
						vf3d sub=ap.pos-close_pt;
						float mag2=sub.mag2();
						//is it too close to the triangle?
						if(mag2<Particle::rad*Particle::rad) {
							//calculate resolution
							float mag=std::sqrtf(mag2);
							vf3d norm=sub/mag;
							vf3d new_pt=close_pt+Particle::rad*norm;
							vf3d delta=new_pt-ap.pos;
							//find contributions
							float m1a=1/ap.mass;
							float m1b0=1/bp0.mass;
							float m1b1=1/bp1.mass;
							float m1b2=1/bp2.mass;
							float m1t=m1a+m1b0+m1b1+m1b2;
							//push apart
							if(!ap.locked) ap.pos+=m1a/m1t*delta;
							//barycentric weights next?
							if(!bp0.locked) bp0.pos-=m1b0/m1t*delta;
							if(!bp1.locked) bp1.pos-=m1b1/m1t*delta;
							if(!bp2.locked) bp2.pos-=m1b2/m1t*delta;
							break;
						}
					}
				}
			}
		}

		//edge-edge collisions
		for(const auto& c:checks) {
			//find joint conflicts
			const std::list<int>* skip_ix_a=nullptr, * skip_ix_b=nullptr;
			for(const auto& j:joints) {
				if(j.shp_a==c.first&&j.shp_b==c.second) {
					skip_ix_a=&j.ix_a, skip_ix_b=&j.ix_b;
					break;
				}
				if(j.shp_a==c.second&&j.shp_b==c.first) {
					skip_ix_a=&j.ix_b, skip_ix_b=&j.ix_a;
					break;
				}
			}

			for(const auto& ea:c.first->edges) {
				//skip jointed edges
				if(skip_ix_a) {
					bool to_skip=false;
					for(const auto& i:*skip_ix_a) {
						if(i==ea.a||i==ea.b) {
							to_skip=true;
							break;
						}
					}
					if(to_skip) break;
				}

				auto& a1=c.first->particles[ea.a];
				auto& a2=c.first->particles[ea.b];
				for(const auto& eb:c.second->edges) {
					//skip jointed edges
					if(skip_ix_a) {
						bool to_skip=false;
						for(const auto& i:*skip_ix_a) {
							if(i==ea.a||i==ea.b) {
								to_skip=true;
								break;
							}
						}
						if(to_skip) break;
					}
					
					auto& b1=c.second->particles[eb.a];
					auto& b2=c.second->particles[eb.b];
					//find close points on both edges
					vf3d u=a2.pos-a1.pos;
					vf3d v=b2.pos-b1.pos;
					vf3d w=a1.pos-b1.pos;
					float a=u.dot(u);
					float b=u.dot(v);
					float c=v.dot(v);
					float d=u.dot(w);
					float e=v.dot(w);
					float D=a*c-b*b;
					float s, t;
					if(std::fabsf(D)<1e-6f) s=0, t=b>c?d/b:e/c;
					else {
						s=std::max(0.f, std::min(1.f, (b*e-c*d)/D));
						t=std::max(0.f, std::min(1.f, (a*e-b*d)/D));
					};
					vf3d close_a=a1.pos+s*u;
					vf3d close_b=b1.pos+t*v;
					vf3d sub=close_a-close_b;
					float mag2=sub.mag2();
					//are they too close to eachother?
					if(mag2<Particle::rad*Particle::rad) {
						//calculate resolution
						float mag=std::sqrtf(mag2);
						vf3d norm=sub/mag;
						float delta=Particle::rad-mag;
						vf3d correct=delta*norm;
						//find contributions
						float m1a1=1/a1.mass;
						float m1a2=1/a2.mass;
						float m1b1=1/b1.mass;
						float m1b2=1/b2.mass;
						float m1t=m1a1+m1a2+m1b1+m1b2;
						//push apart
						if(!a1.locked) a1.pos+=(1-s)*m1a1/m1t*correct;
						if(!a2.locked) a2.pos+=s*m1a2/m1t*correct;
						if(!b1.locked) b1.pos-=(1-t)*m1b1/m1t*correct;
						if(!b2.locked) b2.pos-=t*m1b2/m1t*correct;
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
			s.update(dt);
		}
	}

	bool user_update(float dt) override {
		//exterior input logic
		if(GetKey(olc::Key::T).bPressed) {
			to_time=true;
			std::cout<<"timing info:\n";
		}
		
		if(to_time) update_watch.start();

		handleUserInput(dt);

		if(update_phys) {
			//ensure similar update across multiple framerates
			update_timer+=dt;
			while(update_timer>time_step) {
				const int num_steps=12;
				const float sub_time_step=time_step/num_steps;
				for(int i=0; i<num_steps; i++) {
					handlePhysics(sub_time_step);
				}

				update_timer-=time_step;
			}
		}

		if(to_time) {
			update_watch.stop();
			auto dur=update_watch.getMicros();
			std::cout<<"  update: "<<dur<<"us ("<<(dur/1000.f)<<"ms)\n";
		}

		return true;
	}

	//combine all scene geometry
	bool user_geometry() override {
		if(to_time) geom_watch.start();

		for(const auto& s:shapes) {
			if(show_edges) {
				for(const auto& ie:s.edges) {
					cmn::Line l{
						s.particles[ie.a].pos,
						s.particles[ie.b].pos
					}; l.col=s.col;
					lines_to_project.push_back(l);
				}
			} else {
				for(const auto& it:s.tris) {
					cmn::Triangle t{
						s.particles[it.a].pos,
						s.particles[it.b].pos,
						s.particles[it.c].pos
					}; t.col=s.col;
					tris_to_project.push_back(t);
				}
			}
		}

		if(show_bounds) {
			for(const auto& s:shapes) {
				addAABB(s.getAABB(), olc::BLACK);
			}
		}

		if(to_time) {
			geom_watch.stop();
			auto dur=geom_watch.getMicros();
			std::cout<<"  geom: "<<dur<<"us ("<<(dur/1000.f)<<"ms)\n";
		}

		if(to_time) pc_watch.start();

		return true;
	}

	bool user_render() override {
		if(to_time) {
			pc_watch.stop();
			auto dur=pc_watch.getMicros();
			std::cout<<"  project & clip: "<<dur<<"us ("<<(dur/1000.f)<<"ms)\n";
		}
		
		if(to_time) render_watch.start();

		Clear(olc::Pixel(35, 35, 35));

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

		if(show_verts) {
			for(const auto& s:shapes) {
				for(int i=0; i<s.getNum(); i++) {
					vf3d ndc=s.particles[i].pos*mat_view*mat_proj;
					ndc/=ndc.w;
					float scr_x=(1-ndc.x)*ScreenWidth()/2;
					float scr_y=(1-ndc.y)*ScreenHeight()/2;
					auto str=std::to_string(i);
					DrawString(scr_x-4, scr_y-4, str, olc::BLACK);
				}
			}
		}

		if(!update_phys) {
			DrawRect(0, 0, ScreenWidth()-1, ScreenHeight()-1, olc::WHITE);
		}

		if(to_time) {
			render_watch.stop();
			auto dur=render_watch.getMicros();
			std::cout<<"  render: "<<dur<<"us ("<<(dur/1000.f)<<"ms)\n";

			to_time=false;
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