#define OLC_PGE_APPLICATION
#include "common/3d/engine_3d.h"
namespace olc {
	static const Pixel PURPLE(144, 0, 255);
	static const Pixel ORANGE(255, 115, 0);
}
using cmn::vf3d;
using cmn::Mat4;

#include "mesh.h"

constexpr float Pi=3.1415927f;

struct Example : cmn::Engine3D {
	Example() {
		sAppName="mesh fracturing";
	}

	//camera positioning
	float cam_yaw=-Pi/2;
	float cam_pitch=0;
	vf3d light_pos;

	Mesh mesh;

	float rot_x=0, rot_y=0, rot_z=0;
	bool to_spin=true;
	
	bool offset_meshes=true;
	bool show_bounds=false;
	bool fill_triangles=true;

	bool user_create() override {
		cam_pos={0, 0, 2};
		light_pos={1, 2, 2};

		try {
			mesh=Mesh::loadFromOBJ("assets/bunny.txt");
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
			mesh=m;

			std::cout<<"  successfully loaded mesh w/ "<<mesh.tris.size()<<"tris\n";

			return true;
		}

		if(cmd=="keybinds") {
			std::cout<<
				"  ARROWS   look up, down, left, right\n"
				"  WASD     move forward, back, left, right\n"
				"  SPACE    move up\n"
				"  SHIFT    move down\n"
				"  L        set light pos\n"
				"  ENTER    toggle spinning\n"
				"  O        toggle offset view\n"
				"  B        toggle bounds view\n"
				"  F        toggle triangle fill\n"
				"  ESC      toggle integrated console\n";

			return true;
		}

		if(cmd=="help") {
			std::cout<<
				"  clear        clears the console\n"
				"  import       import mesh from file\n"
				"  keybinds     which keys to press for this program?\n";

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
		if(GetKey(olc::Key::ENTER).bPressed) to_spin^=true;
		if(GetKey(olc::Key::O).bPressed) offset_meshes^=true;
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
		if(GetKey(olc::Key::F).bPressed) fill_triangles^=true;
	}

	bool user_update(float dt) override {
		//open and close the integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		//only allow input when console NOT open
		if(!IsConsoleShowing()) handleUserInput(dt);

		if(to_spin) {
			rot_x+=.3f*dt;
			rot_y+=.2f*dt;
			rot_z+=.4f*dt;
		}

		return true;
	}

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});
		
		//split mesh and color each side accordingly
		vf3d norm{0, 1, 0};
		Mat4 mat_x=Mat4::makeRotX(rot_x);
		Mat4 mat_y=Mat4::makeRotY(rot_y);
		Mat4 mat_z=Mat4::makeRotZ(rot_z);
		norm=norm*mat_x*mat_y*mat_z;
		Mesh pos, neg;
		if(mesh.splitByPlane({0, 0, 0}, norm, pos, neg)) {
			if(offset_meshes) {
				vf3d offset=.075f*norm;
				for(auto& v:pos.verts) v+=offset;
				pos.triangulate();
				for(auto& v:neg.verts) v-=offset;
				neg.triangulate();
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

	bool user_render() override {
		Clear(olc::BLACK);

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

		//show pause border
		if(!to_spin) DrawRect(0, 0, ScreenWidth()-1, ScreenHeight()-1, olc::BLACK);

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(540, 360, 1, 1, false, true)) e.Start();

	return 0;
}