#include "common/3d/engine_3d.h"
using olc::vf2d;
namespace olc {
	static const Pixel PURPLE(155, 0, 255);
}
using cmn::vf3d;
using cmn::Mat4;

#include "mesh.h"

#include "common/stopwatch.h"
#include "common/utils.h"

#include "particle.h"

vf3d reflect(const vf3d& in, const vf3d& norm) {
	return in-2*norm.dot(in)*norm;
}

struct VoxelGame : cmn::Engine3D {
	VoxelGame() {
		sAppName="Voxel Game";
	}

	//camera positioning
	float cam_yaw=cmn::Pi/2;
	float cam_pitch=-.3f;

	//scene stuff
	Mesh model;
	const cmn::AABB3 scene_bounds{{-5, -5, -5}, {5, 5, 5}};

	//render stuff
	vf3d render_pos, render_dir;
	olc::Sprite* render_tex=nullptr;

	//toggles
	bool show_outlines=false;
	bool show_render=false;
	bool show_edges=false;
	bool show_grid=false;
	bool show_depth=false;
	bool show_normals=false;

	//diagnostic flag
	bool to_time=false;
	cmn::Stopwatch update_watch, geom_watch, pc_watch, render_watch;

	//experimental
	std::list<Particle> particles;
	vf3d gravity{0, -1, 0};

	bool player_camera=false;
	bool third_person=false;
	vf3d player_pos;
	vf3d player_vel;
	const float player_rad=.1f;
	const float player_height=.25f;
	bool player_on_ground=false;
	float player_airtime=0;

	bool user_create() override {
		srand(time(0));

		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";
		ConsoleCaptureStdOut(true);

		cam_pos={0, 2, -9};
		light_pos=cam_pos;

		//try load model
		try {
			model=Mesh::loadFromOBJ("assets/models/mountains.txt");
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		//rescale
		model.fitToBounds(scene_bounds);
		model.colorNormals();

		//setup render texture
		render_tex=new olc::Sprite(128, 96);

		return true;
	}

	bool user_destroy() override {
		delete render_tex;
		
		return true;
	}

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") {
			ConsoleClear();

			return true;
		}

		if(cmd=="reset") {
			particles.clear();
			std::cout<<"  removed particles\n";

			return true;
		}

		if(cmd=="time") {
			to_time=true;

			return true;
		}

		if(cmd=="import") {
			std::string filename;
			line_str>>filename;
			if(filename.empty()) {
				std::cout<<"no filename. try using:\n  import <filename>\n";

				return false;
			}

			//try load model
			Mesh m;
			try {
				m=Mesh::loadFromOBJ(filename);
			} catch(const std::exception& e) {
				std::cout<<"  "<<e.what()<<'\n';

				return false;
			}
			model=m;

			//rescale
			model.fitToBounds(scene_bounds);
			model.colorNormals();

			std::cout<<"  successfully loaded model w/ "<<model.triangles.size()<<"tris\n";

			return true;
		}

		if(cmd=="render") {
			cmn::Stopwatch watch;
			watch.start();

			//update positioning
			render_pos=cam_pos;
			render_dir=cam_dir;
			
			//turn off render view for render
			bool old_show_render=show_render;
			show_render=false;

			//rerender scene
			olc::Sprite* new_render=new olc::Sprite(ScreenWidth(), ScreenHeight());
			SetDrawTarget(new_render);
			user_geometry();
			projectAndClip();
			user_render();
			SetDrawTarget(nullptr);

			//replace render view state
			show_render=old_show_render;

			//move render to small render texture
			for(int i=0; i<render_tex->width; i++) {
				for(int j=0; j<render_tex->height; j++) {
					float u=float(i)/render_tex->width;
					float v=float(j)/render_tex->height;
					render_tex->SetPixel(i, j, new_render->Sample(u, v));
				}
			}

			delete new_render;

			watch.stop();
			auto dur=watch.getMicros();
			std::cout<<"  took: "<<dur<<"us ("<<(dur/1000.f)<<" ms)\n";

			return true;
		}

