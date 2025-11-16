#define OLC_PGE_APPLICATION
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

struct Example : cmn::Engine3D {
	Example() {
		sAppName="targetting system";
	}

	//camera positioning
	float cam_yaw=-cmn::Pi/2;
	float cam_pitch=-.1f;
	vf3d light_pos;

	//ui stuff
	vf2d mouse_pos;
	vf2d prev_mouse_pos;

	vf3d mouse_dir, prev_mouse_dir;

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
	bool realize_bounds=false;

	bool help_menu=false;

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
		{
			ReturnCode status;

			Mesh monkey;
			status=Mesh::loadFromOBJ(monkey, "assets/suzanne.txt");
			if(!status.valid) {
				std::cout<<"  "<<status.msg<<'\n';

				return false;
			}
			monkey.translation={-3, 0, 0};
			meshes.push_back(monkey);

			Mesh dragon;
			status=Mesh::loadFromOBJ(dragon, "assets/dragon.txt");
			if(!status.valid) {
				std::cout<<"  "<<status.msg<<'\n';

				return false;
			}
			dragon.scale={3, 3, 3};
			meshes.push_back(dragon);

			Mesh bunny;
			status=Mesh::loadFromOBJ(bunny, "assets/bunny.txt");
			if(!status.valid) {
				std::cout<<"  "<<status.msg<<'\n';

				return false;
			}
			bunny.scale={12, 12, 12};
			bunny.translation={3, -.3f, 0};
			meshes.push_back(bunny);
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

#pragma region UPDATE HELPERS
	void handleCameraLooking(float dt) {
		//cant look while updating
		if(trans_mesh||rot_mesh||scale_mesh) return;
		
		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>cmn::Pi/2) cam_pitch=cmn::Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-cmn::Pi/2) cam_pitch=.001f-cmn::Pi/2;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw-=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw+=dt;
	}

	void handleCameraMovement(float dt) {
		//cant move while updating
		if(trans_mesh||rot_mesh||scale_mesh) return;

		//move up, down
		if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*dt;
		if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::cos(cam_yaw), 0, std::sin(cam_yaw));
		if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*dt*fb_dir;
		if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*dt*lr_dir;
		if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*dt*lr_dir;
	}

	void handleMouseRay() {
		prev_mouse_dir=mouse_dir;

		//unprojection matrix
		Mat4 inv_vp=Mat4::inverse(mat_view*mat_proj);

		float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
		float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
		vf3d clip(ndc_x, ndc_y, 1);
		vf3d world=clip*inv_vp;
		world/=world.w;

		mouse_dir=(world-cam_pos).norm();
	}

	void handleTransActionUpdate() {
		if(!trans_mesh) return;

		//project screen ray onto translation plane
		vf3d prev_pt=segIntersectPlane(cam_pos, cam_pos+prev_mouse_dir, trans_plane, cam_dir);
		vf3d curr_pt=segIntersectPlane(cam_pos, cam_pos+mouse_dir, trans_plane, cam_dir);

		//apply translation delta and update
		trans_mesh->translation+=curr_pt-prev_pt;
		trans_mesh->updateTransforms();
		trans_mesh->applyTransforms();
		trans_mesh->colorNormals();
	}

	void handleRotActionUpdate() {
		if(!rot_mesh) return;

		vf2d b=mouse_pos-rot_start;
		if(b.mag()>20) {
			vf2d a=prev_mouse_pos-rot_start;
			float dot=a.x*b.x+a.y*b.y;
			float cross=a.x*b.y-a.y*b.x;
			float angle=-std::atan2(cross, dot);
			//apply rotation delta and update
			rot_mesh->rotation=rot_mesh->rotation*Quat::fromAxisAngle(cam_dir, angle);
			rot_mesh->updateTransforms();
			rot_mesh->applyTransforms();
			rot_mesh->colorNormals();
		}
	}

	void handleScaleActionUpdate() {
		if(!scale_mesh) return;

		//get scale delta
		float m2c=(mouse_pos-scale_ctr).mag();
		float s2c=(scale_start-scale_ctr).mag();

		//apply scale delta and update
		scale_mesh->scale=base_scale*m2c/s2c;
		scale_mesh->updateTransforms();
		scale_mesh->applyTransforms();
		scale_mesh->colorNormals();
	}
#pragma endregion

	bool user_update(float dt) override {
		handleCameraLooking(dt);

		//polar to cartesian
		cam_dir=vf3d(
			std::cos(cam_yaw)*std::cos(cam_pitch),
			std::sin(cam_pitch),
			std::sin(cam_yaw)*std::cos(cam_pitch)
		);

		handleCameraMovement(dt);

		//update screen mouse
		prev_mouse_pos=mouse_pos;
		mouse_pos=GetMousePos();

		handleMouseRay();

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

		const auto translate_action=GetKey(olc::Key::T);
		if(translate_action.bPressed) {
			trans_mesh=close_mesh;
			if(trans_mesh) {
				trans_plane=cam_pos+mesh_dist*cam_dir;
				trans_start=mouse_pos;
			}
		}
		if(translate_action.bHeld) handleTransActionUpdate();
		if(translate_action.bReleased) trans_mesh=nullptr;

		const auto rotate_action=GetKey(olc::Key::R);
		if(rotate_action.bPressed) {
			rot_mesh=close_mesh;
			if(rot_mesh) rot_start=mouse_pos;
		}
		if(rotate_action.bHeld) handleRotActionUpdate();
		if(rotate_action.bReleased) rot_mesh=nullptr;

		const auto scale_action=GetKey(olc::Key::E);
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
		if(scale_action.bHeld) handleScaleActionUpdate();
		if(scale_action.bReleased) scale_mesh=nullptr;

		//set light pos
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//debug toggles
		if(GetKey(olc::Key::B).bPressed) realize_bounds^=true;
		if(GetKey(olc::Key::H).bPressed) help_menu^=true;

		return true;
	}

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});

		//combine all meshes triangles
		for(const auto& m:meshes) {
			tris_to_project.insert(tris_to_project.end(),
				m.tris.begin(), m.tris.end()
			);
		}

		//add bound lines
		if(realize_bounds) {
			for(const auto& m:meshes) {
				addAABB(m.getAABB(), olc::WHITE);
			}
		}

		return true;
	}

