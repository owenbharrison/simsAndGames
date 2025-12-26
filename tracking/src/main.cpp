#define OLC_PGE_APPLICATION
#include "olc/engine_3d.h"
using cmn::vf3d;
using cmn::mat4;

#include "cmn/utils.h"

#include "mesh.h"

#include "camera.h"

#include "following_camera.h"

static float perpDistToRay(const vf3d& pt, const vf3d& orig, const vf3d& dir) {
	return dir.cross(pt-orig).mag();
}

class TrackingUI : public cmn::Engine3D {
	//camera positioning
	float cam_yaw=-2.67f;
	float cam_pitch=-.44f;
	vf3d light_pos;

	//user input
	mat4 inv_vp;
	vf3d mouse_dir, prev_mouse_dir;

	//helpful movement handle
	bool use_gizmo=false;
	vf3d gizmo_drag_orig;
	const float gizmo_axis_sz=1.3f, gizmo_margin=.3f, gizmo_square_sz=.7f;
	enum struct GizmoMode {
		None,
		XAxis,
		YAxis,
		ZAxis,
		XYPlane,
		YZPlane,
		ZXPlane
	} gizmo_mode=GizmoMode::None;

	//scene stuff
	Mesh terrain, house, model, cctv;
	bool spin_model=true;

	bool realize_frustums=true;

	bool realize_renders=false;

	//graphics stuff
	olc::Sprite* render_spr=nullptr;

	std::vector<Camera*> cams;

	float track_timer=0;
	struct Ray { vf3d orig, dir; };
	std::vector<Ray> track_rays;
	bool trackee_moving=false;
	vf3d trackee_pos;

	bool help_menu=false;

#pragma region SETUP_HELPERS
	bool setupTerrainMesh() {
		//load terrain mesh
		if(!Mesh::loadFromOBJ(terrain, "assets/models/mountains.txt")) return false;

		//color terrain based on slope
		olc::Sprite* gradient_spr=new olc::Sprite("assets/img/grass_falloff.png");
		const vf3d up(0, 1, 0);
		for(auto& t:terrain.tris) {
			//get steepness
			float s=std::abs(up.dot(t.getNorm()));
			t.col=gradient_spr->Sample(s, 0);
		}
		delete gradient_spr;

		return true;
	}

	bool setupHouseMesh() {
		if(!Mesh::loadFromOBJ(house, "assets/models/house.txt")) return false;

		house.translation={0, 3, 0};
		house.scale={.8f, .8f, .8f};
		house.updateMatrix();
		house.updateTriangles(olc::Pixel(33, 174, 255));

		return true;
	}

	bool setupModelMesh() {
		if(!Mesh::loadFromOBJ(model, "assets/models/jeep.txt")) return false;

		model.translation={-2.4f, 2.2f, 6.5f};
		model.scale={.3f, .3f, .3f};
		handleModelMeshUpdate();

		return true;
	}

	bool setupCCTVMesh() {
		if(!Mesh::loadFromOBJ(cctv, "assets/models/cctv.txt")) return false;

		cctv.scale={2, 2, 2};
		cctv.updateTriangles();

		return true;
	}

	//pretty hardcoded for now
	void placeCameras() {
		cams.push_back(new FollowingCamera(
			320, 240,
			{.77f, 4.85f, 2.86f}, {0, 1, 0}, vf3d(-2, -1, 2).norm(),
			-cmn::Pi/4, cmn::Pi/4, -cmn::Pi/6, cmn::Pi/6
		));

		cams.push_back(new Camera(
			320, 240,
			{3.77f, 3.16f, 8.97f},
			{0, 1, 0}, vf3d(-2, 0, -1).norm()
		));

		cams.push_back(new Camera(
			320, 240,
			{-5.38f, 3.29f, 2.88f},
			{0, 1, 0}, vf3d(1, 0, 1).norm()
		));

		cams.push_back(new Camera(
			320, 240,
			{-5.21f, 2.92f, 9.98f},
			{0, 1, 0}, vf3d(1, 0, -2).norm()
		));
	}

