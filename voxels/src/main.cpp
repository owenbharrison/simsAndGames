#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;
namespace olc {
	static const Pixel ORANGE(255, 115, 0);
}

#include <stack>

#include "math/v3d.h"

#include "math/mat4.h"

//vector-matrix multiplication
vf3d operator*(const vf3d& v, const Mat4& m) {
	vf3d r;
	r.x=v.x*m.v[0][0]+v.y*m.v[1][0]+v.z*m.v[2][0]+v.w*m.v[3][0];
	r.y=v.x*m.v[0][1]+v.y*m.v[1][1]+v.z*m.v[2][1]+v.w*m.v[3][1];
	r.z=v.x*m.v[0][2]+v.y*m.v[1][2]+v.z*m.v[2][2]+v.w*m.v[3][2];
	r.w=v.x*m.v[0][3]+v.y*m.v[1][3]+v.z*m.v[2][3]+v.w*m.v[3][3];
	return r;
}

#include "math/v2d.h"

#include "gfx/mesh.h"
#include "gfx/line.h"
#include "gfx/render.h"

#include "common/stopwatch.h"
#include "common/utils.h"

#include "particle.h"

vf3d reflect(const vf3d& in, const vf3d& norm) {
	return in-2*norm.dot(in)*norm;
}

struct VoxelGame : olc::PixelGameEngine {
	VoxelGame() {
		sAppName="Voxel Game";
	}

	//camera positioning
	vf3d cam_pos{0, 2, -9};
	float cam_yaw=cmn::Pi/2;
	float cam_pitch=-.3f;
	vf3d cam_dir;

	//camera matrices
	Mat4 mat_proj, mat_view;

	//graphics stuff
	Mesh model;
	const AABB3 scene_bounds{{-5, -5, -5}, {5, 5, 5}};
	vf3d light_pos=cam_pos;

	//vector is faster than list!
	std::vector<Triangle> tris_to_project;
	std::vector<Line> lines_to_project;
	std::vector<Triangle> tris_to_draw;
	std::vector<Line> lines_to_draw;

	//depth buffering
	float* depth_buffer=nullptr;
	bool inRangeX(int x) const { return x>=0&&x<ScreenWidth(); }
	bool inRangeY(int y) const { return y>=0&&y<ScreenHeight(); }

	//renderer stuff
	Render renderer;
	olc::Sprite* renderer_tex=nullptr;

	//toggles
	bool show_outlines=false;
	bool show_render=false;
	bool show_edges=false;
	bool show_grid=false;
	bool show_depth=false;
	bool show_normals=false;

	//diagnostic flag
	bool to_time=false;

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

