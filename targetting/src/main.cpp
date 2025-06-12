#include "common/3d/engine_3d.h"
using olc::vf2d;
namespace olc {
	static const Pixel PURPLE(144, 0, 255);
	static const Pixel ORANGE(255, 115, 0);
}
using cmn::vf3d;
using cmn::Mat4;

#include "mesh.h"

#include "common/utils.h"
namespace cmn {
	vf2d polar(float rad, float angle) {
		return polar_generic<vf2d>(rad, angle);
	}
}

struct Example : cmn::Engine3D {
	Example() {
		sAppName="targetting system";
	}

	//camera positioning
	float cam_yaw=-cmn::Pi/2;
	float cam_pitch=-.1f;

	//ui stuff
	vf2d mouse_pos;
	vf2d prev_mouse_pos;

	vf3d mouse_dir;

	//handles and offsets
	Mesh* trans_mesh=nullptr;
	vf3d trans_plane;
	vf2d trans_start;

	Mesh* rot_mesh=nullptr;
	vf2d rot_start;

	Mesh* scale_mesh=nullptr;
	vf2d scale_start, scale_ctr;
	vf3d base_scale;

	//scene stuff
	std::vector<Mesh> meshes;

	//debug toggles
	bool show_bounds=false;

	//dont want to sort meshes by id...
	int lowestUniqueID() const {
		for(int id=0; ; id++) {
			bool unique=true;
			for(const auto& m:meshes) {
				if(m.id==id) {
					unique=false;
					break;
				}
			}
			if(unique) return id;
		}
	}

	bool user_create() override {
		cam_pos={0, 0, 3.5f};

		//load monkey, dragon, and bunny
		try {
			Mesh a=Mesh::loadFromOBJ("assets/suzanne.txt");
			a.translation={-3, 0, 0};
			meshes.push_back(a);
			Mesh b=Mesh::loadFromOBJ("assets/dragon.txt");
			b.scale={3, 3, 3};
			meshes.push_back(b);
			Mesh c=Mesh::loadFromOBJ("assets/bunny.txt");
			c.scale={10, 10, 10};
			c.translation={3, -.3f, 0};
			meshes.push_back(c);
		} catch(const std::exception& e) {
			std::cout<<e.what()<<'\n';
			return false;
		}

		//update things
		for(auto& m:meshes) {
			m.updateTransforms();
			m.id=lowestUniqueID();
			m.applyTransforms();
			m.colorNormals();
		}

		return true;
	}

	bool user_update(float dt) override {
		//cant move while updating
		if(!trans_mesh&&!rot_mesh&&!scale_mesh) {
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
		}

		//set light pos
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//debug toggles
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;

		//update screen mouse
		prev_mouse_pos=mouse_pos;
		mouse_pos=GetMousePos();

		//unprojection matrix
		Mat4 invVP;
		bool invVP_avail=true;
		try {
			invVP=Mat4::inverse(mat_view*mat_proj);
		} catch(const std::exception& e) {
			invVP_avail=false;
		}

		//update world mouse ray
		if(invVP_avail) {
			float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
			float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
			vf3d clip(ndc_x, ndc_y, 1);
			vf3d world=clip*invVP;
			world/=world.w;

			mouse_dir=(world-cam_pos).norm();
		}

		//get closest mesh
		float mesh_dist=-1;
		Mesh* close_mesh=nullptr;
		for(auto& m:meshes) {
			float dist=m.intersectRay(cam_pos, mouse_dir);
			if(dist>0) {
				if(mesh_dist<0||dist<mesh_dist) {
					mesh_dist=dist;
					close_mesh=&m;
				}
			}
		}

		const auto translate_action=GetMouse(olc::Mouse::LEFT);
		if(translate_action.bPressed) {
			trans_mesh=close_mesh;
			if(trans_mesh) {
				trans_plane=cam_pos+mesh_dist*cam_dir;
				trans_start=mouse_pos;
			}
		}
		if(translate_action.bReleased) trans_mesh=nullptr;

		const auto rotate_action=GetMouse(olc::Mouse::RIGHT);
		if(rotate_action.bPressed) {
			rot_mesh=close_mesh;
			if(rot_mesh) rot_start=mouse_pos;
		}
		if(rotate_action.bReleased) rot_mesh=nullptr;

		const auto scale_action=GetMouse(olc::Mouse::MIDDLE);
		if(scale_action.bPressed) {
			scale_mesh=close_mesh;
			if(scale_mesh) {
				scale_start=mouse_pos;
				base_scale=scale_mesh->scale;
				vf3d ctr_ndc=scale_mesh->translation*mat_view*mat_proj;
				ctr_ndc/=ctr_ndc.w;
				scale_ctr.x=(1-ctr_ndc.x)*ScreenWidth()/2;
				scale_ctr.y=(1-ctr_ndc.y)*ScreenHeight()/2;
			}
		}
		if(scale_action.bReleased) scale_mesh=nullptr;

		//update translated mesh
		if(trans_mesh&&invVP_avail) {
			//project screen ray onto translation plane
			float prev_ndc_x=1-2*prev_mouse_pos.x/ScreenWidth();
			float prev_ndc_y=1-2*prev_mouse_pos.y/ScreenHeight();
			vf3d prev_clip(prev_ndc_x, prev_ndc_y, 1);
			vf3d prev_world=prev_clip*invVP;
			prev_world/=prev_world.w;
			vf3d prev_pt=segIntersectPlane(cam_pos, prev_world, trans_plane, cam_dir);
			float curr_ndc_x=1-2*mouse_pos.x/ScreenWidth();
			float curr_ndc_y=1-2*mouse_pos.y/ScreenHeight();
			vf3d curr_clip(curr_ndc_x, curr_ndc_y, 1);
			vf3d curr_world=curr_clip*invVP;
			curr_world/=curr_world.w;
			vf3d curr_pt=segIntersectPlane(cam_pos, curr_world, trans_plane, cam_dir);

			//apply translation delta and update
			trans_mesh->translation+=curr_pt-prev_pt;
			trans_mesh->updateTransforms();
			trans_mesh->applyTransforms();
			trans_mesh->colorNormals();
		}

		//update rotated mesh
		if(rot_mesh) {
			vf2d b=mouse_pos-rot_start;
			if(b.mag()>20) {
				vf2d a=prev_mouse_pos-rot_start;
				float dot=a.x*b.x+a.y*b.y;
				float cross=a.x*b.y-a.y*b.x;
				float angle=-std::atan2f(cross, dot);
				//apply rotation delta and update
				rot_mesh->rotation=Quat::fromAxisAngle(cam_dir, angle)*rot_mesh->rotation;
				rot_mesh->updateTransforms();
				rot_mesh->applyTransforms();
				rot_mesh->colorNormals();
			}
		}

		//update scaled mesh
		if(scale_mesh) {
			float m2c=(mouse_pos-scale_ctr).mag();
			float s2c=(scale_start-scale_ctr).mag();
			//apply scale delta and update
			scale_mesh->scale=base_scale*m2c/s2c;
			scale_mesh->updateTransforms();
			scale_mesh->applyTransforms();
			scale_mesh->colorNormals();
		}

		return true;
	}

