#include "common/3d/engine_3d.h"
using olc::vf2d;
using cmn::vf3d;
using cmn::Mat4;

#include "scene.h"

#include "common/utils.h"

struct Example : cmn::Engine3D {
	Example() {
		sAppName="3d builder";
	}

	//camera positioning
	float cam_yaw=-cmn::Pi/2;
	float cam_pitch=-.1f;

	//scene stuff
	Scene scene;

	bool user_create() override {	
		cam_pos={0, 0, 3.5f};

		//try load scene
		try {
			scene=Scene::load("assets/models.scn");
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		//update things
		for(auto& m:scene.meshes) {
			m.updateTransforms();
			m.id=scene.lowestUniqueID();
			m.applyTransforms();
			m.colorNormals();
		}

		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";
		ConsoleCaptureStdOut(true);

		return true;
	}

	//mostly camera controls
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

		return true;
	}

	bool user_geometry() override {
		//combine all scene geometry
		for(const auto& m:scene.meshes) {
			tris_to_project.insert(tris_to_project.end(),
				m.tris.begin(), m.tris.end()
			);
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::Pixel(100, 100, 100));

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

		return true;
	}
};

int main() {
	Example e;
	bool vsync=true;
	if(e.Construct(540, 360, 1, 1, false, vsync)) e.Start();
	
	return 0;
}