	bool OnUserCreate() override {
		srand(time(0));

		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";
		ConsoleCaptureStdOut(true);

		//make projection matrix
		float asp=float(ScreenHeight())/ScreenWidth();
		mat_proj=Mat4::makeProj(90, asp, .001f, 1000.f);

		depth_buffer=new float[ScreenWidth()*ScreenHeight()];

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

		//setup renderer stuff
		renderer=Render(128, 96, 90);
		renderer_tex=new olc::Sprite(renderer.getWidth(), renderer.getHeight());

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
			std::cout<<"  removing particles\n";

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

			//rerender scene
			renderer.updatePos(cam_pos, cam_dir, light_pos);
			renderer.resetBuffers();
			renderer.render(model.triangles);

			//update render texture
			for(int i=0; i<renderer.getWidth(); i++) {
				for(int j=0; j<renderer.getHeight(); j++) {
					int k=renderer.ix(i, j);
					renderer_tex->SetPixel(i, j, olc::Pixel(
						renderer.pixels[3*k],
						renderer.pixels[1+3*k],
						renderer.pixels[2+3*k]
					));
				}
			}

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
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw-=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw+=dt;

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
				const Triangle* closest=nullptr;
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
				Triangle* closest=nullptr;
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

	bool update(float dt) {
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
	}

	void geometry() {
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
			Triangle f1{tl, br, tr}; f1.col=p.col;
			tris_to_project.push_back(f1);
			Triangle f2{tl, bl, br}; f2.col=p.col;
			tris_to_project.push_back(f2);
		}

		if(third_person) {
			AABB3 box{
				player_pos-player_rad,
				player_pos+player_rad+vf3d(0, player_height, 0)
			};
			vf3d v0=box.min, v7=box.max;
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
			float w=.75f, h=w*renderer.getHeight()/renderer.getWidth();
			float d=w/2/std::tanf(renderer.getFOVRad()/2);

			//camera positioning
			vf3d pos=renderer.getCamPos();
			vf3d norm=renderer.getCamDir();
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
			Line l1{tl, tr}; l1.col=olc::ORANGE;
			lines_to_project.push_back(l1);
			Line l2{tr, br}; l2.col=olc::ORANGE;
			lines_to_project.push_back(l2);
			Line l3{br, bl}; l3.col=olc::ORANGE;
			lines_to_project.push_back(l3);
			Line l4{bl, tl}; l4.col=olc::ORANGE;
			lines_to_project.push_back(l4);
			Line l5{tl, pos}; l5.col=olc::ORANGE;
			lines_to_project.push_back(l5);
			Line l6{tr, pos}; l6.col=olc::ORANGE;
			lines_to_project.push_back(l6);
			Line l7{br, pos}; l7.col=olc::ORANGE;
			lines_to_project.push_back(l7);
			Line l8{bl, pos}; l8.col=olc::ORANGE;
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
				Line ly{vf3d(x, scene_bounds.min.y, 0), vf3d(x, scene_bounds.max.y, 0)};
				lines_to_project.push_back(ly);
				Line lz{vf3d(x, 0, scene_bounds.min.z), vf3d(x, 0, scene_bounds.max.z)};
				lines_to_project.push_back(lz);
			}

			//slice thru y: draw z&x lines
			for(int i=0; i<num_y; i++) {
				float y=cmn::map(i, 0, num_y-1, scene_bounds.min.y, scene_bounds.max.y);
				Line lz{vf3d(0, y, scene_bounds.min.z), vf3d(0, y, scene_bounds.max.z)};
				lines_to_project.push_back(lz);
				Line lx{vf3d(scene_bounds.min.x, y, 0), vf3d(scene_bounds.max.x, y, 0)};
				lines_to_project.push_back(lx);
			}

			//slice thru z: draw x&y lines
			for(int i=0; i<num_z; i++) {
				float z=cmn::map(i, 0, num_z-1, scene_bounds.min.z, scene_bounds.max.z);
				Line lx{vf3d(scene_bounds.min.x, 0, z), vf3d(scene_bounds.max.x, 0, z)};
				lines_to_project.push_back(lx);
				Line ly{vf3d(0, scene_bounds.min.y, z), vf3d(0, scene_bounds.max.y, z)};
				lines_to_project.push_back(ly);
			}

			//draw xyz axes
			const float rad=1000;
			Line x_axis{vf3d(-rad, 0, 0), vf3d(rad, 0, 0)}; x_axis.col=olc::RED;
			lines_to_project.push_back(x_axis);
			Line y_axis{vf3d(0, -rad, 0), vf3d(0, rad, 0)}; y_axis.col=olc::GREEN;
			lines_to_project.push_back(y_axis);
			Line z_axis{vf3d(0, 0, -rad), vf3d(0, 0, rad)}; z_axis.col=olc::BLUE;
			lines_to_project.push_back(z_axis);
		}