	bool user_geometry() override {
		//combine all meshes triangles
		for(const auto& m:meshes) {
			tris_to_project.insert(tris_to_project.end(),
				m.tris.begin(), m.tris.end()
			);
		}

		//add bound lines
		if(show_bounds) {
			for(const auto& m:meshes) {
				addAABB(m.getAABB(), olc::WHITE);
			}
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::Pixel(150, 150, 150));

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

		//selected mesh edge detection
		for(int k=0; k<3; k++) {
			Mesh* select=nullptr;
			olc::Pixel col;
			switch(k) {
				case 0: select=trans_mesh, col=olc::ORANGE; break;
				case 1: select=rot_mesh, col=olc::RED; break;
				case 2: select=scale_mesh, col=olc::YELLOW; break;
			}
			if(select) {
				int id=select->id;
				for(int i=1; i<ScreenWidth()-1; i++) {
					for(int j=1; j<ScreenHeight()-1; j++) {
						bool curr=id_buffer[i+ScreenWidth()*j]==id;
						bool lft=id_buffer[i-1+ScreenWidth()*j]==id;
						bool rgt=id_buffer[i+1+ScreenWidth()*j]==id;
						bool top=id_buffer[i+ScreenWidth()*(j-1)]==id;
						bool btm=id_buffer[i+ScreenWidth()*(j+1)]==id;
						if(curr!=lft||curr!=rgt||curr!=top||curr!=btm) {
							Draw(i, j, col);
						}
					}
				}
			}
		}

		//draw translation handle
		if(trans_mesh) {
			DrawLine(trans_start, mouse_pos, olc::BLUE);
			DrawLine(trans_start.x-8, trans_start.y, trans_start.x+8, trans_start.y, olc::ORANGE);
			DrawLine(trans_start.x, trans_start.y-8, trans_start.x, trans_start.y+8, olc::ORANGE);
		}

		//draw rotation handle
		if(rot_mesh) {
			vf2d sub=mouse_pos-rot_start;
			float dist=sub.mag();
			float rad=std::max(20.f, dist);
			float angle=std::atan2f(sub.y, sub.x);
			vf2d a=rot_start+cmn::polar(rad, angle);
			vf2d b=rot_start+cmn::polar(rad, angle+.667f*cmn::Pi);
			vf2d c=rot_start+cmn::polar(rad, angle+1.33f*cmn::Pi);
			DrawLine(a, b, olc::GREEN);
			DrawLine(b, c, olc::GREEN);
			DrawLine(c, a, olc::GREEN);
			DrawLine(rot_start.x-8, rot_start.y, rot_start.x+8, rot_start.y, olc::RED);
			DrawLine(rot_start.x, rot_start.y-8, rot_start.x, rot_start.y+8, olc::RED);
		}

		//draw scale handle
		if(scale_mesh) {
			float m2c=(mouse_pos-scale_ctr).mag();
			DrawCircle(scale_ctr, m2c, olc::PURPLE);
			float s2c=(scale_start-scale_ctr).mag();
			DrawCircle(scale_ctr, s2c, olc::PURPLE);
			DrawLine(scale_ctr.x-8, scale_ctr.y, scale_ctr.x+8, scale_ctr.y, olc::YELLOW);
			DrawLine(scale_ctr.x, scale_ctr.y-8, scale_ctr.x, scale_ctr.y+8, olc::YELLOW);
		}

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(540, 360, 1, 1, false, true)) e.Start();

	return 0;
}