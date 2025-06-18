#include "common/3d/engine_3d.h"
namespace olc {
	static const Pixel PURPLE(144, 0, 255);
	static const Pixel ORANGE(255, 115, 0);
	Pixel operator*(float f, const Pixel& col) {
		return col*f;
	}
}
using olc::vf2d;
using cmn::vf3d;
using cmn::Mat4;

#include "common/stopwatch.h"

constexpr float Pi=3.1415927f;

vf3d projectOntoPlane(const vf3d& v, const vf3d& norm) {
	return v-norm.dot(v)*norm;
}

#include "scene.h"

#include "mesh.h"

struct Physics3DUI : cmn::Engine3D {
	Physics3DUI() {
		sAppName="3D Physics";
	}

	//camera positioning
	float cam_yaw=-Pi/2;
	float cam_pitch=-.36f;

	//debug toggles
	bool show_bounds=false;
	bool show_vertexes=false;
	bool show_edges=false;
	bool show_joints=false;
	bool update_phys=false;
	bool to_time=false;

	Shape* highlighted=nullptr;

	cmn::Stopwatch update_watch, geom_watch, pc_watch, render_watch;

	Scene scene;

	Mesh vertex;

	const int num_sub_steps=4;
	const float time_step=1/60.f;
	const float sub_time_step=time_step/num_sub_steps;
	float update_timer=0;

	bool user_create() override {
		srand(time(0));

		cam_pos={0, 5.5f, 8};
		light_pos={0, 12, 0};
		
		{
			scene=Scene();

			Shape softbody1({{-1, .5f, -2}, {1, 1.5f, 2}}, 1, sub_time_step);
			softbody1.fill=olc::RED;
			scene.shapes.push_back(softbody1);

			Shape softbody2({{-2, 2, -1}, {2, 3, 1}}, 1, sub_time_step);
			softbody2.fill=olc::BLUE;
			scene.shapes.push_back(softbody2);

			Shape softbody3({{-1, 3.5f, -2}, {1, 4.5f, 2}}, 1, sub_time_step);
			softbody3.fill=olc::YELLOW;
			scene.shapes.push_back(softbody3);

			Shape softbody4({{-2, 5, -1}, {2, 6, 1}}, 1, sub_time_step);
			softbody4.fill=olc::GREEN;
			scene.shapes.push_back(softbody4);

			Shape ground({{-4, -1, -4}, {4, 0, 4}});
			for(int i=0; i<ground.getNum(); i++) {
				ground.particles[i].locked=true;
			}
			ground.fill=olc::WHITE;
			scene.shapes.push_back(ground);

			scene.shrinkWrap(3);
		}

		try {
			vertex=Mesh::loadFromOBJ("assets/icosahedron.txt");
			vertex.scale={.02f, .02f, .02f};
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";
		ConsoleCaptureStdOut(true);

		return true;
	}

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") {
			ConsoleClear();

			return true;
		}

		if(cmd=="time") {
			to_time=true;

			return true;
		}

		if(cmd=="import") {
			std::string filename;
			line_str>>filename;

			try {
				Scene new_scene=Scene::loadFromFZX(filename);
				scene=new_scene;
				std::cout<<"  success!\n";
				return true;
			} catch(const std::exception& e) {
				std::cout<<"  error: "<<e.what()<<'\n';
				return false;
			}
		}

		if(cmd=="export") {
			std::string filename;
			line_str>>filename;

			try {
				scene.saveToFZX(filename);
				std::cout<<"  success!\n";
				return true;
			} catch(const std::exception& e) {
				std::cout<<"  error: "<<e.what()<<'\n';
				return false;
			}
		}