	//simulate camera calibration by
	//  rendering billboard infront of cam
	//  determining object bounding box
	//  estimate focal length
	bool calibrateCamera(Camera& c) {
		//10cm square in front of camera
		const float c_w=.1f, c_h=.1f;
		//15-25cm away
		float d=cmn::randFloat(.15f, .25f);

		//vertex positioning
		vf3d ctr=c.pos+d*c.fwd;
		vf3d tl=ctr+c_w/2*c.rgt+c_h/2*c.up;
		vf3d bl=ctr-c_w/2*c.rgt+c_h/2*c.up;
		vf3d tr=ctr+c_w/2*c.rgt-c_h/2*c.up;
		vf3d br=ctr-c_w/2*c.rgt-c_h/2*c.up;

		//render billboard
		handleCameraCapture(c, {{tl, br, tr}, {tl, bl, br}});

		//find bounding box of rendered pixels
		int min_x=-1, min_y=-1;
		int max_x=-1, max_y=-1;
		for(int x=0; x<c.getWidth(); x++) {
			for(int y=0; y<c.getHeight(); y++) {
				olc::Pixel curr=c.curr_spr->GetPixel(x, y);
				float lum=(curr.r+curr.g+curr.b)/765.f;
				if(lum>.1f) {
					if(min_x==-1||x<min_x) min_x=x;
					if(min_y==-1||y<min_y) min_y=y;
					if(max_x==-1||x>max_x) max_x=x;
					if(max_y==-1||y>max_y) max_y=y;
				}
			}
		}

		//nothing rendered
		if(min_x==-1||min_y==-1||max_x==-1||max_y==-1) return false;

		//estimate focal length
		c.focal_length_x=(max_x-min_x)*d/c_w;
		c.focal_length_y=(max_y-min_y)*d/c_h;

		return true;
	}
#pragma endregion

	bool user_create() override {
		std::srand(std::time(0));

		cam_pos={2.49f, 5.12f, 12.2f};
		light_pos={3, 23, 11};

		if(!setupTerrainMesh()) {
			std::cout<<"  unable to load terrain\n";

			return false;
		}

		if(!setupHouseMesh()) {
			std::cout<<"  unable to load house\n";

			return false;
		}

		if(!setupModelMesh()) {
			std::cout<<"  unable to load model\n";

			return false;
		}

		if(!setupCCTVMesh()) {
			std::cout<<"  unable to load cctv\n";

			return false;
		}

		//setup render texture
		render_spr=new olc::Sprite(ScreenWidth(), ScreenHeight());

		placeCameras();

		//get focal length of each camera
		for(auto& c:cams) {
			if(!calibrateCamera(*c)) {
				std::cout<<"unable to calibrate camera\n";

				return false;
			}
		}

		return true;
	}

