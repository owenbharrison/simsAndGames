#define OLC_PGE_APPLICATION
#include "olc/engine_3d.h"
namespace olc {
	static const Pixel PURPLE(144, 0, 255);
	static const Pixel ORANGE(255, 115, 0);
}

#include "mesh.h"

using cmn::vf3d;
using cmn::mat4;

constexpr float Pi=3.1415927f;

//y p => x y z
//0 0 => 0 0 1
static vf3d polar3D(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

struct FractureUI : cmn::Engine3D {
	FractureUI() {
		sAppName="Mesh Fracturing";
	}

	//camera positioning
	float cam_yaw=-2.69f;
	float cam_pitch=-0.254f;

	std::vector<Mesh> meshes;
	Mesh* mesh_to_use=nullptr;

	vf3d rot;
	bool to_spin=true;
	
	bool offset_meshes=true;
	bool show_bounds=false;
	bool fill_triangles=true;

	bool help_menu=false;

	bool user_create() override {
		std::srand(std::time(0));
		
		cam_pos={1.34f, .558f, 2.03f};
		
		{//load meshes
			const std::vector<std::string> filenames{
				"assets/armadillo.txt",
				"assets/bunny.txt",
				"assets/cow.txt",
				"assets/dragon.txt",
				"assets/horse.txt",
				"assets/monkey.txt",
				"assets/jeep.txt"
			};
			for(const auto& f:filenames) {
				meshes.push_back({});
				if(!Mesh::loadFromOBJ(meshes.back(), f)) {
					std::cout<<"  unable to load "<<f<<'\n';

					return false;
				}
			}
		}

		randomizeMesh();

		return true;
	}

	void handleCameraLooking(float dt) {
		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw+=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw-=dt;
	}

	void handleCameraMovement(float dt) {
		//move up, down
		if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*dt;
		if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::sin(cam_yaw), 0, std::cos(cam_yaw));
		if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*dt*fb_dir;
		if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*dt*lr_dir;
		if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*dt*lr_dir;
	}

	void randomizeMesh() {
		int i=std::rand()%meshes.size();
		mesh_to_use=&meshes[i];
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		cam_dir=polar3D(cam_yaw, cam_pitch);

		handleCameraMovement(dt);

		//debug toggles
		if(GetKey(olc::Key::ENTER).bPressed) to_spin^=true;
		if(GetKey(olc::Key::O).bPressed) offset_meshes^=true;
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
		if(GetKey(olc::Key::F).bPressed) fill_triangles^=true;
		if(GetKey(olc::Key::R).bPressed) randomizeMesh();
		if(GetKey(olc::Key::H).bPressed) help_menu^=true;
	}

	bool user_update(float dt) override {
		handleUserInput(dt);

		if(to_spin) {
			rot.x+=.3f*dt;
			rot.y+=.2f*dt;
			rot.z+=.4f*dt;
		}

		return true;
	}

	bool user_geometry() override {
		//add main light at cam_pos
		lights.push_back({cam_pos, olc::WHITE});
		
		//find plane to split about
		mat4 rot_x=mat4::makeRotX(rot.x);
		mat4 rot_y=mat4::makeRotY(rot.y);
		mat4 rot_z=mat4::makeRotZ(rot.z);
		mat4 rot_xyz=mat4::mul(rot_z, mat4::mul(rot_y, rot_x));
		float w=1;
		vf3d norm=matMulVec(rot_xyz, {0, 1, 0}, w);

		//split mesh and color each side accordingly
		Mesh pos, neg;
		if(mesh_to_use->splitByPlane({0, 0, 0}, norm, pos, neg)) {
			if(offset_meshes) {
				vf3d offset=.075f*norm;
				for(auto& v:pos.verts) v+=offset;
				pos.updateTriangles();
				for(auto& v:neg.verts) v-=offset;
				neg.updateTriangles();
			}
			tris_to_project.insert(tris_to_project.end(),
				pos.tris.begin(), pos.tris.end()
			);
			tris_to_project.insert(tris_to_project.end(),
				neg.tris.begin(), neg.tris.end()
			);
			if(show_bounds) {
				addAABB(pos.getAABB(), olc::ORANGE);
				addAABB(neg.getAABB(), olc::PURPLE);
			}
		}

		//show split plane
		{
			vf3d up(0, 1, 0);
			vf3d rgt=up.cross(norm).norm();
			up=norm.cross(rgt);
			vf3d tl=up-rgt, tr=up+rgt;
			vf3d bl=-up-rgt, br=-up+rgt;
			cmn::Line l1{tl, tr}; l1.col=olc::RED;
			lines_to_project.push_back(l1);
			cmn::Line l2{tr, br}; l2.col=olc::RED;
			lines_to_project.push_back(l2);
			cmn::Line l3{br, bl}; l3.col=olc::RED;
			lines_to_project.push_back(l3);
			cmn::Line l4{bl, tl}; l4.col=olc::RED;
			lines_to_project.push_back(l4);
			cmn::Line l5{tl, br}; l5.col=olc::RED;
			lines_to_project.push_back(l5);
		}

		if(!fill_triangles) {
			for(const auto& t:tris_to_project) {
				cmn::Line l1{t.p[0], t.p[1]}; l1.col=t.col;
				lines_to_project.push_back(l1);
				cmn::Line l2{t.p[1], t.p[2]}; l2.col=t.col;
				lines_to_project.push_back(l2);
				cmn::Line l3{t.p[2], t.p[0]}; l3.col=t.col;
				lines_to_project.push_back(l3);
			}
			tris_to_project.clear();
		}

		return true;
	}

	void renderHelpHints() {
		int cx=ScreenWidth()/2;
		if(help_menu) {
			DrawString(8, 8, "Movement Controls");
			DrawString(8, 16, "WASD, Shift, & Space to move");
			DrawString(8, 24, "ARROWS to look around");

			DrawString(ScreenWidth()-8*19, 8, "Toggleable Options");
			DrawString(ScreenWidth()-8*21, 16, "Enter for model spin", to_spin?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*18, 24, "O for mesh offset", offset_meshes?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*21, 32, "B for bounding boxes", show_bounds?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*20, 40, "F for triangle fill", fill_triangles?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*15, 48, "R for new mesh");

			DrawString(cx-4*18, ScreenHeight()-8, "[Press H to close]");
		} else {
			DrawString(cx-4*18, ScreenHeight()-8, "[Press H for help]");
		}
	}

	bool user_render() override {
		Clear(olc::BLACK);

		//render 3d stuff
		resetBuffers();

		for(const auto& t:tris_to_draw) {
			FillDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].z,
				t.p[1].x, t.p[1].y, t.t[1].z,
				t.p[2].x, t.p[2].y, t.t[2].z,
				t.col, t.id
			);
		}

		for(const auto& l:lines_to_draw) {
			DrawDepthLine(
				l.p[0].x, l.p[0].y, l.t[0].z,
				l.p[1].x, l.p[1].y, l.t[1].z,
				l.col, l.id
			);
		}

		renderHelpHints();

		return true;
	}
};

int main() {
	FractureUI fui;
	if(fui.Construct(540, 360, 1, 1, false, true)) fui.Start();

	return 0;
}