		if(cmd=="keybinds") {
			std::cout<<
				"  ARROWS   look up, down, left, right\n"
				"  WASD     move forward, back, left, right\n"
				"  SPACE    move up/jump\n"
				"  SHIFT    move down\n"
				"  P        add particle\n"
				"  L        set light pos\n"
				"  C        toggle player camera\n"
				"  F        toggle third person\n"
				"  O        toggle outlines\n"
				"  G        toggle grid\n"
				"  N        toggle normals\n"
				"  E        toggle edge view\n"
				"  R        toggle render view\n"
				"  B        toggle depth view\n"
				"  ESC      toggle integrated console\n";

			return true;
		}

		if(cmd=="mousebinds") {
			std::cout<<
				"  LEFT   paint triangles white\n";

			return true;
		}

		if(cmd=="help") {
			std::cout<<
				"  clear        clears the console\n"
				"  reset        removes all particles\n"
				"  import       import model from file\n"
				"  render       update camera render\n"
				"  time         get diagnostics for each stage of game loop\n"
				"  keybinds     which keys to press for this program?\n"
				"  mousebinds   which buttons to press for this program?\n";

			return true;
		}

		std::cout<<"unknown command. type help for list of commands.\n";

		return false;
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

		if(player_camera) {
			//xz movement
			vf3d movement;

			//move forward, backward
			vf3d fb_dir(std::cosf(cam_yaw), 0, std::sinf(cam_yaw));
			if(GetKey(olc::Key::W).bHeld) movement+=.5f*dt*fb_dir;
			if(GetKey(olc::Key::S).bHeld) movement-=.3f*dt*fb_dir;

			//move left, right
			vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
			if(GetKey(olc::Key::A).bHeld) movement+=.4f*dt*lr_dir;
			if(GetKey(olc::Key::D).bHeld) movement-=.4f*dt*lr_dir;

			//walk up hill
			if(player_on_ground) {
				//find closest triangle
				float record;
				const cmn::Triangle* closest=nullptr;
				for(const auto& t:model.triangles) {
					vf3d close_pt=t.getClosePt(player_pos);
					vf3d sub=close_pt-player_pos;
					float dist2=sub.mag2();
					if(!closest||dist2<record) {
						record=dist2;
						closest=&t;
					}
				}

				if(closest) {
					//project movement onto triangle plane
					vf3d norm=closest->getNorm();
					movement-=norm*norm.dot(movement);
				}
			}

			player_pos+=movement;

			//jumping??
			if(GetKey(olc::Key::SPACE).bPressed) {
				//no double jump.
				if(player_airtime<.1f) {
					player_vel.y=175*dt;
					player_on_ground=false;
				}
			}

			//player pos is at feet, so offset camera to head
			cam_pos=player_pos+vf3d(0, player_height, 0);
			if(third_person) {
				cam_pos-=.5f*cam_dir;
			}
		} else {
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
		}

		//add quad at camera
		if(GetKey(olc::Key::P).bPressed) {
			particles.push_back({cam_pos});
		}

		//set light to camera
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//toggle player camera
		if(GetKey(olc::Key::C).bPressed) {
			if(!player_camera) {
				player_pos=cam_pos-vf3d(0, player_height, 0);
				player_vel={0, 0, 0};
			}
			player_camera^=true;
		}

		//ui toggles
		if(GetKey(olc::Key::O).bPressed) show_outlines^=true;
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;
		if(GetKey(olc::Key::N).bPressed) show_normals^=true;
		if(GetKey(olc::Key::E).bPressed) show_edges^=true;
		if(GetKey(olc::Key::R).bPressed) show_render^=true;
		if(GetKey(olc::Key::B).bPressed) show_depth^=true;
		if(GetKey(olc::Key::F).bPressed) third_person^=true;