		if(show_normals) {
			const float sz=.2f;
			for(const auto& t:tris_to_project) {
				vf3d ctr=t.getCtr();
				Line l{ctr, ctr+sz*t.getNorm()};
				l.col=t.col;
				lines_to_project.push_back(l);
			}
		}
	}

	void projectAndClip() {
		//recalculate matrices
		{
			const vf3d up(0, 1, 0);
			cam_dir=vf3d(
				std::cosf(cam_yaw)*std::cosf(cam_pitch),
				std::sinf(cam_pitch),
				std::sinf(cam_yaw)*std::cosf(cam_pitch)
			);
			vf3d target=cam_pos+cam_dir;
			Mat4 mat_cam=Mat4::makePointAt(cam_pos, target, up);
			mat_view=Mat4::quickInverse(mat_cam);
		}

		//clip triangles against near plane
		std::vector<Triangle> tris_to_clip;
		for(const auto& tri:tris_to_project) {
			vf3d norm=tri.getNorm();

			//is triangle pointing towards me? culling
			if(norm.dot(tri.p[0]-cam_pos)<0) {
				//lighting
				vf3d light_dir=(light_pos-tri.getCtr()).norm();
				float dp=std::max(.5f, norm.dot(light_dir));

				//transform triangles given camera positioning
				Triangle tri_view;
				for(int i=0; i<3; i++) {
					tri_view.p[i]=tri.p[i]*mat_view;
					tri_view.t[i]=tri.t[i];
				}
				tri_view.col=tri.col*dp;

				//clip
				Triangle clipped[2];
				int num=tri_view.clipAgainstPlane(vf3d(0, 0, .1f), vf3d(0, 0, 1), clipped[0], clipped[1]);
				for(int i=0; i<num; i++) {
					Triangle tri_proj;
					//for each vert
					for(int j=0; j<3; j++) {
						//project
						tri_proj.p[j]=clipped[i].p[j]*mat_proj;

						//copy over texture stuff
						tri_proj.t[j]=clipped[i].t[j];

						//w normalization?
						float w=tri_proj.p[j].w;
						tri_proj.t[j].u/=w;
						tri_proj.t[j].v/=w;
						tri_proj.t[j].w=1/w;

						//scale into view
						tri_proj.p[j]/=w;

						//x/y inverted so put them back
						tri_proj.p[j].x*=-1;
						tri_proj.p[j].y*=-1;

						//offset into visible normalized space
						tri_proj.p[j]+=vf3d(1, 1, 0);
						tri_proj.p[j].x*=ScreenWidth()/2;
						tri_proj.p[j].y*=ScreenHeight()/2;
					}
					tri_proj.col=clipped[i].col;

					tris_to_clip.push_back(tri_proj);
				}
			}
		}

		//clip lines against near plane
		std::vector<Line> lines_to_clip;
		for(const auto& line:lines_to_project) {
			//transform triangles given camera positioning
			Line line_view;
			for(int i=0; i<2; i++) {
				line_view.p[i]=line.p[i]*mat_view;
				line_view.t[i]=line.t[i];
			}
			line_view.col=line.col;

			//clip
			Line clipped;
			if(line_view.clipAgainstPlane(vf3d(0, 0, .1f), vf3d(0, 0, 1), clipped)) {
				Line line_proj;
				//for each vert
				for(int j=0; j<2; j++) {
					//project
					line_proj.p[j]=clipped.p[j]*mat_proj;

					//copy over texture stuff
					line_proj.t[j]=clipped.t[j];

					//w normalization?
					float w=line_proj.p[j].w;
					line_proj.t[j].u/=w;
					line_proj.t[j].v/=w;
					line_proj.t[j].w=1/w;

					//scale into view
					line_proj.p[j]/=w;

					//x/y inverted so put them back
					line_proj.p[j].x*=-1;
					line_proj.p[j].y*=-1;

					//offset into visible normalized space
					line_proj.p[j]+=vf3d(1, 1, 0);
					line_proj.p[j].x*=ScreenWidth()/2;
					line_proj.p[j].y*=ScreenHeight()/2;
				}
				line_proj.col=clipped.col;

				lines_to_clip.push_back(line_proj);
			}
		}

		//left,right,top,bottom
		const vf3d ctrs[4]{
			vf3d(0, 0, 0),
			vf3d(ScreenWidth(), 0, 0),
			vf3d(0, 0, 0),
			vf3d(0, ScreenHeight(), 0)
		};
		const vf3d norms[4]{
			vf3d(1, 0, 0),
			vf3d(-1, 0, 0),
			vf3d(0, 1, 0),
			vf3d(0, -1, 0)
		};

		//clip tris against edges of screen
		std::stack<Triangle> tri_queue;
		tris_to_draw.clear();
		for(const auto& t:tris_to_clip) {
			tri_queue.push(t);
			int num_new=1;
			Triangle clipped[2];
			for(int i=0; i<4; i++) {
				//iteratively clip all tris
				while(num_new>0) {
					Triangle test=tri_queue.top();
					tri_queue.pop();
					num_new--;

					int num_clip=test.clipAgainstPlane(ctrs[i], norms[i], clipped[0], clipped[1]);
					for(int j=0; j<num_clip; j++) tri_queue.push(clipped[j]);
				}
				num_new=tri_queue.size();
			}

			//add to conglomerate
			while(!tri_queue.empty()) {
				tris_to_draw.push_back(tri_queue.top());
				tri_queue.pop();
			}
		}

		//clip lines against edges of screen
		std::stack<Line> line_queue;
		lines_to_draw.clear();
		for(const auto& l:lines_to_clip) {
			Line clipped;
			line_queue.push(l);
			int num_new=1;
			for(int i=0; i<4; i++) {
				//iteratively clip all lines
				while(num_new>0) {
					Line test=line_queue.top();
					line_queue.pop();
					num_new--;

					if(test.clipAgainstPlane(ctrs[i], norms[i], clipped)) {
						line_queue.push(clipped);
					}
				}
				num_new=line_queue.size();
			}

			//add to conglomerate
			while(!line_queue.empty()) {
				lines_to_draw.push_back(line_queue.top());
				line_queue.pop();
			}
		}
	}

