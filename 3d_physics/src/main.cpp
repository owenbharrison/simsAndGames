#define OLC_GFX_OPENGL33
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

#define OLC_PGEX_SHADERS
#include "olcPGEX_Shaders.h"

#include "shader.h"

struct Physics3DUI : cmn::Engine3D {
	Physics3DUI() {
		sAppName="3D Physics";
	}

	//camera positioning
	float cam_yaw=-Pi/2;
	float cam_pitch=-.36f;
	vf3d light_pos;

	//ui stuff
	vf3d mouse_dir;

	Shape* grab_shape=nullptr;
	vf3d grab_ctr, grab_dir;
	Particle mouse_particle;
	std::list<Spring> mouse_springs;

	vf2d menu_pos;
	const float menu_sz=105;
	const float menu_min_sz=.2f*menu_sz;
	const float menu_logo_sz=.4f*menu_sz;

	//physics stuff
	Scene scene;

	const int num_sub_steps=4;
	const float time_step=1/60.f;
	const float sub_time_step=time_step/num_sub_steps;
	float update_timer=0;

	bool update_phys=false;

	//geometry stuf
	Mesh vertex;

	bool show_bounds=false;
	bool show_vertexes=false;
	bool show_edges=false;
	bool show_joints=false;

	//render stuff
	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;
	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;

	//post processing stuff
	olc::Sprite* source_spr=nullptr;
	olc::Decal* source_dec=nullptr;
	olc::Sprite* target_spr=nullptr;
	olc::Decal* target_dec=nullptr;
	unsigned int frame_count=0;

	//shader stuff
	olc::Shade shade;
	std::list<Shader> shaders;
	olc::Effect* identity_pass=nullptr;
	Shader* chosen_shader=nullptr;

	//diagnostic stuff
	bool to_time=false;
	cmn::Stopwatch update_watch, geom_watch, pc_watch, render_watch, pp_watch;

