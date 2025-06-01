#include "common/3d/engine_3d.h"
using cmn::vf3d;

constexpr float Pi=3.1415927f;

struct Billboard {
	int id;
	vf3d pos;
	float w=1, h=1;
};

struct Example : cmn::Engine3D {
	Example() {
		sAppName="billboard texturing";
	}

	//camera positioning
	float cam_yaw=-Pi/2;
	float cam_pitch=0;

	std::vector<olc::Sprite*> texture_atlas;
	std::vector<Billboard> billboards;

	bool user_create() override {
		cam_pos={0, 0, 2.5f};
		
		texture_atlas.push_back(new olc::Sprite("assets/mario_8bit.png")); 
		billboards.push_back({0, vf3d(-3, 0, 0)});
		
		texture_atlas.push_back(new olc::Sprite("assets/luigi_8bit.png"));
		billboards.push_back({1, vf3d(-1, 0, 0)});
		
		texture_atlas.push_back(new olc::Sprite("assets/mario.png"));
		billboards.push_back({2, vf3d(1, 0, 0)});
		
		texture_atlas.push_back(new olc::Sprite("assets/luigi.png"));
		billboards.push_back({3, vf3d(3, 0, 0)});
		
		return true;
	}

	bool user_destroy() override {
		for(auto& t:texture_atlas) delete t;
		
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

		return true;
	}

	bool user_geometry() override {
		for(const auto& b:billboards) {
			vf3d norm=(b.pos-cam_pos).norm();
			vf3d up(0, 1, 0);
			vf3d rgt=norm.cross(up).norm();
			up=rgt.cross(norm);

			//vertex positioning
			vf3d tl=b.pos+b.w/2*rgt+b.h/2*up;
			vf3d tr=b.pos+b.w/2*rgt-b.h/2*up;
			vf3d bl=b.pos-b.w/2*rgt+b.h/2*up;
			vf3d br=b.pos-b.w/2*rgt-b.h/2*up;

			//texture coords
			cmn::v2d tl_t{0, 0};
			cmn::v2d tr_t{0, 1};
			cmn::v2d bl_t{1, 0};
			cmn::v2d br_t{1, 1};

			//tesselation
			cmn::Triangle f1{tl, br, tr, tl_t, br_t, tr_t}; f1.id=b.id;
			tris_to_project.push_back(f1);
			cmn::Triangle f2{tl, bl, br, tl_t, bl_t, br_t}; f2.id=b.id;
			tris_to_project.push_back(f2);
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::BLACK);

		//render 3d stuff
		resetBuffers();

		for(const auto& t:tris_to_draw) {
			FillTexturedDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
				texture_atlas[t.id], t.col, t.id
			);
		}

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(640, 320, 1, 1, false, true)) e.Start();

	return 0;
}