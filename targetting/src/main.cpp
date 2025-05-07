#include "common/3d/engine_3d.h"
namespace cmn {
	static const olc::Pixel PURPLE(150, 0, 255);
}

#include "mesh.h"
#include "common/utils.h"

struct Example : cmn::Engine3D {
	Example() {
		sAppName="targetting system";
	}

	//camera positioning
	float cam_yaw=-cmn::Pi/2;
	float cam_pitch=-.1f;

	//scene stuff
	std::vector<Mesh> meshes;
	Mesh* selected=nullptr;

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
		cam_pos={0, 0, 3};
		
		//load monkey and bunny
		try{
			Mesh a=Mesh::loadFromOBJ("assets/suzanne.txt");
			a.translation={-2.5f, 0, 0};
			meshes.push_back(a);
			Mesh b=Mesh::loadFromOBJ("assets/dragon.txt");
			b.rotation={0, cmn::Pi, 0};
			b.scale={3, 3, 3};
			meshes.push_back(b);
			Mesh c=Mesh::loadFromOBJ("assets/bunny.txt");
			c.scale={10, 10, 10};
			c.translation={2.5f, -1, 0};
			meshes.push_back(c);
		} catch(const std::exception& e) {
			std::cout<<e.what()<<'\n';
			return false;
		}

		//update things
		for(auto& m:meshes) {
			m.updateMatrices();
			m.id=lowestUniqueID();
			m.updateTris();
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

		if(GetMouse(olc::Mouse::LEFT).bPressed) {
			try {
				//screen -> world with inverse matrix
				Mat4 invPV=Mat4::inverse(mat_view*mat_proj);
				float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
				float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
				vf3d clip(ndc_x, ndc_y, 1);
				vf3d world=clip*invPV;
				world/=world.w;

				vf3d dir=(world-cam_pos).norm();

				//select closest mesh
				float record;
				selected=nullptr;
				for(auto& m:meshes) {
					float dist=m.intersectRay(cam_pos, dir);
					if(dist>0) {
						if(!selected||dist<record) {
							record=dist;
							selected=&m;
						}
					}
				}
			} catch(const std::exception& e) {
				//matrix could be singular.
				std::cout<<"  "<<e.what()<<'\n';
			}
		}
		if(GetKey(olc::Key::ESCAPE).bPressed) selected=nullptr;

		//debug toggles
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;

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
				addAABB(m.getAABB(), olc::GREEN);
			}
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::Pixel(100, 100, 100));

		render3D();
		
		//edge detection with selected object
		if(selected) {
			int id=selected->id;
			for(int i=1; i<ScreenWidth()-1; i++) {
				for(int j=1; j<ScreenHeight()-1; j++) {
					bool curr=id_buffer[i+ScreenWidth()*j]==id;
					bool lft=id_buffer[i-1+ScreenWidth()*j]==id;
					bool rgt=id_buffer[i+1+ScreenWidth()*j]==id;
					bool top=id_buffer[i+ScreenWidth()*(j-1)]==id;
					bool btm=id_buffer[i+ScreenWidth()*(j+1)]==id;
					if(curr!=lft||curr!=rgt||curr!=top||curr!=btm) {
						Draw(i, j, olc::WHITE);
					}
				}
			}
		}

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(540, 360, 1, 1, false, true)) e.Start();

	return 0;
}