	bool user_destroy() override {
		delete render_spr;
		render_spr=nullptr;

		for(auto& c:cams) {
			delete c;
			c=nullptr;
		}

		return true;
	}

#pragma region UPDATE_HELPERS
	void handleCameraCapture(Camera& c, const std::vector<cmn::Triangle> tris) {
		//target tex & save pos
		vf3d old_cam_pos=cam_pos;
		vf3d old_cam_dir=cam_dir;
		SetDrawTarget(render_spr);

		//mini pipeline
		cam_pos=c.pos;
		cam_dir=c.fwd;
		tris_to_project=tris;
		projectAndClip();

		//reset
		resetBuffers();
		Clear(olc::BLACK);

		//rasterize
		for(const auto& t:tris_to_draw) {
			FillDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].z,
				t.p[1].x, t.p[1].y, t.t[1].z,
				t.p[2].x, t.p[2].y, t.t[2].z,
				t.col, t.id
			);
		}

		//reset target & pos
		cam_pos=old_cam_pos;
		cam_dir=old_cam_dir;
		SetDrawTarget(nullptr);

		c.updateSprite(render_spr);
	}

	void handleCameraLooking(float dt) {
		//dont look while gizmoing
		if(gizmo_mode!=GizmoMode::None) return;

		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>cmn::Pi/2) cam_pitch=cmn::Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-cmn::Pi/2) cam_pitch=.001f-cmn::Pi/2;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw+=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw-=dt;
	}

	void handleCameraMovement(float dt) {
		//dont move while gizmoing
		if(gizmo_mode!=GizmoMode::None) return;

		//move up, down
		if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*dt;
		if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::sin(cam_yaw), 0, std::cos(cam_yaw));
		if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*dt*fb_dir;
		if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*dt*lr_dir;
		if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*dt*lr_dir;
	}

	void handleMouseRays() {
		prev_mouse_dir=mouse_dir;

		//unproject mouse point
		float ndc_x=2.f*GetMouseX()/ScreenWidth()-1;
		float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
		vf3d clip(ndc_x, ndc_y, 1);
		float w=1;
		vf3d world=matMulVec(inv_vp, clip, w);
		world/=w;

		mouse_dir=(world-cam_pos).norm();
	}

	void handleGizmoDragBegin() {
		const auto& g=model.translation;
		const auto& a=gizmo_axis_sz;
		const auto& m=gizmo_margin;
		const auto& s=gizmo_square_sz;

		handleGizmoDragEnd();
		gizmo_drag_orig=g;

		//is mouse over axis ends?
		int n=0;
		if(perpDistToRay(g+vf3d(a, 0, 0), cam_pos, mouse_dir)<m) gizmo_mode=GizmoMode::XAxis;
		if(perpDistToRay(g+vf3d(0, a, 0), cam_pos, mouse_dir)<m) gizmo_mode=GizmoMode::YAxis;
		if(perpDistToRay(g+vf3d(0, 0, a), cam_pos, mouse_dir)<m) gizmo_mode=GizmoMode::ZAxis;

		//is mouse over squares?
		vf3d rel=g-cam_pos;
		float u, v;
		cmn::segIntersectTri({0, 0, 0}, mouse_dir, rel+vf3d(m, m, 0), rel+vf3d(m+s, m, 0), rel+vf3d(m, m+s, 0), &u, &v);
		if(u>0&&v>0&&u<1&&v<1) gizmo_mode=GizmoMode::XYPlane;
		cmn::segIntersectTri({0, 0, 0}, mouse_dir, rel+vf3d(0, m, m), rel+vf3d(0, m+s, m), rel+vf3d(0, m, m+s), &u, &v);
		if(u>0&&v>0&&u<1&&v<1) gizmo_mode=GizmoMode::YZPlane;
		cmn::segIntersectTri({0, 0, 0}, mouse_dir, rel+vf3d(m, 0, m), rel+vf3d(m, 0, m+s), rel+vf3d(m+s, 0, m), &u, &v);
		if(u>0&&v>0&&u<1&&v<1) gizmo_mode=GizmoMode::ZXPlane;
	}

	void handleGizmoDragUpdate() {
		//which plane/axis to constrain motion to?
		vf3d axis, norm;
		bool constrain=false;
		switch(gizmo_mode) {
			default: return;
			case GizmoMode::XAxis: constrain=true, axis={1, 0, 0}, norm={0, 1, 0}; break;
			case GizmoMode::YAxis: constrain=true, axis={0, 1, 0}, norm={0, 0, 1}; break;
			case GizmoMode::ZAxis: constrain=true, axis={0, 0, 1}, norm={1, 0, 0}; break;
			case GizmoMode::XYPlane: norm={0, 0, 1}; break;
			case GizmoMode::YZPlane: norm={1, 0, 0}; break;
			case GizmoMode::ZXPlane: norm={0, 1, 0}; break;
		}

		auto& g=model.translation;

		//intersect prev & curr mouse rays to plane
		vf3d curr_pt=cmn::segIntersectPlane(cam_pos, cam_pos+mouse_dir, gizmo_drag_orig, norm);
		vf3d prev_pt=cmn::segIntersectPlane(cam_pos, cam_pos+prev_mouse_dir, gizmo_drag_orig, norm);

		//move by delta
		vf3d delta=curr_pt-prev_pt;

		if(constrain) g+=axis.dot(delta)*axis;
		else g+=delta;
	}

	void handleGizmoDragEnd() {
		gizmo_mode=GizmoMode::None;
	}

	void handleGizmoUpdate() {
		if(GetKey(olc::Key::G).bPressed) use_gizmo^=true;

		if(!use_gizmo) return;

		const auto grab_action=GetMouse(olc::Mouse::LEFT);
		if(grab_action.bPressed) handleGizmoDragBegin();
		if(grab_action.bHeld) handleGizmoDragUpdate();
		if(grab_action.bReleased) handleGizmoDragEnd();
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		//polar to cartesian
		cam_dir=polar3D(cam_yaw, cam_pitch);

		handleCameraMovement(dt);

		//update unprojection matrix
		//  using prev cam transforms, but its fine.
		inv_vp=mat4::inverse(mat4::mul(cam_proj, cam_view));

		handleMouseRays();

		handleGizmoUpdate();

		//set light pos
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//toggles
		if(GetKey(olc::Key::ENTER).bPressed) spin_model^=true;
		if(GetKey(olc::Key::F).bPressed) realize_frustums^=true;
		if(GetKey(olc::Key::R).bPressed) realize_renders^=true;
		if(GetKey(olc::Key::H).bPressed) help_menu^=true;
	}

	void handleModelMeshUpdate() {
		model.updateMatrix();
		model.updateTriangles();
		model.colorNormals();
	}

	void handleFollowing(FollowingCamera& c, float dt) {
		//dont bother if nothing is moving
		if(!c.guess_valid) return;

		//"normalized" space [-1, 1]
		float x=2*c.guess_x/c.getWidth()-1;
		float y=2*c.guess_y/c.getHeight()-1;

		//turn towards movement
		//  w/ speed falloff of
		//  deadzone=0, edge=1
		const float dead_zone=.25f;
		float spd_x=(std::abs(x)-dead_zone)/(1-dead_zone);
		if(x<dead_zone) c.yaw-=spd_x*dt;
		if(x>dead_zone) c.yaw+=spd_x*dt;
		float spd_y=(std::abs(y)-dead_zone)/(1-dead_zone);
		if(y<dead_zone) c.pitch+=spd_y*dt;
		if(y>dead_zone) c.pitch-=spd_y*dt;
		c.clampAngles();

		c.updateBasisVectors();
	}

	void handleTracking(float dt) {
		//mini geometry step
		std::vector<cmn::Triangle> mini_tris;
		realizeTerrainMesh(mini_tris);
		realizeHouseMesh(mini_tris);
		realizeModelMesh(mini_tris);

		track_rays.clear();
		for(const auto& c:cams) {
			//capture & compute motion
			handleCameraCapture(*c, mini_tris);
			c->updateGuess();

			//update rays
			if(c->guess_valid) {
				const float len=3.2f;
				vf3d dir=c->getDir(c->guess_x, c->guess_y);
				track_rays.push_back({c->pos, dir});
			}
		}

		//triangulate movement by
		//  finding rays intersection
		//  using gradient descent
		trackee_moving=false;
		const int num_track_rays=track_rays.size();
		if(num_track_rays>=2) {
			trackee_moving=true;

			//sum of distance to lines
			auto cost=[&] (vf3d pos) {
				float sum=0;
				for(const auto& r:track_rays) {
					sum+=r.dir.cross(pos-r.orig).mag();
				}
				return sum;
			};

			//initial guess at average of camera positions
			vf3d cam_sum;
			for(const auto& r:track_rays) cam_sum+=r.orig;
			trackee_pos=cam_sum/num_track_rays;

			//gradient descent
			const float h=.001f;
			const float rate=.05f;
			const int steps=150;
			for(int i=0; i<steps; i++) {
				//finite difference method
				float curr=cost(trackee_pos);
				float ddx=(cost(trackee_pos+vf3d(h, 0, 0))-curr)/h;
				float ddy=(cost(trackee_pos+vf3d(0, h, 0))-curr)/h;
				float ddz=(cost(trackee_pos+vf3d(0, 0, h))-curr)/h;
				trackee_pos-=rate*vf3d(ddx, ddy, ddz);

				//stop if barely moving
				float mag=ddx*ddx+ddy*ddy+ddz*ddz;
				if(mag<.001f) break;
			}
		}

		for(auto& c:cams) {
			//for followers:
			if(auto fc=dynamic_cast<FollowingCamera*>(c)) {
				//update angles
				handleFollowing(*fc, dt);

				//rerender to avoid tracking self movement
				handleCameraCapture(*c, mini_tris);
				c->updateGuess();
			}

			//swap frames
			std::swap(c->curr_spr, c->prev_spr);
		}
	}