#pragma region RENDER HELPERS
	void renderSelectedMeshEdges() {
		for(int k=0; k<3; k++) {
			Mesh* select=nullptr;
			olc::Pixel col;
			switch(k) {
				case 0: select=trans_mesh, col=olc::ORANGE; break;
				case 1: select=rot_mesh, col=olc::RED; break;
				case 2: select=scale_mesh, col=olc::YELLOW; break;
			}
			if(!select) continue;
			
			int id=select->id;
			for(int i=1; i<ScreenWidth()-1; i++) {
				for(int j=1; j<ScreenHeight()-1; j++) {
					bool curr=id_buffer[bufferIX(i, j)]==id;
					bool lft=id_buffer[bufferIX(i-1, j)]==id;
					bool rgt=id_buffer[bufferIX(i+1, j)]==id;
					bool top=id_buffer[bufferIX(i, j-1)]==id;
					bool btm=id_buffer[bufferIX(i, j+1)]==id;
					if(curr!=lft||curr!=rgt||curr!=top||curr!=btm) {
						Draw(i, j, col);
					}
				}
			}
		}
	}

	void renderTransMeshHandle() {
		if(!trans_mesh) return;

		//line of action
		DrawLine(trans_start, mouse_pos, olc::BLUE);

		//crosshair
		DrawLine(trans_start.x-8, trans_start.y, trans_start.x+8, trans_start.y, olc::ORANGE);
		DrawLine(trans_start.x, trans_start.y-8, trans_start.x, trans_start.y+8, olc::ORANGE);
	}

	void renderRotMeshHandle() {
		if(!rot_mesh) return;

		//fun looking triangle?
		vf2d sub=mouse_pos-rot_start;
		float dist=sub.mag();
		float rad=std::max(20.f, dist);
		float angle=std::atan2(sub.y, sub.x);
		vf2d a=rot_start+cmn::polar<vf2d>(rad, angle);
		vf2d b=rot_start+cmn::polar<vf2d>(rad, angle+.667f*cmn::Pi);
		vf2d c=rot_start+cmn::polar<vf2d>(rad, angle+1.33f*cmn::Pi);
		DrawLine(a, b, olc::GREEN);
		DrawLine(b, c, olc::GREEN);
		DrawLine(c, a, olc::GREEN);
		
		//crosshair
		DrawLine(rot_start.x-8, rot_start.y, rot_start.x+8, rot_start.y, olc::RED);
		DrawLine(rot_start.x, rot_start.y-8, rot_start.x, rot_start.y+8, olc::RED);
	}

	void renderScaleMeshHandle() {
		if(!scale_mesh) return;

		//cool circles
		float m2c=(mouse_pos-scale_ctr).mag();
		DrawCircle(scale_ctr, m2c, olc::PURPLE);
		float s2c=(scale_start-scale_ctr).mag();
		DrawCircle(scale_ctr, s2c, olc::PURPLE);

		//crosshair
		DrawLine(scale_ctr.x-8, scale_ctr.y, scale_ctr.x+8, scale_ctr.y, olc::YELLOW);
		DrawLine(scale_ctr.x, scale_ctr.y-8, scale_ctr.x, scale_ctr.y+8, olc::YELLOW);
	}

	void renderHelpHints() {
		int cx=ScreenWidth()/2;
		if(help_menu) {
			DrawString(8, 8, "Movement Controls");
			DrawString(8, 16, "WASD, Space, & Shift to move");
			DrawString(8, 24, "ARROWS to look around");

			DrawString(ScreenWidth()-8*17, 8, "General Controls");
			DrawString(ScreenWidth()-8*16, 16, "T for translate", trans_mesh?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*13, 24, "R for rotate", rot_mesh?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*13, 32, "E for extent", scale_mesh?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*13, 40, "B for bounds", realize_bounds?olc::WHITE:olc::RED);

			DrawString(cx-4*18, ScreenHeight()-8, "[Press H to close]");
		} else {
			DrawString(cx-4*18, ScreenHeight()-8, "[Press H for help]");
		}
	}
#pragma endregion

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

		renderSelectedMeshEdges();

		renderTransMeshHandle();

		renderRotMeshHandle();

		renderScaleMeshHandle();

		renderHelpHints();

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(540, 360, 1, 1, false, true)) e.Start();

	return 0;
}