#pragma region RENDER HELPERS
	void DrawDepthLine( 
		int x1, int y1, float w1,
		int x2, int y2, float w2,
		olc::Pixel col
	) {
		auto draw=[&] (int x, int y, float t) {
			if(!inRangeX(x)||!inRangeY(y)) return;

			float& depth=depth_buffer[x+ScreenWidth()*y];
			float w=w1+t*(w2-w1);
			if(w>depth) {
				Draw(x, y, col);
				depth=w;
			}
		};
		int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;
		dx=x2-x1; dy=y2-y1;
		dx1=abs(dx), dy1=abs(dy);
		px=2*dy1-dx1, py=2*dx1-dy1;
		if(dy1<=dx1) {
			bool flip=false;
			if(dx>=0) x=x1, y=y1, xe=x2;
			else x=x2, y=y2, xe=x1, flip=true;
			draw(x, y, flip);
			float t_step=1.f/dx1;
			for(i=0; x<xe; i++) {
				x++;
				if(px<0) px+=2*dy1;
				else {
					if((dx<0&&dy<0)||(dx>0&&dy>0)) y++;
					else y--;
					px+=2*(dy1-dx1);
				}
				draw(x, y, flip?1-t_step*i:t_step*i);
			}
		} else {
			bool flip=false;
			if(dy>=0) x=x1, y=y1, ye=y2;
			else x=x2, y=y2, ye=y1, flip=true;
			draw(x, y, flip);
			float t_step=1.f/dy1;
			for(i=0; y<ye; i++) {
				y++;
				if(py<=0) py+=2*dx1;
				else {
					if((dx<0&&dy<0)||(dx>0&&dy>0)) x++;
					else x--;
					py+=2*(dx1-dy1);
				}
				draw(x, y, flip?1-t_step*i:t_step*i);
			}
		}
	}

	void DrawDepthTriangle(
		int x1, int y1, float w1,
		int x2, int y2, float w2,
		int x3, int y3, float w3,
		olc::Pixel col
	) {
		DrawDepthLine(x1, y1, w1, x2, y2, w2, col);
		DrawDepthLine(x2, y2, w2, x3, y3, w3, col);
		DrawDepthLine(x3, y3, w3, x1, y1, w1, col);
	}

	void FillDepthTriangle(
		int x1, int y1, float u1, float v1, float w1,
		int x2, int y2, float u2, float v2, float w2,
		int x3, int y3, float u3, float v3, float w3,
		olc::Pixel col//olc::Sprite*s
	) {
		//sort by y
		if(y2<y1) {
			std::swap(x1, x2);
			std::swap(y1, y2);
			std::swap(u1, u2);
			std::swap(v1, v2);
			std::swap(w1, w2);
		}
		if(y3<y1) {
			std::swap(x1, x3);
			std::swap(y1, y3);
			std::swap(u1, u3);
			std::swap(v1, v3);
			std::swap(w1, w3);
		}
		if(y3<y2) {
			std::swap(x2, x3);
			std::swap(y2, y3);
			std::swap(u2, u3);
			std::swap(v2, v3);
			std::swap(w2, w3);
		}

		//calculate slopes
		int dx1=x2-x1;
		int dy1=y2-y1;
		float du1=u2-u1;
		float dv1=v2-v1;
		float dw1=w2-w1;

		int dx2=x3-x1;
		int dy2=y3-y1;
		float du2=u3-u1;
		float dv2=v3-v1;
		float dw2=w3-w1;

		float tex_u, tex_v, tex_w;

		float dax_step=0, dbx_step=0,
			du1_step=0, dv1_step=0,
			du2_step=0, dv2_step=0,
			dw1_step=0, dw2_step=0;

		if(dy1) dax_step=dx1/std::fabsf(dy1);
		if(dy2) dbx_step=dx2/std::fabsf(dy2);

		if(dy1) du1_step=du1/std::fabsf(dy1);
		if(dy1) dv1_step=dv1/std::fabsf(dy1);
		if(dy1) dw1_step=dw1/std::fabsf(dy1);
		if(dy2) du2_step=du2/std::fabsf(dy2);
		if(dy2) dv2_step=dv2/std::fabsf(dy2);
		if(dy2) dw2_step=dw2/std::fabsf(dy2);

		//start scanline filling triangles
		if(dy1) {
			for(int j=y1; j<=y2; j++) {
				int ax=x1+dax_step*(j-y1);
				int bx=x1+dbx_step*(j-y1);
				float tex_su=u1+du1_step*(j-y1);
				float tex_sv=v1+dv1_step*(j-y1);
				float tex_sw=w1+dw1_step*(j-y1);
				float tex_eu=u1+du2_step*(j-y1);
				float tex_ev=v1+dv2_step*(j-y1);
				float tex_ew=w1+dw2_step*(j-y1);
				//sort along x
				if(ax>bx) {
					std::swap(ax, bx);
					std::swap(tex_su, tex_eu);
					std::swap(tex_sv, tex_ev);
					std::swap(tex_sw, tex_ew);
				}
				float t_step=1.f/(bx-ax);
				float t=0;
				for(int i=ax; i<bx; i++) {
					tex_u=tex_su+t*(tex_eu-tex_su);
					tex_v=tex_sv+t*(tex_ev-tex_sv);
					tex_w=tex_sw+t*(tex_ew-tex_sw);
					if(inRangeX(i)&&inRangeY(j)) {
						float& depth=depth_buffer[i+ScreenWidth()*j];
						if(tex_w>depth) {
							Draw(i, j, col);//s->Sample(tex_u/tex_w, tex_v/tex_w));
							depth=tex_w;
						}
					}
					t+=t_step;
				}
			}
		}

		//recalculate slopes
		dx1=x3-x2;
		dy1=y3-y2;
		du1=u3-u2;
		dv1=v3-v2;
		dw1=w3-w2;

		if(dy1) dax_step=dx1/std::fabsf(dy1);

		du1_step=0, dv1_step=0;
		if(dy1) du1_step=du1/std::fabsf(dy1);
		if(dy1) dv1_step=dv1/std::fabsf(dy1);
		if(dy1) dw1_step=dw1/std::fabsf(dy1);

		for(int j=y2; j<=y3; j++) {
			int ax=x2+dax_step*(j-y2);
			int bx=x1+dbx_step*(j-y1);
			float tex_su=u2+du1_step*(j-y2);
			float tex_sv=v2+dv1_step*(j-y2);
			float tex_sw=w2+dw1_step*(j-y2);
			float tex_eu=u1+du2_step*(j-y1);
			float tex_ev=v1+dv2_step*(j-y1);
			float tex_ew=w1+dw2_step*(j-y1);
			//sort along x
			if(ax>bx) {
				std::swap(ax, bx);
				std::swap(tex_su, tex_eu);
				std::swap(tex_sv, tex_ev);
				std::swap(tex_sw, tex_ew);
			}
			float t_step=1.f/(bx-ax);
			float t=0;
			for(int i=ax; i<bx; i++) {
				tex_u=tex_su+t*(tex_eu-tex_su);
				tex_v=tex_sv+t*(tex_ev-tex_sv);
				tex_w=tex_sw+t*(tex_ew-tex_sw);
				if(inRangeX(i)&&inRangeY(j)) {
					float& depth=depth_buffer[i+ScreenWidth()*j];
					if(tex_w>depth) {
						Draw(i, j, col);//s->Sample(tex_u/tex_w, tex_v/tex_w));
						depth=tex_w;
					}
				}
				t+=t_step;
			}
		}
	}