#pragma endregion

	bool user_update(float dt) override {
		//clamp delta time
		dt=std::min(std::max(dt, 1/165.f), 1/30.f);

		handleUserInput(dt);

		//spin model about y axis
		if(spin_model) model.rotation.y+=.67f*dt;
		handleModelMeshUpdate();

		//"track" every now and then
		if(track_timer<0) {
			const float track_time_step=1/3.f;
			track_timer+=track_time_step;

			handleTracking(track_time_step);
		}
		track_timer-=dt;

		return true;
	}

#pragma region GEOMETRY HELPERS
	//this looks nice :D
	void realizeArrow(const vf3d& a, const vf3d& b, float sz, const olc::Pixel& col) {
		vf3d ba=b-a;
		float mag=ba.mag();
		vf3d ca=cam_pos-a;
		//make tip of arrow "point" towards cam_pos
		vf3d perp=.5f*sz*mag*ba.cross(ca).norm();
		vf3d c=b-sz*ba;
		vf3d l=c-perp, r=c+perp;
		cmn::Line l1{a, c}; l1.col=col;
		lines_to_project.push_back(l1);
		cmn::Line l2{l, r}; l2.col=col;
		lines_to_project.push_back(l2);
		cmn::Line l3{l, b}; l3.col=col;
		lines_to_project.push_back(l3);
		cmn::Line l4{r, b}; l4.col=col;
		lines_to_project.push_back(l4);
	}

	//a & b form vector sides of parallelogram
	void realizeParallelogram(const vf3d& pos, const vf3d& a, const vf3d& b, const olc::Pixel& col) {
		//corners
		vf3d v1=pos+a;
		vf3d v2=pos+b;
		vf3d v3=pos+a+b;
		cmn::Triangle t1{pos, v1, v3}; t1.col=col;
		cmn::Triangle t2{pos, v2, v3}; t2.col=col;
		//add twice so as not to cull
		tris_to_project.push_back(t1);
		std::swap(t1.p[0], t1.p[1]);
		tris_to_project.push_back(t1);
		tris_to_project.push_back(t2);
		std::swap(t2.p[0], t2.p[1]);
		tris_to_project.push_back(t2);
	}

	void realizeAxes(const vf3d& pos, float sz) {
		vf3d dx(sz, 0, 0);
		vf3d dy(0, sz, 0);
		vf3d dz(0, 0, sz);
		cmn::Line lx{pos-dx, pos+dx}; lx.col=olc::RED;
		cmn::Line ly{pos-dy, pos+dy}; ly.col=olc::BLUE;
		cmn::Line lz{pos-dz, pos+dz}; lz.col=olc::GREEN;
		lines_to_project.push_back(lx);
		lines_to_project.push_back(ly);
		lines_to_project.push_back(lz);
	}

	void realizeGizmo() {
		if(!use_gizmo) return;

		const auto& g=model.translation;
		const auto& a=gizmo_axis_sz;
		const auto& m=gizmo_margin;
		const auto& s=gizmo_square_sz;

		//xyz axes in red, blue, & green
		//color white if selected
		realizeArrow(
			g, g+vf3d(a, 0, 0), .2f,
			gizmo_mode==GizmoMode::XAxis?olc::WHITE:olc::RED
		);
		realizeArrow(
			g, g+vf3d(0, a, 0), .2f,
			gizmo_mode==GizmoMode::YAxis?olc::WHITE:olc::BLUE
		);
		realizeArrow(
			g, g+vf3d(0, 0, a), .2f,
			gizmo_mode==GizmoMode::ZAxis?olc::WHITE:olc::GREEN
		);

		//xy, yz, zx planes in magenta, cyan, & yellow
		//color white if selected
		realizeParallelogram(
			g+vf3d(m, m, 0),
			vf3d(s, 0, 0), vf3d(0, s, 0),
			gizmo_mode==GizmoMode::XYPlane?olc::WHITE:olc::MAGENTA
		);
		realizeParallelogram(
			g+vf3d(0, m, m),
			vf3d(0, s, 0), vf3d(0, 0, s),
			gizmo_mode==GizmoMode::YZPlane?olc::WHITE:olc::CYAN
		);
		realizeParallelogram(
			g+vf3d(m, 0, m),
			vf3d(0, 0, s), vf3d(s, 0, 0),
			gizmo_mode==GizmoMode::ZXPlane?olc::WHITE:olc::YELLOW
		);
	}

	void realizeTerrainMesh(std::vector<cmn::Triangle>& tris) {
		tris.insert(tris.end(),
			terrain.tris.begin(), terrain.tris.end()
		);
	}

	void realizeHouseMesh(std::vector<cmn::Triangle>& tris) {
		tris.insert(tris.end(),
			house.tris.begin(), house.tris.end()
		);
	}

	void realizeModelMesh(std::vector<cmn::Triangle>& tris) {
		tris.insert(tris.end(),
			model.tris.begin(), model.tris.end()
		);
	}

	void realizeCameraMeshes(const olc::Pixel& col) {
		for(const auto& c:cams) {
			cctv.translation=c->pos;
			cctv.updateMatrix(c->rgt, c->up, c->fwd);
			cctv.updateTriangles(col);
			tris_to_project.insert(tris_to_project.end(),
				cctv.tris.begin(), cctv.tris.end()
			);
		}
	}

	//show camera renders as textured billboards
	void realizeCameraRenders() {
		//texture coords 
		const vf3d tl_t(1, 0, 0);
		const vf3d tr_t(1, 1, 0);
		const vf3d bl_t(0, 0, 0);
		const vf3d br_t(0, 1, 0);
		for(int i=0; i<cams.size(); i++) {
			const auto& c=cams[i];

			//determine sizing
			float w=1, h=w*c->getHeight()/c->getWidth();
			float d=1;
			vf3d ctr=c->pos-d*c->fwd;

			//get basis vectors
			vf3d norm=(ctr-cam_pos).norm();
			vf3d up(0, 1, 0);
			vf3d rgt=norm.cross(up).norm();
			up=rgt.cross(norm);

			//vertex positioning
			vf3d tl=ctr+w/2*rgt+h/2*up;
			vf3d tr=ctr+w/2*rgt-h/2*up;
			vf3d bl=ctr-w/2*rgt+h/2*up;
			vf3d br=ctr-w/2*rgt-h/2*up;

			//tessellation
			cmn::Triangle f1{tl, br, tr, tl_t, br_t, tr_t};
			cmn::Triangle f2{tl, bl, br, tl_t, bl_t, br_t};

			//reference camera
			f1.id=i, f2.id=i;
			tris_to_project.push_back(f1);
			tris_to_project.push_back(f2);
		}
	}

	//show outline of camera view
	void realizeCameraFrustums(const olc::Pixel& col) {
		for(const auto& c:cams) {
			//camera sizing
			float d=.3f;
			float w=c->getWidth()*d/c->focal_length_x;
			float h=c->getHeight()*d/c->focal_length_y;

			//vertex positioning
			vf3d ctr=c->pos+d*c->fwd;
			vf3d tl=ctr+w/2*c->rgt+h/2*c->up;
			vf3d tr=ctr+w/2*c->rgt-h/2*c->up;
			vf3d bl=ctr-w/2*c->rgt+h/2*c->up;
			vf3d br=ctr-w/2*c->rgt-h/2*c->up;

			//show camera wireframe
			cmn::Line l1{tl, tr}; l1.col=col;
			lines_to_project.push_back(l1);
			cmn::Line l2{tr, br}; l2.col=col;
			lines_to_project.push_back(l2);
			cmn::Line l3{br, bl}; l3.col=col;
			lines_to_project.push_back(l3);
			cmn::Line l4{bl, tl}; l4.col=col;
			lines_to_project.push_back(l4);
			cmn::Line l5{tl, c->pos}; l5.col=col;
			lines_to_project.push_back(l5);
			cmn::Line l6{tr, c->pos}; l6.col=col;
			lines_to_project.push_back(l6);
			cmn::Line l7{br, c->pos}; l7.col=col;
			lines_to_project.push_back(l7);
			cmn::Line l8{bl, c->pos}; l8.col=col;
			lines_to_project.push_back(l8);
		}
	}

	void realizeTrackRays(const olc::Pixel& col) {
		const float len=3.2f;
		for(const auto& r:track_rays) {
			cmn::Line l{r.orig, r.orig+len*r.dir};
			l.col=col;
			lines_to_project.push_back(l);
		}
	}
