#include "common/3d/engine_3d.h"

#include "mesh.h"

constexpr float Pi=3.1415927f;

struct Example : cmn::Engine3D {
	Example() {
		sAppName="mesh fracturing";
	}

	//camera positioning
	float cam_yaw=-Pi/2;
	float cam_pitch=0;

	Mesh mesh;

	float rot_x=0, rot_y=0, rot_z=0;
	bool to_spin=true;
	
	bool show_outlines=false;

	bool user_create() override {
		cam_pos={0, 0, 2};
		light_pos={1, 2, 2};

		try {
			mesh=Mesh::loadFromOBJ("assets/bunny.txt");
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		return true;
	}

	bool user_update(float dt) override {
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
		
		if(GetKey(olc::Key::ENTER).bPressed) to_spin^=true;
		
		if(GetKey(olc::Key::O).bPressed) show_outlines^=true;

		if(to_spin) {
			rot_x+=.3f*dt;
			rot_y+=.2f*dt;
			rot_z+=.4f*dt;
		}

		return true;
	}

	bool user_geometry() override {
		//split mesh and color each side accordingly
		vf3d norm{0, 1, 0};
		Mat4 mat_x=Mat4::makeRotX(rot_x);
		Mat4 mat_y=Mat4::makeRotY(rot_y);
		Mat4 mat_z=Mat4::makeRotZ(rot_z);
		norm=norm*mat_x*mat_y*mat_z;
		Mesh pos, neg;
		if(mesh.splitByPlane({0, 0, 0}, norm, pos, neg)) {
			pos.color(olc::BLUE);
			tris_to_project.insert(tris_to_project.end(),
				pos.tris.begin(), pos.tris.end()
			);
			neg.color(olc::GREEN);
			tris_to_project.insert(tris_to_project.end(),
				neg.tris.begin(), neg.tris.end()
			);
		}

		//show split plane
		{
			vf3d up(0, 1, 0);
			vf3d rgt=up.cross(norm).norm();
			up=norm.cross(rgt);
			vf3d tl=up-rgt, tr=up+rgt;
			vf3d bl=-up-rgt, br=-up+rgt;
			Line l1{tl, tr}; l1.col=olc::RED;
			lines_to_project.push_back(l1);
			Line l2{tr, br}; l2.col=olc::RED;
			lines_to_project.push_back(l2);
			Line l3{br, bl}; l3.col=olc::RED;
			lines_to_project.push_back(l3);
			Line l4{bl, tl}; l4.col=olc::RED;
			lines_to_project.push_back(l4);
			Line l5{tl, br}; l5.col=olc::RED;
			lines_to_project.push_back(l5);
		}

		if(show_outlines) {
			for(const auto& t:tris_to_project) {
				Line l1{t.p[0], t.p[1]}; l1.col=t.col;
				lines_to_project.push_back(l1);
				Line l2{t.p[1], t.p[2]}; l2.col=t.col;
				lines_to_project.push_back(l2);
				Line l3{t.p[2], t.p[0]}; l3.col=t.col;
				lines_to_project.push_back(l3);
			}
			tris_to_project.clear();
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::DARK_GREY);

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