		//mouse painting?
		if(GetMouse(olc::Mouse::LEFT).bHeld) {
			//matrix could be singular.
			try {
				//screen -> world with inverse matrix
				Mat4 invPV=Mat4::inverse(mat_view*mat_proj);
				float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
				float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
				vf3d clip(ndc_x, ndc_y, 1);
				vf3d world=clip*invPV;
				world/=world.w;

				//check every tri against segment
				vf3d start=cam_pos;
				float record=INFINITY;
				cmn::Triangle* closest=nullptr;
				for(auto& t:model.triangles) {
					float dist=segIntersectTri(start, world, t);
					if(dist>0&&dist<record) {
						record=dist;
						closest=&t;
					}
				}

				//if found, paint
				if(closest) closest->col=olc::WHITE;
			} catch(const std::exception& e) {
				std::cout<<"  "<<e.what()<<'\n';
			}
		}
	}

	bool user_update(float dt) override {
		update_watch.start();
		
		//open and close the integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		//only allow input when console NOT open
		if(!IsConsoleShowing()) handleUserInput(dt);

		if(player_camera) {
			//player kinematics
			if(!player_on_ground) {
				player_vel+=gravity*dt;
			}
			player_pos+=player_vel*dt;

			//airtimer
			player_airtime+=dt;

			//player collisions
			player_on_ground=false;
			for(const auto& t:model.triangles) {
				vf3d close_pt=t.getClosePt(player_pos);
				vf3d sub=player_pos-close_pt;
				float dist2=sub.mag2();
				if(dist2<player_rad*player_rad) {
					float fix=player_rad-std::sqrtf(dist2);
					player_pos+=fix*t.getNorm();
					player_vel={0, 0, 0};
					player_on_ground=true;
					player_airtime=0;
				}
			}
		}

		//"sanitize" particles
		for(auto it=particles.begin(); it!=particles.end();) {
			if(!scene_bounds.contains(it->pos)) {
				it=particles.erase(it);
			} else it++;
		}

		//for each particle...
		for(auto& p:particles) {
			//kinematics
			p.accelerate(gravity);
			p.update(dt);

			//collisions
			for(const auto& t:model.triangles) {
				vf3d close_pt=t.getClosePt(p.pos);
				vf3d sub=p.pos-close_pt;
				float dist2=sub.mag2();
				if(dist2<p.rad*p.rad) {
					float fix=p.rad-std::sqrtf(dist2);
					vf3d norm=t.getNorm();
					p.pos+=fix*norm;
					vf3d vel=p.pos-p.oldpos;
					p.oldpos=p.pos-reflect(vel, norm);
					break;
				}
			}
		}

		update_watch.stop();

		return true;
	}

	bool user_geometry() override {
		geom_watch.start();
		
		//add each particle as a quad
		tris_to_project=model.triangles;
		for(const auto& p:particles) {
			//billboarded to point at camera
			vf3d norm=(p.pos-cam_pos).norm();
			vf3d up(0, 1, 0);
			vf3d rgt=norm.cross(up).norm();
			up=rgt.cross(norm);

			//vertex positioning
			vf3d tl=p.pos+p.rad*(rgt+up);
			vf3d tr=p.pos+p.rad*(rgt-up);
			vf3d bl=p.pos+p.rad*(-rgt+up);
			vf3d br=p.pos+p.rad*(-rgt-up);

			//tesselation
			cmn::Triangle f1{tl, br, tr}; f1.col=p.col;
			tris_to_project.push_back(f1);
			cmn::Triangle f2{tl, bl, br}; f2.col=p.col;
			tris_to_project.push_back(f2);
		}

		if(third_person) {
			vf3d v0=player_pos-player_rad;
			vf3d v7=player_pos+player_rad+vf3d(0, player_height, 0);
			vf3d v1(v7.x, v0.y, v0.z);
			vf3d v2(v0.x, v7.y, v0.z);
			vf3d v3(v7.x, v7.y, v0.z);
			vf3d v4(v0.x, v0.y, v7.z);
			vf3d v5(v7.x, v0.y, v7.z);
			vf3d v6(v0.x, v7.y, v7.z);
			tris_to_project.push_back({v0, v1, v4});
			tris_to_project.push_back({v1, v5, v4});
			tris_to_project.push_back({v0, v2, v1});
			tris_to_project.push_back({v2, v3, v1});
			tris_to_project.push_back({v1, v3, v7});
			tris_to_project.push_back({v1, v7, v5});
			tris_to_project.push_back({v5, v7, v6});
			tris_to_project.push_back({v5, v6, v4});
			tris_to_project.push_back({v0, v6, v2});
			tris_to_project.push_back({v0, v4, v6});
			tris_to_project.push_back({v2, v7, v3});
			tris_to_project.push_back({v2, v6, v7});
			tris_to_project.push_back({v0, v1, v2});
		}

		//add blender-like viewport camera "frustum" lines
		lines_to_project.clear();
		if(show_render) {
			//camera sizing
			float w=.75f, h=w*ScreenHeight()/ScreenWidth();
			float cam_fov_rad=cam_fov_deg/180*cmn::Pi;
			float d=w/2/std::tanf(cam_fov_rad/2);

			//camera positioning
			vf3d pos=render_pos;
			vf3d norm=render_dir;
			vf3d up(0, 1, 0);
			vf3d rgt=norm.cross(up).norm();
			up=rgt.cross(norm);

			//vertex positioning
			vf3d ctr=pos+d*norm;
			vf3d tl=ctr+w/2*rgt+h/2*up;
			vf3d tr=ctr+w/2*rgt-h/2*up;
			vf3d bl=ctr-w/2*rgt+h/2*up;
			vf3d br=ctr-w/2*rgt-h/2*up;

			//show camera wireframe
			cmn::Line l1{tl, tr}; l1.col=olc::PURPLE;
			lines_to_project.push_back(l1);
			cmn::Line l2{tr, br}; l2.col=olc::PURPLE;
			lines_to_project.push_back(l2);
			cmn::Line l3{br, bl}; l3.col=olc::PURPLE;
			lines_to_project.push_back(l3);
			cmn::Line l4{bl, tl}; l4.col=olc::PURPLE;
			lines_to_project.push_back(l4);
			cmn::Line l5{tl, pos}; l5.col=olc::PURPLE;
			lines_to_project.push_back(l5);
			cmn::Line l6{tr, pos}; l6.col=olc::PURPLE;
			lines_to_project.push_back(l6);
			cmn::Line l7{br, pos}; l7.col=olc::PURPLE;
			lines_to_project.push_back(l7);
			cmn::Line l8{bl, pos}; l8.col=olc::PURPLE;
			lines_to_project.push_back(l8);
		}

		//grid lines on xy, yz, zx planes
		if(show_grid) {
			const float res=1;
			int num_x=1+(scene_bounds.max.x-scene_bounds.min.x)/res;
			int num_y=1+(scene_bounds.max.y-scene_bounds.min.y)/res;
			int num_z=1+(scene_bounds.max.z-scene_bounds.min.z)/res;

			//slice thru x: draw y&z lines
			for(int i=0; i<num_x; i++) {
				float x=cmn::map(i, 0, num_x-1, scene_bounds.min.x, scene_bounds.max.x);
				cmn::Line ly{vf3d(x, scene_bounds.min.y, 0), vf3d(x, scene_bounds.max.y, 0)};
				lines_to_project.push_back(ly);
				cmn::Line lz{vf3d(x, 0, scene_bounds.min.z), vf3d(x, 0, scene_bounds.max.z)};
				lines_to_project.push_back(lz);
			}

			//slice thru y: draw z&x lines
			for(int i=0; i<num_y; i++) {
				float y=cmn::map(i, 0, num_y-1, scene_bounds.min.y, scene_bounds.max.y);
				cmn::Line lz{vf3d(0, y, scene_bounds.min.z), vf3d(0, y, scene_bounds.max.z)};
				lines_to_project.push_back(lz);
				cmn::Line lx{vf3d(scene_bounds.min.x, y, 0), vf3d(scene_bounds.max.x, y, 0)};
				lines_to_project.push_back(lx);
			}

			//slice thru z: draw x&y lines
			for(int i=0; i<num_z; i++) {
				float z=cmn::map(i, 0, num_z-1, scene_bounds.min.z, scene_bounds.max.z);
				cmn::Line lx{vf3d(scene_bounds.min.x, 0, z), vf3d(scene_bounds.max.x, 0, z)};
				lines_to_project.push_back(lx);
				cmn::Line ly{vf3d(0, scene_bounds.min.y, z), vf3d(0, scene_bounds.max.y, z)};
				lines_to_project.push_back(ly);
			}

			//draw xyz axes
			const float rad=1000;
			cmn::Line x_axis{vf3d(-rad, 0, 0), vf3d(rad, 0, 0)}; x_axis.col=olc::RED;
			lines_to_project.push_back(x_axis);
			cmn::Line y_axis{vf3d(0, -rad, 0), vf3d(0, rad, 0)}; y_axis.col=olc::BLUE;
			lines_to_project.push_back(y_axis);
			cmn::Line z_axis{vf3d(0, 0, -rad), vf3d(0, 0, rad)}; z_axis.col=olc::GREEN;
			lines_to_project.push_back(z_axis);
		}

		if(show_normals) {
			const float sz=.2f;
			for(const auto& t:tris_to_project) {
				vf3d ctr=t.getCtr();
				cmn::Line l{ctr, ctr+sz*t.getNorm()};
				l.col=t.col;
				lines_to_project.push_back(l);
			}
		}

		geom_watch.stop();

		pc_watch.start();

		return true;
	}

	bool user_render() override {
		pc_watch.stop();
		
		render_watch.start();
		
		//grey background
		Clear(olc::Pixel(180, 180, 180));

		resetBuffers();

		//rasterize all triangles
		for(const auto& t:tris_to_draw) {
			if(show_outlines) {
				DrawDepthLine(
					t.p[0].x, t.p[0].y, t.t[0].w,
					t.p[1].x, t.p[1].y, t.t[1].w,
					t.col, t.id
				);
				DrawDepthLine(
					t.p[1].x, t.p[1].y, t.t[1].w,
					t.p[2].x, t.p[2].y, t.t[2].w,
					t.col, t.id
				);
				DrawDepthLine(
					t.p[2].x, t.p[2].y, t.t[2].w,
					t.p[0].x, t.p[0].y, t.t[0].w,
					t.col, t.id
				);
			} else FillDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].w,
				t.col, t.id
			);
		}

		//rasterize all lines
		for(const auto& l:lines_to_draw) {
			DrawDepthLine(
				l.p[0].x, l.p[0].y, l.t[0].w,
				l.p[1].x, l.p[1].y, l.t[1].w,
				l.col, l.id
			);
		}

		if(show_depth) {
			//how to convert to actual depth?

			//find min and max values
			float min_depth=INFINITY, max_depth=-min_depth;
			for(int i=0; i<ScreenWidth()*ScreenHeight(); i++) {
				const auto& depth=depth_buffer[i];
				if(depth<min_depth) min_depth=depth;
				if(depth>max_depth) max_depth=depth;
			}

			//show all depth values
			for(int i=0; i<ScreenWidth(); i++) {
				for(int j=0; j<ScreenHeight(); j++) {
					float depth=depth_buffer[i+ScreenWidth()*j];
					float shade=cmn::map(depth, min_depth, max_depth, 0, 1);
					Draw(i, j, olc::PixelF(shade, shade, shade));
				}
			}
		}

		//edge detection based on depth values
		if(show_edges) {
			for(int i=1; i<ScreenWidth()-1; i++) {
				for(int j=1; j<ScreenHeight()-1; j++) {
					bool curr=depth_buffer[i+ScreenWidth()*j]==0;
					bool lft=depth_buffer[i-1+ScreenWidth()*j]==0;
					bool rgt=depth_buffer[i+1+ScreenWidth()*j]==0;
					bool top=depth_buffer[i+ScreenWidth()*(j-1)]==0;
					bool btm=depth_buffer[i+ScreenWidth()*(j+1)]==0;
					if(curr!=lft||curr!=rgt||curr!=top||curr!=btm) {
						Draw(i, j, olc::PURPLE);
					}
				}
			}
		}

		//show camera render
		if(show_render) {
			int x=ScreenWidth()-render_tex->width;
			int y=ScreenHeight()-render_tex->height;
			DrawSprite(x, y, render_tex);
			DrawRect(x, y, render_tex->width-1, render_tex->height-1, olc::PURPLE);
			DrawString(x, y, "Render");
		}

		render_watch.stop();

		if(to_time) {
			//get elapsed time for each stage and print
			auto update_dur=update_watch.getMicros(),
				geom_dur=geom_watch.getMicros(),
				pc_dur=pc_watch.getMicros(),
				render_dur=render_watch.getMicros();
			std::cout<<
				"  update: "<<update_dur<<"us ("<<(update_dur/1000.f)<<"ms)\n"
				"  geom: "<<geom_dur<<"us ("<<(geom_dur/1000.f)<<"ms)\n"
				"  project & clip: "<<pc_dur<<"us ("<<(pc_dur/1000.f)<<"ms)\n"
				"  render: "<<render_dur<<"us ("<<(render_dur/1000.f)<<"ms)\n";

			//turn of timing flag
			to_time=false;
		}

		return true;
	}
};

int main() {
	VoxelGame vg;
	bool vsync=true;
	if(vg.Construct(640, 480, 1, 1, false, vsync)) vg.Start();

	return 0;
}