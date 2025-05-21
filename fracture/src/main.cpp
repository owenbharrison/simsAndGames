#include "common/3d/engine_3d.h"
namespace olc {
	static const Pixel PURPLE(144, 0, 255);
	static const Pixel ORANGE(255, 115, 0);
}

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
	
	bool fill_triangles=true;
	bool offset_meshes=true;
	bool show_bounds=false;

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
		
		//debug toggles
		if(GetKey(olc::Key::ENTER).bPressed) to_spin^=true;
		if(GetKey(olc::Key::F).bPressed) fill_triangles^=true;
		if(GetKey(olc::Key::O).bPressed) offset_meshes^=true;
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;

		if(to_spin) {
			rot_x+=.3f*dt;
			rot_y+=.2f*dt;
			rot_z+=.4f*dt;
		}

		return true;
	}

	void addAABB(const AABB3& box, const olc::Pixel& col) {
		//corner vertexes
		const vf3d& v0=box.min, & v7=box.max;
		vf3d v1(v7.x, v0.y, v0.z);
		vf3d v2(v0.x, v7.y, v0.z);
		vf3d v3(v7.x, v7.y, v0.z);
		vf3d v4(v0.x, v0.y, v7.z);
		vf3d v5(v7.x, v0.y, v7.z);
		vf3d v6(v0.x, v7.y, v7.z);
		//bottom
		Line l1{v0, v1}; l1.col=col; lines_to_project.push_back(l1);
		Line l2{v1, v3}; l2.col=col; lines_to_project.push_back(l2);
		Line l3{v3, v2}; l3.col=col; lines_to_project.push_back(l3);
		Line l4{v2, v0}; l4.col=col; lines_to_project.push_back(l4);
		//sides
		Line l5{v0, v4}; l5.col=col; lines_to_project.push_back(l5);
		Line l6{v1, v5}; l6.col=col; lines_to_project.push_back(l6);
		Line l7{v2, v6}; l7.col=col; lines_to_project.push_back(l7);
		Line l8{v3, v7}; l8.col=col; lines_to_project.push_back(l8);
		//top
		Line l9{v4, v5}; l9.col=col; lines_to_project.push_back(l9);
		Line l10{v5, v7}; l10.col=col; lines_to_project.push_back(l10);
		Line l11{v7, v6}; l11.col=col; lines_to_project.push_back(l11);
		Line l12{v6, v4}; l12.col=col; lines_to_project.push_back(l12);
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
				addAABB(pos.getAABB(), olc::PURPLE);
				addAABB(neg.getAABB(), olc::ORANGE);
			}
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

		if(!fill_triangles) {
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