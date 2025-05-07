#include "common/3d/engine_3d.h"

#include "mesh.h"
#include "common/utils.h"

struct Example : cmn::Engine3D {
	Example() {
		sAppName="targetting system";
	}

	//camera positioning
	float cam_yaw=0;
	float cam_pitch=0;

	//scene stuff
	Mesh mesh;
	vf3d* aspect=nullptr;

	bool user_create() override {
		try{
			Mesh m=Mesh::loadFromOBJ("assets/suzanne.txt");
			m.colorNormals();
			mesh=m;
			aspect=&mesh.rotation;
		} catch(const std::exception& e) {
			std::cout<<e.what()<<'\n';
			return false;
		}

		return true;
	}

	bool user_update(float dt) override {
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

		//transform testing
		if(GetKey(olc::Key::K1).bPressed) aspect=&mesh.rotation;
		if(GetKey(olc::Key::K2).bPressed) aspect=&mesh.scale;
		if(GetKey(olc::Key::K3).bPressed) aspect=&mesh.translation;

		//transform testing
		bool to_update=false;
		int sign=1-2*GetKey(olc::Key::CTRL).bHeld;
		if(GetKey(olc::Key::X).bHeld) aspect->x+=sign*dt, to_update=true;
		if(GetKey(olc::Key::Y).bHeld) aspect->y+=sign*dt, to_update=true;
		if(GetKey(olc::Key::Z).bHeld) aspect->z+=sign*dt, to_update=true;
		if(to_update) {
			mesh.updateMatrices();
			mesh.updateTris();
			mesh.colorNormals();
		}

		return true;
	}

	bool user_geometry() override {
		//combine all meshes triangles
		tris_to_project.insert(tris_to_project.end(), mesh.tris.begin(), mesh.tris.end());

		return true;
	}

	bool user_render() override {
		Clear(olc::Pixel(100, 100, 100));

		render3D();
		
		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(400, 400, 1, 1, false, true)) e.Start();

	return 0;
}