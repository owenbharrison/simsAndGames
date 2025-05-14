#include "common/3d/engine_3d.h"
using olc::vf2d;

#include "mesh.h"
#include "common/utils.h"

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

	Mesh* trans_mesh=nullptr;
	vf3d trans_plane;
	vf2d trans_start;

	Mesh* rot_mesh=nullptr;
	vf2d rot_start;

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
		try{
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
		if(invVP_avail){
			float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
			float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
			vf3d clip(ndc_x, ndc_y, 1);
			vf3d world=clip*invVP;
			world/=world.w;

			mouse_dir=(world-cam_pos).norm();
		}

		const auto translate_action=GetMouse(olc::Mouse::LEFT);
		if(translate_action.bPressed) {
			//get closest mesh
			float record=-1;
			trans_mesh=nullptr;
			for(auto& m:meshes) {
				float dist=m.intersectRay(cam_pos, mouse_dir);
				if(dist>0) {
					if(record<0||dist<record) {
						record=dist;
						trans_mesh=&m;
					}
				}
			}
			if(trans_mesh) {
				trans_plane=cam_pos+record*cam_dir;
				trans_start=mouse_pos;
			}
		}
		if(translate_action.bReleased) {
			trans_mesh=nullptr;
		}

		const auto rotate_action=GetMouse(olc::Mouse::RIGHT);
		if(rotate_action.bPressed) {
			//get closest mesh
			float record=-1;
			rot_mesh=nullptr;
			for(auto& m:meshes) {
				float dist=m.intersectRay(cam_pos, mouse_dir);
				if(dist>0) {
					if(record<0||dist<record) {
						record=dist;
						rot_mesh=&m;
					}
				}
			}
			if(rot_mesh) {
				rot_start=mouse_pos;
			}
		}
		if(rotate_action.bReleased) {
			rot_mesh=nullptr;
		}

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
		if(rot_mesh&&invVP_avail) {
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

		return true;
	}

	float thing_timer=0;

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
		Line l1{v0, v1}; l1.col=col;
		lines_to_project.push_back(l1);
		Line l2{v1, v3}; l2.col=col;
		lines_to_project.push_back(l2);
		Line l3{v3, v2}; l3.col=col;
		lines_to_project.push_back(l3);
		Line l4{v2, v0}; l4.col=col;
		lines_to_project.push_back(l4);
		//sides
		Line l5{v0, v4}; l5.col=col;
		lines_to_project.push_back(l5);
		Line l6{v1, v5}; l6.col=col;
		lines_to_project.push_back(l6);
		Line l7{v2, v6}; l7.col=col;
		lines_to_project.push_back(l7);
		Line l8{v3, v7}; l8.col=col;
		lines_to_project.push_back(l8);
		//top
		Line l9{v4, v5}; l9.col=col;
		lines_to_project.push_back(l9);
		Line l10{v5, v7}; l10.col=col;
		lines_to_project.push_back(l10);
		Line l11{v7, v6}; l11.col=col;
		lines_to_project.push_back(l11);
		Line l12{v6, v4}; l12.col=col;
		lines_to_project.push_back(l12);
	}

	bool user_geometry() override {
		//combine all meshes triangles
		for(const auto& m:meshes){
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

		//rot mesh edge detection
		if(rot_mesh) {
			int id=rot_mesh->id;
			for(int i=1; i<ScreenWidth()-1; i++) {
				for(int j=1; j<ScreenHeight()-1; j++) {
					bool curr=id_buffer[i+ScreenWidth()*j]==id;
					bool lft=id_buffer[i-1+ScreenWidth()*j]==id;
					bool rgt=id_buffer[i+1+ScreenWidth()*j]==id;
					bool top=id_buffer[i+ScreenWidth()*(j-1)]==id;
					bool btm=id_buffer[i+ScreenWidth()*(j+1)]==id;
					if(curr!=lft||curr!=rgt||curr!=top||curr!=btm) {
						Draw(i, j, olc::BLACK);
					}
				}
			}
		}

		//trans mesh edge detection
		if(trans_mesh) {
			int id=trans_mesh->id;
			for(int i=1; i<ScreenWidth()-1; i++) {
				for(int j=1; j<ScreenHeight()-1; j++) {
					bool curr=id_buffer[i+ScreenWidth()*j]==id;
					bool lft=id_buffer[i-1+ScreenWidth()*j]==id;
					bool rgt=id_buffer[i+1+ScreenWidth()*j]==id;
					bool top=id_buffer[i+ScreenWidth()*(j-1)]==id;
					bool btm=id_buffer[i+ScreenWidth()*(j+1)]==id;
					if(curr!=lft||curr!=rgt||curr!=top||curr!=btm) {
						Draw(i, j, olc::BLACK);
					}
				}
			}
		}

		//draw translation handle
		if(trans_mesh) {
			DrawLine(trans_start, mouse_pos, olc::BLUE);
			DrawLine(trans_start.x-8, trans_start.y, trans_start.x+8, trans_start.y, olc::GREEN);
			DrawLine(trans_start.x, trans_start.y-8, trans_start.x, trans_start.y+8, olc::GREEN);
		}

		//draw rotation handle
		if(rot_mesh) {
			float dist=(mouse_pos-rot_start).mag();
			float rad=std::max(20.f, dist);
			DrawCircle(rot_start, rad, olc::YELLOW);
			DrawLine(rot_start.x-8, rot_start.y, rot_start.x+8, rot_start.y, olc::RED);
			DrawLine(rot_start.x, rot_start.y-8, rot_start.x, rot_start.y+8, olc::RED);
		}

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(540, 360, 1, 1, false, true)) e.Start();

	return 0;
}