		if(cmd=="keybinds") {
			std::cout<<
				"  ARROWS   look up, down, left, right\n"
				"  WASD     move forward, back, left, right\n"
				"  SPACE    move up/jump\n"
				"  SHIFT    move down\n"
				"  L        set light pos\n"
				"  ENTER    toggle physics update\n"
				"  B        toggle bounds\n"
				"  V        toggle vertex view\n"
				"  E        toggle edge view\n"
				"  J        toggle joint view\n"
				"  ESC      toggle integrated console\n";

			return true;
		}

		if(cmd=="mousebinds") {
			std::cout<<
				"  LEFT   highlight shape\n";

			return true;
		}


		if(cmd=="help") {
			std::cout<<
				"  clear        clears the console\n"
				"  time         get diagnostics for each stage of game loop\n"
				"  import       imports scene from specified file\n"
				"  export       exports scene to specified file\n"
				"  keybinds     which keys to press for this program?\n"
				"  mousebinds   which buttons to press for this program?\n";

			return true;
		}

		std::cout<<"  unknown command. type help for list of commands.\n";

		return false;
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
			std::cos(cam_yaw)*std::cos(cam_pitch),
			std::sin(cam_pitch),
			std::sin(cam_yaw)*std::cos(cam_pitch)
		);

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

		//set light pos
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//debug toggles
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
		if(GetKey(olc::Key::V).bPressed) show_vertexes^=true;
		if(GetKey(olc::Key::E).bPressed) show_edges^=true;
		if(GetKey(olc::Key::J).bPressed) show_joints^=true;
		if(GetKey(olc::Key::ENTER).bPressed) update_phys^=true;

		//shape highlighting
		if(GetMouse(olc::Mouse::LEFT).bPressed) {
			try {
				//unprojection matrix
				Mat4 invVP=Mat4::inverse(mat_view*mat_proj);

				//get mouse ray
				float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
				float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
				vf3d clip(ndc_x, ndc_y, 1);
				vf3d world=clip*invVP;
				world/=world.w;
				vf3d mouse_dir=(world-cam_pos).norm();

				//find closest shape
				float record=-1;
				highlighted=nullptr;
				for(auto& s:scene.shapes) {
					float dist=s.intersectRay(cam_pos, mouse_dir);
					if(dist>0) {
						if(record<0||dist<record) {
							record=dist;
							highlighted=&s;
						}
					}
				}
			} catch(const std::exception& e) {}
		}
	}

	bool user_update(float dt) override {
		update_watch.start();

		//open and close the integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		//only allow input when console NOT open
		if(!IsConsoleShowing()) handleUserInput(dt);

		if(update_phys) {
			//ensure similar update across multiple framerates
			update_timer+=dt;
			while(update_timer>time_step) {
				for(int i=0; i<num_sub_steps; i++) {
					scene.handleCollisions();
					scene.update(sub_time_step);
				}

				update_timer-=time_step;
			}
		}

		update_watch.stop();

		return true;
	}

	//combine all scene geometry
	bool user_geometry() override {
		geom_watch.start();

		//realize shape geometry
		if(show_edges) {
			for(const auto& s:scene.shapes) {
				for(const auto& ie:s.edges) {
					cmn::Line l{
						s.particles[ie.a].pos,
						s.particles[ie.b].pos
					}; l.col=s.fill, l.id=s.id;
					lines_to_project.push_back(l);
				}
			}
		} else {
			for(const auto& s:scene.shapes) {
				for(const auto& it:s.tris) {
					cmn::Triangle t{
						s.particles[it.a].pos,
						s.particles[it.b].pos,
						s.particles[it.c].pos
					}; t.col=s.fill, t.id=s.id;
					tris_to_project.push_back(t);
				}
			}
		}

		if(show_bounds) {
			for(const auto& s:scene.shapes) {
				addAABB(s.getAABB(), olc::PURPLE);
			}
			addAABB(scene.bounds, olc::ORANGE);
		}

		if(show_joints) {
			for(const auto& j:scene.joints) {
				//for each shape in joint...
				for(int i=0; i<2; i++) {
					//draw lines between joint indexes
					const auto& shp=i?j.shp_a:j.shp_b;
					const auto& ixs=i?j.ix_a:j.ix_b;
					olc::Pixel col=i?j.shp_b->fill:j.shp_a->fill;
					for(const auto& j:ixs) {
						vertex.translation=shp->particles[j].pos;
						vertex.updateTransforms();
						vertex.updateTriangles(col);
						tris_to_project.insert(tris_to_project.end(),
							vertex.tris.begin(), vertex.tris.end()
						);
					}
				}
			}
		}

		geom_watch.stop();

		pc_watch.start();

		return true;
	}

	bool user_render()  override {
		pc_watch.stop();

		render_watch.start();

		//dark grey background
		Clear(olc::Pixel(35, 35, 35));

		//render 3d stuff
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

		//edge detection on highlighted object
		if(highlighted) {
			int id=highlighted->id;
			olc::Pixel col=highlighted->fill;
			for(int i=1; i<ScreenWidth()-1; i++) {
				for(int j=1; j<ScreenHeight()-1; j++) {
					bool curr=id_buffer[i+ScreenWidth()*j]==id;
					bool lft=id_buffer[i-1+ScreenWidth()*j]==id;
					bool rgt=id_buffer[i+1+ScreenWidth()*j]==id;
					bool top=id_buffer[i+ScreenWidth()*(j-1)]==id;
					bool btm=id_buffer[i+ScreenWidth()*(j+1)]==id;
					if(curr!=lft||curr!=rgt||curr!=top||curr!=btm) {
						Draw(i, j, col);
					}
				}
			}
		}

		//crude vertex projection
		auto showVerts=[&] (const Shape& s) {
			//show vertex indexes
			for(int i=0; i<s.getNum(); i++) {
				vf3d ndc=s.particles[i].pos*mat_view*mat_proj;
				ndc/=ndc.w;
				float scr_x=(1-ndc.x)*ScreenWidth()/2;
				float scr_y=(1-ndc.y)*ScreenHeight()/2;
				auto str=std::to_string(i);
				DrawString(scr_x-4, scr_y-4, str, olc::WHITE);
			}

			//show shape index
			vf3d ndc=s.getAABB().getCenter()*mat_view*mat_proj;
			ndc/=ndc.w;
			float scr_x=(1-ndc.x)*ScreenWidth()/2;
			float scr_y=(1-ndc.y)*ScreenHeight()/2;
			auto str=std::to_string(s.id);
			DrawString(scr_x-4, scr_y-4, str, olc::CYAN);
		};

		if(show_vertexes) {
			if(highlighted) showVerts(*highlighted);
			else for(const auto& s:scene.shapes) {
				showVerts(s);
			}
		}

		//show red border if physics update is off
		if(!update_phys) {
			DrawRect(0, 0, ScreenWidth()-1, ScreenHeight()-1, olc::RED);
		}

		render_watch.stop();

		//print diagnostics
		if(to_time) {
			auto update_dur=update_watch.getMicros();
			auto geom_dur=geom_watch.getMicros();
			auto pc_dur=pc_watch.getMicros();
			auto render_dur=render_watch.getMicros();
			std::cout<<
				"  update: "<<update_dur<<"us ("<<(update_dur/1000.f)<<"ms)\n"<<
				"  geom: "<<geom_dur<<"us ("<<(geom_dur/1000.f)<<"ms)\n"<<
				"  project & clip: "<<pc_dur<<"us ("<<(pc_dur/1000.f)<<"ms)\n"<<
				"  render: "<<render_dur<<"us ("<<(render_dur/1000.f)<<"ms)\n";

			//reset print flag
			to_time=false;
		}

		return true;
	}
};

int main() {
	Physics3DUI p3dui;
	bool vsync=true;
	if(p3dui.Construct(540, 360, 1, 1, false, vsync)) p3dui.Start();

	return 200;
}