#pragma endregion

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});

		realizeTerrainMesh(tris_to_project);

		realizeHouseMesh(tris_to_project);

		realizeGizmo();

		realizeModelMesh(tris_to_project);

		realizeCameraMeshes(olc::WHITE);

		if(realize_renders) realizeCameraRenders();

		//purple
		if(realize_frustums) realizeCameraFrustums(olc::Pixel(170, 0, 255));

		realizeTrackRays(olc::WHITE);

		//realize trackee pos
		if(trackee_moving) realizeAxes(trackee_pos, .9f);

		return true;
	}

	void renderHelpHints() {
		int cx=ScreenWidth()/2;
		if(help_menu) {
			DrawString(8, 8, "Movement Controls");
			DrawString(8, 16, "WASD, Shift, & Space to move");
			DrawString(8, 24, "ARROWS to look around");

			DrawString(ScreenWidth()-8*19, 8, "Toggleable Options");
			DrawString(ScreenWidth()-8*21, 16, "Enter for model spin", spin_model?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*18, 24, "G for model gizmo", use_gizmo?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*18, 32, "R for render view", realize_renders?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*19, 40, "F for frustum view", realize_frustums?olc::WHITE:olc::RED);

			DrawString(cx-4*18, ScreenHeight()-8, "[Press H to close]");
		} else {
			DrawString(cx-4*18, ScreenHeight()-8, "[Press H for help]");
		}
	}

	bool user_render() override {
		//grey background
		Clear(olc::BLACK);

		//render 3d stuff
		resetBuffers();

		for(const auto& t:tris_to_draw) {
			if(t.id==-1) {
				FillDepthTriangle(
					t.p[0].x, t.p[0].y, t.t[0].z,
					t.p[1].x, t.p[1].y, t.t[1].z,
					t.p[2].x, t.p[2].y, t.t[2].z,
					t.col, t.id
				);
			} else {
				FillTexturedDepthTriangle(
					t.p[0].x, t.p[0].y, t.t[0].x, t.t[0].y, t.t[0].z,
					t.p[1].x, t.p[1].y, t.t[1].x, t.t[1].y, t.t[1].z,
					t.p[2].x, t.p[2].y, t.t[2].x, t.t[2].y, t.t[2].z,
					cams[t.id]->curr_spr, t.col, t.id
				);
			}
		}

		for(const auto& l:lines_to_draw) {
			DrawDepthLine(
				l.p[0].x, l.p[0].y, l.t[0].z,
				l.p[1].x, l.p[1].y, l.t[1].z,
				l.col, l.id
			);
		}

		renderHelpHints();

		return true;
	}

public:
	TrackingUI() {
		sAppName="3D Tracking Demo";
	}
};

int main() {
	TrackingUI tui;
	bool fullscreen=false;
	bool vsync=true;
	if(tui.Construct(640, 480, 1, 1, fullscreen, vsync)) tui.Start();

	return 0;
}