	bool user_create() override {
		srand(time(0));

		cam_pos={0, 5.5f, 8};
		light_pos={0, 12, 0};

		//ensure mouse particle has no influencers
		mouse_particle.locked=true;

		//initialize softbody scene
		{
			scene=Scene();

			Shape softbody1({{-1, .5f, -2}, {1, 1.5f, 2}}, 1, sub_time_step);
			softbody1.fill=olc::RED;
			scene.addShape(softbody1);

			Shape softbody2({{-2, 2, -1}, {2, 3, 1}}, 1, sub_time_step);
			softbody2.fill=olc::BLUE;
			scene.addShape(softbody2);

			Shape softbody3({{-1, 3.5f, -2}, {1, 4.5f, 2}}, 1, sub_time_step);
			softbody3.fill=olc::YELLOW;
			scene.addShape(softbody3);

			Shape softbody4({{-2, 5, -1}, {2, 6, 1}}, 1, sub_time_step);
			softbody4.fill=olc::GREEN;
			scene.addShape(softbody4);

			Shape ground({{-4, -1, -4}, {4, 0, 4}});
			for(int i=0; i<ground.getNum(); i++) {
				ground.particles[i].locked=true;
			}
			ground.fill=olc::WHITE;
			scene.addShape(ground);

			scene.shrinkWrap(3);
		}

		//load vertex model for joint view
		try {
			vertex=Mesh::loadFromOBJ("assets/models/icosahedron.txt");
			vertex.scale={.03f, .03f, .03f};
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		//make some "primitives" to help draw with
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

		//allocate source and target buffers
		source_spr=new olc::Sprite(ScreenWidth(), ScreenHeight());
		source_dec=new olc::Decal(source_spr);
		target_spr=new olc::Sprite(ScreenWidth(), ScreenHeight());
		target_dec=new olc::Decal(target_spr);

		//load shader options
		try {
			//default to identity shader
			shaders.push_back(Shader(shade, "assets/logos/identity.png", {"assets/fx/identity.glsl"}));
			identity_pass=&shaders.back().passes.front();
			chosen_shader=&shaders.back();
			
			shaders.push_back(Shader(shade, "assets/logos/rgb.png", {"assets/fx/rgb_ht.glsl"}));
			shaders.push_back(Shader(shade, "assets/logos/ascii.png", {"assets/fx/ascii.glsl"}));
			shaders.push_back(Shader(shade, "assets/logos/cmyk.png", {"assets/fx/cmyk_ht.glsl"}));
			shaders.push_back(Shader(shade, "assets/logos/crt.png", {"assets/fx/crt.glsl"}));
			shaders.push_back(Shader(shade, "assets/logos/sobel.png", {"assets/fx/rainbow_sobel.glsl"}));
			shaders.push_back(Shader(shade, "assets/logos/crosshatch.png", {
				"assets/fx/crosshatch.glsl",
				"assets/fx/gaussian_blur.glsl"
				}));
			shaders.push_back(Shader(shade, "assets/logos/voronoi.png", {"assets/fx/voronoi.glsl"}));
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		//print help disclaimer
		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";
		ConsoleCaptureStdOut(true);

		return true;
	}

	bool user_destroy() override {
		//deallocate helpers
		delete prim_rect_spr;
		delete prim_rect_dec;
		delete prim_circ_spr;
		delete prim_circ_dec;

		//deallocate buffers
		delete source_dec;
		delete source_spr;
		delete target_dec;
		delete target_spr;

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
				"  LEFT    grab and drag shape\n"<<
				"  RIGHT   shader selection menu\n";

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
		//cant move if grabbing shape
		if(!grab_shape) {
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

		//cant move if grabbing shape
		if(!grab_shape) {
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
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//debug toggles
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
		if(GetKey(olc::Key::V).bPressed) show_vertexes^=true;
		if(GetKey(olc::Key::E).bPressed) show_edges^=true;
		if(GetKey(olc::Key::J).bPressed) show_joints^=true;
		if(GetKey(olc::Key::ENTER).bPressed) update_phys^=true;

		//update mouse ray (matrix could be singular)
		try {
			//unprojection matrix
			Mat4 invVP=Mat4::inverse(mat_view*mat_proj);

			//get ray thru screen mouse pos
			float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
			float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
			vf3d clip(ndc_x, ndc_y, 1);
			vf3d world=clip*invVP;
			world/=world.w;
			mouse_dir=(world-cam_pos).norm();
		} catch(const std::exception& e) {}

		//shape grabbing
		const auto grab_action=GetMouse(olc::Mouse::LEFT);
		if(grab_action.bPressed) {
			grab_shape=nullptr;

			//find closest shape
			float record=-1;
			for(auto& s:scene.shapes) {
				float dist=s.intersectRay(cam_pos, mouse_dir);
				if(dist>0) {
					if(record<0||dist<record) {
						record=dist;
						grab_shape=&s;
					}
				}
			}

			if(grab_shape) {
				//store translation plane info
				grab_ctr=cam_pos+record*mouse_dir;
				grab_dir=mouse_dir;

				//temp update for spring setup
				mouse_particle.pos=grab_ctr;

				//setup mouse springs for every particle
				for(int i=0; i<grab_shape->getNum(); i++) {
					mouse_springs.emplace_back(&mouse_particle, &grab_shape->particles[i], sub_time_step);
				}
			}
		}
		if(grab_action.bReleased) {
			grab_shape=nullptr;
			mouse_springs.clear();
		}

		//update mouse particle position
		if(grab_shape) {
			vf3d ix=cmn::segIntersectPlane(
				cam_pos,
				cam_pos+mouse_dir,
				grab_ctr,
				grab_dir
			);
			mouse_particle.pos=ix;
			mouse_particle.old_pos=ix;
		}

		//menu positioning
		if(GetMouse(olc::Mouse::RIGHT).bPressed) menu_pos=GetMousePos();

		//menu selection
		if(GetMouse(olc::Mouse::RIGHT).bReleased) {
			//get relative offset
			vf2d sub=menu_pos-GetMousePos();
			//ensure within selection radius
			float dist=sub.mag();
			if(dist>menu_min_sz&&dist<menu_sz) {
				//find angle
				float angle=std::atan2(sub.y, sub.x);
				//normalize
				float a01=(1.5f*Pi+angle)/2/Pi;
				//truncate to nearest section
				int reg=int(std::round(shaders.size()*a01))%shaders.size();
				//choose shader with iterators
				auto it=shaders.begin();
				std::advance(it, reg);
				chosen_shader=&*it;
			}
		}
	}

	bool user_update(float dt) override {
		if(to_time) update_watch.start();

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
					//update mouse springs
					for(auto& m:mouse_springs) {
						m.update(sub_time_step);
					}
					scene.update(sub_time_step);
				}

				update_timer-=time_step;
			}
		}

		if(to_time) update_watch.stop();

		return true;
	}

	//combine all scene geometry
	bool user_geometry() override {
		if(to_time) geom_watch.start();

		//add main light
		lights.push_back({light_pos, olc::WHITE});

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

		//add all shapes bounds and scene bounds
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

		//show axis at mouse particle
		if(grab_shape) {
			const float sz=.3f;
			cmn::Line l6{mouse_particle.pos-vf3d(sz, 0, 0), mouse_particle.pos+vf3d(sz, 0, 0)}; l6.col=olc::RED;
			lines_to_project.push_back(l6);
			cmn::Line l7{mouse_particle.pos-vf3d(0, sz, 0), mouse_particle.pos+vf3d(0, sz, 0)}; l7.col=olc::BLUE;
			lines_to_project.push_back(l7);
			cmn::Line l8{mouse_particle.pos-vf3d(0, 0, sz), mouse_particle.pos+vf3d(0, 0, sz)}; l8.col=olc::GREEN;
			lines_to_project.push_back(l8);
		}

		if(to_time)  geom_watch.stop();

		if(to_time)  pc_watch.start();

		return true;
	}

#pragma region RENDER HELPERS
	void DrawThickLineDecal(const vf2d& a, const vf2d& b, float w, olc::Pixel col) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=(sub/len).perp();

		float angle=std::atan2f(sub.y, sub.x);
		DrawRotatedDecal(a-w*tang, prim_rect_dec, angle, {0, 0}, {len, 2*w}, col);
	}