#pragma endregion

	void render() {
		//dark grey background
		Clear(olc::Pixel(90, 90, 90));

		//reset depth buffer
		for(int i=0; i<ScreenWidth()*ScreenHeight(); i++) {
			depth_buffer[i]=0;
		}

		//rasterize all triangles
		for(const auto& t:tris_to_draw) {
			if(show_outlines) {
				//these arent SUPER visible, but its not a big deal...
				DrawDepthTriangle(
					t.p[0].x, t.p[0].y, t.t[0].w,
					t.p[1].x, t.p[1].y, t.t[1].w,
					t.p[2].x, t.p[2].y, t.t[2].w,
					t.col
				);
			} else FillDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
				t.col
			);
		}

		//rasterize all lines
		for(const auto& l:lines_to_draw) {
			DrawDepthLine(
				l.p[0].x, l.p[0].y, l.t[0].w,
				l.p[1].x, l.p[1].y, l.t[1].w,
				l.col
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
						Draw(i, j, olc::ORANGE);
					}
				}
			}
		}

		//little camera render in bottom of screen
		if(show_render) {
			vf2d corner(ScreenWidth()-renderer.getWidth(), ScreenHeight()-renderer.getHeight());
			DrawSprite(corner, renderer_tex);
			DrawString(corner, "Render");
		}
	}

	//this function serves as a wrapper for timing
	//and encapsulation of the stages of the graphics pipeline
	bool OnUserUpdate(float dt) override {
		//setup timers
		cmn::Stopwatch update_timer, geom_timer, pc_timer, render_timer;
		
		//user update, physics
		update_timer.start();
		update(dt);
		update_timer.stop();

		//adding temporary geometry
		geom_timer.start();
		geometry();
		geom_timer.stop();

		//project to screen 
		//  and clip against frustum
		pc_timer.start();
		projectAndClip();
		pc_timer.stop();

		//render triangles, lines
		//  and debug stuff to screen
		render_timer.start();
		render();
		render_timer.stop();

		if(to_time) {
			//get elapsed time for each stage and print
			auto update_dur=update_timer.getMicros(),
				geom_dur=geom_timer.getMicros(),
				pc_dur=pc_timer.getMicros(),
				render_dur=render_timer.getMicros();
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