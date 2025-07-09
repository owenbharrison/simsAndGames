#include "common/3d/engine_3d.h"
using cmn::vf3d;

#include "mesh.h"

#include "particle.h"

#include "common/utils.h"
#include "common/stopwatch.h"

struct FXUI : cmn::Engine3D {
	FXUI() {
		sAppName="3D Particle Effects";
	}

	//camera positioning
	float cam_yaw=1.07f;
	float cam_pitch=-.56f;

	Mesh terrain;

	std::list<Particle> particles;
	vf3d buoyancy{0, 9.8f, 0};

	std::vector<olc::Sprite*> texture_atlas;

	bool user_create() override {
		cam_pos={-.47f, 9.19f, -8.36f};

		//try load terrain model
		try {
			terrain=Mesh::loadFromOBJ("assets/terrain.txt");
			terrain.colorNormals();
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		texture_atlas.push_back(new olc::Sprite("assets/smoke.png"));

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

		//add particles at mouse
		if(GetMouse(olc::Mouse::LEFT).bHeld){
			//matrix could be singular.
			try {
				//unprojection matrix
				cmn::Mat4 inv_vp=cmn::Mat4::inverse(mat_view*mat_proj);

				//get ray thru screen mouse pos
				float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
				float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
				vf3d clip(ndc_x, ndc_y, 1);
				vf3d world=clip*inv_vp;
				world/=world.w;
				vf3d mouse_dir=(world-cam_pos).norm();

				//cast ray thru "scene"
				float dist=terrain.intersectRay(cam_pos, mouse_dir);
				if(dist>0) {
					vf3d pt=cam_pos+dist*mouse_dir;

					vf3d dir(cmn::random(-1, 1), cmn::random(-1, 1), cmn::random(-1, 1));
					float speed=cmn::random(1, 2);
					vf3d vel=speed*dir.norm();
					float rot=cmn::random(2*cmn::Pi);
					float rot_vel=cmn::random(-1.5f, 1.5f);
					float size=cmn::random(.5f, 1.5f);
					float size_vel=cmn::random(.3f, .5f);
					particles.push_back(Particle(pt, vel, rot, rot_vel, size, size_vel));
				}
			} catch(const std::exception& e) {}
		}

		//update particles
		for(auto& p:particles) {
			//drag
			p.vel*=1-.85f*dt;
			p.rot_vel*=1-.55f*dt;
			p.size_vel*=1-.25f*dt;

			//kinematics
			p.accelerate(buoyancy);
			p.update(dt);

			//aging?
			p.life-=.25f*dt;
		}

		//remove dead particles
		for(auto it=particles.begin(); it!=particles.end();) {
			if(it->isDead()) {
				it=particles.erase(it);
			} else it++;
		}

		return true;
	}

	bool user_geometry() override {
		//add terrain mesh
		tris_to_project.insert(tris_to_project.end(),
			terrain.triangles.begin(), terrain.triangles.end()
		);

		//sort particles by camera dist for transparency
		particles.sort([&] (const Particle& a, const Particle& b) {
			return (a.pos-cam_pos).mag2()>(b.pos-cam_pos).mag2();
		});

		//add particles as billboards
		for(const auto& p:particles) {
			//basis vectors to point at camera
			vf3d norm=(p.pos-cam_pos).norm();
			vf3d up(0, 1, 0);
			vf3d rgt=norm.cross(up).norm();
			up=rgt.cross(norm);

			//rotate basis vectors w/ rotation matrix
			float c=std::cos(p.rot), s=std::sin(p.rot);
			vf3d new_rgt=c*rgt+s*up;
			vf3d new_up=-s*rgt+c*up;

			//vertex positioning
			vf3d tl=p.pos-p.size/2*new_rgt+p.size/2*new_up;
			vf3d tr=p.pos+p.size/2*new_rgt+p.size/2*new_up;
			vf3d bl=p.pos-p.size/2*new_rgt-p.size/2*new_up;
			vf3d br=p.pos+p.size/2*new_rgt-p.size/2*new_up;

			//texture coords
			cmn::v2d tl_t{0, 0};
			cmn::v2d tr_t{1, 0};
			cmn::v2d bl_t{0, 1};
			cmn::v2d br_t{1, 1};

			//tessellation
			olc::Pixel col=olc::PixelF(1, 1, 1, p.life);
			cmn::Triangle t1{tl, br, tr, tl_t, br_t, tr_t}; t1.col=col, t1.id=0;
			tris_to_project.push_back(t1);
			cmn::Triangle t2{tl, bl, br, tl_t, bl_t, br_t}; t2.col=col, t2.id=0;
			tris_to_project.push_back(t2);
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::Pixel(70, 70, 70));

		//render 3d stuff
		resetBuffers();

		for(const auto& t:tris_to_draw) {
			if(t.id==-1) {
				FillDepthTriangle(
					t.p[0].x, t.p[0].y, t.t[0].w,
					t.p[1].x, t.p[1].y, t.t[1].w,
					t.p[2].x, t.p[2].y, t.t[2].w,
					t.col, t.id
				);
			} else {
				SetPixelMode(olc::Pixel::ALPHA);
				FillTexturedDepthTriangle(
					t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
					t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
					t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
					texture_atlas[t.id], t.col, t.id
				);
				SetPixelMode(olc::Pixel::NORMAL);
			}
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
	FXUI f;
	bool vsync=true;
	if(f.Construct(640, 480, 1, 1, false, vsync)) f.Start();

	return 0;
}