	void FillCircleDecal(const vf2d& pos, float rad, olc::Pixel col) {
		vf2d offset(rad, rad);
		vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	void DrawThickCircleDecal(const vf2d& pos, float rad, float w, const olc::Pixel& col) {
		const int num=32;
		vf2d first, prev;
		for(int i=0; i<num; i++) {
			float angle=2*Pi*i/num;
			vf2d curr(pos.x+rad*std::cos(angle), pos.y+rad*std::sin(angle));
			FillCircleDecal(curr, w, col);
			if(i==0) first=curr;
			else DrawThickLineDecal(prev, curr, w, col);
			prev=curr;
		}
		DrawThickLineDecal(first, prev, w, col);
	}
#pragma endregion

	bool user_render()  override {
		if(to_time) pc_watch.stop();

		if(to_time) render_watch.start();

		//render to source sprite
		SetDrawTarget(source_spr);

		//dark grey background
		Clear(olc::Pixel(60, 60, 60));

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

		//edge detection on grabbed shape
		if(grab_shape) {
			int id=grab_shape->id;
			for(int i=1; i<ScreenWidth()-1; i++) {
				for(int j=1; j<ScreenHeight()-1; j++) {
					bool curr=id_buffer[bufferIX(i, j)]==id;
					bool lft=id_buffer[bufferIX(i-1, j)]==id;
					bool rgt=id_buffer[bufferIX(i+1, j)]==id;
					bool top=id_buffer[bufferIX(i, j-1)]==id;
					bool btm=id_buffer[bufferIX(i, j+1)]==id;
					if(curr!=lft||curr!=rgt||curr!=top||curr!=btm) {
						Draw(i, j, grab_shape->fill);
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
			//show only verts of selected, else all
			if(grab_shape) showVerts(*grab_shape);
			else for(const auto& s:scene.shapes) {
				showVerts(s);
			}
		}

		//show red border if physics update is off
		if(!update_phys) {
			DrawRect(0, 0, ScreenWidth()-1, ScreenHeight()-1, olc::RED);
		}

		if(to_time) render_watch.stop();

		//post processing
		if(to_time) pp_watch.start();

		//update frame info
		Draw(0, 0, {
			0xFF&frame_count,
			0xFF&(frame_count>>8),
			0xFF&(frame_count>>16),
			0xFF&(frame_count>>24)
			});
		frame_count++;

		//update source decal with sprite
		source_dec->Update();

		//apply each shader pass
		for(auto& p:chosen_shader->passes) {
			shade.SetSourceDecal(source_dec);
			shade.SetTargetDecal(target_dec);
			shade.Start(&p);
			shade.DrawQuad({-1, -1}, {2, 2});
			shade.End();

			//put target in source
			shade.SetSourceDecal(target_dec);
			shade.SetTargetDecal(source_dec);
			shade.Start(identity_pass);
			shade.DrawQuad({-1, -1}, {2, 2});
			shade.End();
		}

		//draw effect
		DrawDecal({0, 0}, target_dec);

		//show shader menu
		if(GetMouse(olc::Mouse::RIGHT).bHeld) {
			//for each shader option
			int i=0;
			for(const auto& s:shaders) {
				//draw logo
				float logo_angle=Pi*(-.5f+2.f*i/shaders.size());
				vf2d logo_dir(std::cos(logo_angle), std::sin(logo_angle));
				vf2d ctr=menu_pos+.67f*menu_sz*logo_dir;
				vf2d size(s.logo_spr->width, s.logo_spr->height);
				float scl=menu_logo_sz/size.x;
				DrawDecal(ctr-scl/2*size, s.logo_dec, {scl, scl});
					
				//draw divider
				float div_angle=logo_angle+Pi/shaders.size();
				vf2d div_dir(std::cos(div_angle), std::sin(div_angle));
				DrawThickLineDecal(menu_pos+menu_min_sz*div_dir, menu_pos+menu_sz*div_dir, 1, olc::WHITE);

				i++;
			}

			//draw circle bounds
			DrawThickCircleDecal(menu_pos, menu_min_sz, 2, olc::WHITE);
			DrawThickCircleDecal(menu_pos, menu_sz, 2, olc::WHITE);
		}

		if(to_time) pp_watch.stop();

		//print diagnostics
		if(to_time) {
			auto update_dur=update_watch.getMicros();
			auto geom_dur=geom_watch.getMicros();
			auto pc_dur=pc_watch.getMicros();
			auto render_dur=render_watch.getMicros();
			auto pp_dur=pp_watch.getMicros();
			std::cout<<
				"  update: "<<update_dur<<"us ("<<(update_dur/1000.f)<<"ms)\n"<<
				"  geom: "<<geom_dur<<"us ("<<(geom_dur/1000.f)<<"ms)\n"<<
				"  project & clip: "<<pc_dur<<"us ("<<(pc_dur/1000.f)<<"ms)\n"<<
				"  render: "<<render_dur<<"us ("<<(render_dur/1000.f)<<"ms)\n"<<
				"  post process: "<<pp_dur<<"us ("<<(pp_dur/1000.f)<<"ms)\n";

			//reset print flag
			to_time=false;
		}

		return true;
	}
};

int main() {
	Physics3DUI p3dui;
	bool vsync=true;
	if(p3dui.Construct(720, 480, 1, 1, false, vsync)) p3dui.Start();

	return 0;
}