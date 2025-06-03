#include "common/3d/engine_3d.h"
namespace olc {
	static const Pixel PURPLE(144, 0, 255);
}
using cmn::vf3d;

constexpr float Pi=3.1415927f;

#include "mesh.h"

struct Camera {
	int width=0, height=0;
	vf3d pos, dir;
	olc::Sprite* curr_spr=nullptr, *prev_spr=nullptr;

	Camera() {}

	Camera(int w, int h, vf3d p, vf3d d) {
		width=w;
		height=h;
		pos=p;
		dir=d;
		curr_spr=new olc::Sprite(w, h);
		prev_spr=new olc::Sprite(w, h);
	}
};

struct Example : cmn::Engine3D {
	Example() {
		sAppName="tracking";
	}

	//camera positioning
	float cam_yaw=-Pi/2;
	float cam_pitch=0;

	//scene stuff
	Mesh mesh;
	const cmn::AABB3 bounds{vf3d(-3, -2, -3), vf3d(3, 2, 3)};

	std::list<Camera> cams;

	//graphics stuff
	olc::Sprite* render_spr=nullptr;

	bool to_spin=true;

	bool user_create() override {
		cam_pos={-1.75f, 0, 8};
		light_pos={0, 10, 0};

		//try load mesh
		try {
			mesh=Mesh::loadFromOBJ("assets/car.txt");
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		//add cameras at corners pointing toward center
		vf3d ctr=bounds.getCenter();
		vf3d vnn(bounds.min.x, bounds.min.y, bounds.min.z);
		cams.emplace_back(120, 90, vnn, (ctr-vnn).norm());
		vf3d vpn(bounds.max.x, bounds.min.y, bounds.min.z);
		cams.emplace_back(120, 90, vpn, (ctr-vpn).norm());
		vf3d vpp(bounds.max.x, bounds.min.y, bounds.max.z);
		cams.emplace_back(120, 90, vpp, (ctr-vpp).norm());
		vf3d vnp(bounds.min.x, bounds.min.y, bounds.max.z);
		cams.emplace_back(120, 90, vnp, (ctr-vnp).norm());

		//setup render texture
		render_spr=new olc::Sprite(ScreenWidth(), ScreenHeight());

		return true;
	}

	void renderCam(Camera& c) {
		//clip
		cam_pos=c.pos;
		cam_dir=c.dir;
		projectAndClip();

		//reset
		resetBuffers();
		for(int i=0; i<render_spr->width*render_spr->height; i++) {
			render_spr->pColData[i]=olc::BLACK;
		}

		//rasterize
		for(const auto& t:tris_to_draw) {
			FillDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].w,
				t.col, t.id
			);
		}

		//store current in previous
		std::swap(c.curr_spr, c.prev_spr);

		//update texture
		for(int i=0; i<c.curr_spr->width; i++) {
			for(int j=0; j<c.curr_spr->height; j++) {
				float u=float(i)/c.curr_spr->width;
				float v=float(j)/c.curr_spr->height;
				c.curr_spr->SetPixel(i, j, render_spr->Sample(u, v));
			}
		}
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

		if(to_spin) {
			//spin mesh about y axis
			mesh.rotation=Quat::fromAxisAngle(vf3d(0, 1, 0), dt)*mesh.rotation;
			mesh.updateTransforms();
			mesh.updateTriangles();
			mesh.colorNormals();
		}

		//target tex & save pos
		vf3d old_cam_pos=cam_pos;
		vf3d old_cam_dir=cam_dir;
		SetDrawTarget(render_spr);

		//render all cameras
		tris_to_project=mesh.tris;
		for(auto& c:cams) {
			renderCam(c);
		}

		//reset target & pos
		cam_pos=old_cam_pos;
		cam_dir=old_cam_dir;
		SetDrawTarget(nullptr);

		return true;
	}

	bool user_geometry() override {
		//add mesh
		tris_to_project=mesh.tris;
		
		//add cameras
		for(const auto& c:cams) {
			//camera sizing
			float w=.75f, h=w*c.height/c.width;
			float cam_fov_rad=cam_fov_deg/180*Pi;
			float d=w/2/std::tanf(cam_fov_rad/2);

			//camera positioning
			vf3d up(0, 1, 0);
			vf3d rgt=c.dir.cross(up).norm();
			up=rgt.cross(c.dir);

			//vertex positioning
			vf3d ctr=c.pos+d*c.dir;
			vf3d tl=ctr+w/2*rgt+h/2*up;
			vf3d tr=ctr+w/2*rgt-h/2*up;
			vf3d bl=ctr-w/2*rgt+h/2*up;
			vf3d br=ctr-w/2*rgt-h/2*up;

			//show camera wireframe
			cmn::Line l1{tl, tr}; l1.col=olc::PURPLE;
			lines_to_project.push_back(l1);
			cmn::Line l2{tr, br}; l2.col=olc::PURPLE;
			lines_to_project.push_back(l2);
			cmn::Line l3{br, bl}; l3.col=olc::PURPLE;
			lines_to_project.push_back(l3);
			cmn::Line l4{bl, tl}; l4.col=olc::PURPLE;
			lines_to_project.push_back(l4);
			cmn::Line l5{tl, c.pos}; l5.col=olc::PURPLE;
			lines_to_project.push_back(l5);
			cmn::Line l6{tr, c.pos}; l6.col=olc::PURPLE;
			lines_to_project.push_back(l6);
			cmn::Line l7{br, c.pos}; l7.col=olc::PURPLE;
			lines_to_project.push_back(l7);
			cmn::Line l8{bl, c.pos}; l8.col=olc::PURPLE;
			lines_to_project.push_back(l8);
		}

		//add bounds
		addAABB(bounds, olc::WHITE);

		return true;
	}

	bool user_render() override {
		Clear(olc::Pixel(51, 51, 51));

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

		//show cameras on side
		int n=0, y=0;
		for(const auto& c:cams) {
			FillRect(0, y, c.width-1, c.height-1, olc::BLACK);
			//show differences
			for(int i=0; i<c.width; i++) {
				for(int j=0; j<c.height; j++) {
					olc::Pixel curr=c.curr_spr->GetPixel(i, j);
					olc::Pixel prev=c.prev_spr->GetPixel(i, j);
					int dr=curr.r-prev.r;
					int dg=curr.g-prev.g;
					int db=curr.b-prev.b;
					int diff=dr*dr+dg*dg+db*db;
					if(diff>20) Draw(i, y+j, olc::WHITE);
				}
			}
			//show boundary and label
			DrawRect(0, y, c.width-1, c.height-1, olc::PURPLE);
			DrawString(0, y, std::to_string(n));
			n++;
			y+=c.height;
		}

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(480, 360, 1, 1, false, true)) e.Start();

	return 0;
}