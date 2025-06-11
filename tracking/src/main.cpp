#include "common/3d/engine_3d.h"
namespace olc {
	static const Pixel PURPLE(144, 0, 255);
}
using olc::vf2d;
using cmn::vf3d;
using cmn::Mat4;

#include "common/utils.h"

#include "mesh.h"

#include "camera.h"

struct TrackingUI : cmn::Engine3D {
	TrackingUI() {
		sAppName="3D Tracking Demo";
	}

	//camera positioning
	float cam_yaw=-1.83f;
	float cam_pitch=-0.37f;

	//scene stuff
	Mesh model;
	const cmn::AABB3 bounds{vf3d(-6, -3, -6), vf3d(6, 3, 6)};

	//graphics stuff
	std::vector<Camera> cams;
	olc::Sprite* render_spr=nullptr;

	//debug toggles
	bool to_spin=true;
	bool show_cams=false;

	//tracking info
	float track_timer=0;
	std::vector<cmn::Line> rays;
	bool object_moving=false;
	vf3d object_pos;

	void updateCam(Camera& c, const std::vector<cmn::Triangle> tris) {
		//target tex & save pos
		vf3d old_cam_pos=cam_pos;
		vf3d old_cam_dir=cam_dir;
		SetDrawTarget(render_spr);

		//clip
		cam_pos=c.pos;
		cam_dir=c.dir;
		tris_to_project=tris;
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
		c.updateSpr(render_spr);

		//reset target & pos
		cam_pos=old_cam_pos;
		cam_dir=old_cam_dir;
		SetDrawTarget(nullptr);
	}

	bool user_create() override {
		cam_pos={2.95f, 2.55f, 10.73f};
		light_pos={0, 10, 0};

		//try load model
		try {
			model=Mesh::loadFromOBJ("assets/car.txt");
			model.scale={.4f, .4f, .4f};
			model.updateTransforms();
			model.updateTriangles();
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		//add cameras at corners and point towards center
		{
			const vf3d c[8]{
				{bounds.min.x, bounds.min.y, bounds.min.z},
				{bounds.max.x, bounds.min.y, bounds.min.z},
				{bounds.min.x, bounds.max.y, bounds.min.z},
				{bounds.max.x, bounds.max.y, bounds.min.z},
				{bounds.min.x, bounds.min.y, bounds.max.z},
				{bounds.max.x, bounds.min.y, bounds.max.z},
				{bounds.min.x, bounds.max.y, bounds.max.z},
				{bounds.max.x, bounds.max.y, bounds.max.z}
			};
			vf3d ctr=bounds.getCenter();
			for(int i=0; i<8; i++) {
				cams.emplace_back(360, 270, c[i], (ctr-c[i]).norm());
			}
		}

		//setup render texture
		render_spr=new olc::Sprite(ScreenWidth(), ScreenHeight());

		//simulate camera calibration
		for(auto& c:cams) {
			//get basis vectors
			vf3d up(0, 1, 0);
			vf3d rgt=c.dir.cross(up).norm();
			up=rgt.cross(c.dir);

			//10cm square in front of camera
			const float c_w=.1f, c_h=.1f;
			//10-20cm away
			float d=cmn::random(.1f, .2f);

			//vertex positioning
			vf3d ctr=c.pos+d*c.dir;
			vf3d tl=ctr+c_w/2*rgt+c_h/2*up;
			vf3d bl=ctr-c_w/2*rgt+c_h/2*up;
			vf3d tr=ctr+c_w/2*rgt-c_h/2*up;
			vf3d br=ctr-c_w/2*rgt-c_h/2*up;
			updateCam(c, {{tl, br, tr}, {tl, bl, br}});

			//find rendered size
			int min_x=-1, min_y=-1;
			int max_x=-1, max_y=-1;
			for(int x=0; x<c.width; x++) {
				for(int y=0; y<c.height; y++) {
					olc::Pixel curr=c.curr_spr->GetPixel(x, y);
					float lum=(curr.r+curr.g+curr.b)/765.f;
					if(lum>.1f) {
						if(min_x<0||x<min_x) min_x=x;
						if(min_y<0||y<min_y) min_y=y;
						if(max_x<0||x>max_x) max_x=x;
						if(max_y<0||y>max_y) max_y=y;
					}
				}
			}

			if(min_x<0||min_y<0||max_x<0||max_y<0) {
				std::cout<<"  camera calibration failed\n";
				return false;
			}

			//calculate focal length
			c.focal_length.x=(max_x-min_x)*d/c_w;
			c.focal_length.y=(max_y-min_y)*d/c_h;
		}

		return true;
	}

	bool user_destroy() override {
		delete render_spr;

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
		if(GetKey(olc::Key::ENTER).bPressed) to_spin^=true;
		if(GetKey(olc::Key::C).bPressed) show_cams^=true;

		//move model
		{
			int dir=GetKey(olc::Key::CTRL).bHeld?-1:1;
			if(GetKey(olc::Key::X).bHeld) model.translation.x+=2*dir*dt;
			if(GetKey(olc::Key::Y).bHeld) model.translation.y+=2*dir*dt;
			if(GetKey(olc::Key::Z).bHeld) model.translation.z+=2*dir*dt;
		}
		
		//spin mesh about y axis
		if(to_spin) model.rotation=Quat::fromAxisAngle(vf3d(0, 1, 0), dt)*model.rotation;
		
		//update model
		model.updateTransforms();
		model.updateTriangles();
		model.colorNormals();

		//track every now and then
		if(track_timer<0) {
			track_timer=.5f;

			//update cameras
			for(auto& c:cams) {
				updateCam(c, model.tris);
			}

			//where in image are most pixels moving
			rays.clear();
			const vf3d rgb_weights(.299f, .587f, .114f);
			for(int i=0; i<cams.size(); i++) {
				const auto& c=cams[i];

				//luminance weighted average
				float x_sum=0, y_sum=0;
				float w_sum=0;
				for(int x=0; x<c.width; x++) {
					for(int y=0; y<c.height; y++) {
						olc::Pixel curr=c.curr_spr->GetPixel(x, y);
						vf3d curr01(curr.r/255.f, curr.r/255.f, curr.r/255.f);
						float curr_lum=curr01.dot(rgb_weights);
						olc::Pixel prev=c.prev_spr->GetPixel(x, y);
						vf3d prev01(prev.r/255.f, prev.r/255.f, prev.r/255.f);
						float prev_lum=prev01.dot(rgb_weights);
						float lum_diff=std::fabsf(curr_lum-prev_lum);
						x_sum+=lum_diff*x;
						y_sum+=lum_diff*y;
						w_sum+=lum_diff;
					}
				}
				if(w_sum<1e-6) continue;

				//find position of differences
				float x_avg=x_sum/w_sum;
				float y_avg=y_sum/w_sum;
				vf3d dir=c.getDir(x_avg, y_avg);
				const float len=3.2f;
				cmn::Line ray{c.pos, c.pos+len*dir};
				ray.col=olc::WHITE;
				rays.push_back(ray);
			}

			//find segment intersection
			object_moving=false;
			if(rays.size()>=2) {
				object_moving=true;
				 
				//sum of distance to lines
				auto cost=[&] (vf3d pos) {
					float sum=0;
					for(const auto& r:rays) {
						vf3d pa=pos-r.p[0], ba=r.p[1]-r.p[0];
						sum+=pa.cross(ba).mag()/ba.mag();
					}
					return sum;
				};

				//gradient descent
				const float h=.001f;
				const float rate=.1f;
				const int steps=50;
				vf3d ix(0, 0, 0);
				for(int i=0; i<steps; i++) {
					//finite difference method
					float curr=cost(ix);
					float ddx=(cost(ix+vf3d(h, 0, 0))-curr)/h;
					float ddy=(cost(ix+vf3d(0, h, 0))-curr)/h;
					float ddz=(cost(ix+vf3d(0, 0, h))-curr)/h;
					ix-=rate*vf3d(ddx, ddy, ddz);
				}

				//update position
				object_pos=ix;
			}
		}
		track_timer-=dt;

		return true;
	}

	bool user_geometry() override {
		//show model
		tris_to_project=model.tris;

		//show camera frustums?
		for(const auto& c:cams) {
			//camera sizing
			float w=.75f, h=w*c.height/c.width;
			float cam_fov_rad=cam_fov_deg/180*cmn::Pi;
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

		//show camera renders as billboards
		if(show_cams) {
			for(int i=0; i<cams.size(); i++) {
				const auto& c=cams[i];

				//get basis vectors
				vf3d norm=(c.pos-cam_pos).norm();
				vf3d up(0, 1, 0);
				vf3d rgt=norm.cross(up).norm();
				up=rgt.cross(norm);

				//vertex positioning
				float w=1, h=w*c.height/c.width;
				float d=1;
				vf3d ctr=c.pos-d*c.dir;
				vf3d tl=ctr+w/2*rgt+h/2*up;
				vf3d tr=ctr+w/2*rgt-h/2*up;
				vf3d bl=ctr-w/2*rgt+h/2*up;
				vf3d br=ctr-w/2*rgt-h/2*up;

				//texture coords
				cmn::v2d tl_t{1, 0};
				cmn::v2d tr_t{1, 1};
				cmn::v2d bl_t{0, 0};
				cmn::v2d br_t{0, 1};

				//tessellation
				cmn::Triangle f1{tl, br, tr, tl_t, br_t, tr_t};
				cmn::Triangle f2{tl, bl, br, tl_t, bl_t, br_t};
				//reference camera
				f1.id=i;
				f2.id=i;
				tris_to_project.push_back(f1);
				tris_to_project.push_back(f2);
			}
		}

		//show bounds
		addAABB(bounds, olc::BLACK);

		//show rays
		for(const auto& r:rays) {
			lines_to_project.push_back(r);
		}

		//show intersection axis
		const float size=1.7f;
		vf3d x_diff(size/2, 0, 0);
		vf3d y_diff(0, size/2, 0);
		vf3d z_diff(0, 0, size/2);
		cmn::Line ix_x{object_pos-x_diff, object_pos+x_diff};
		cmn::Line ix_y{object_pos-y_diff, object_pos+y_diff};
		cmn::Line ix_z{object_pos-z_diff, object_pos+z_diff};
		if(object_moving) {
			ix_x.col=olc::RED;
			ix_y.col=olc::BLUE;
			ix_z.col=olc::GREEN;
		}
		lines_to_project.push_back(ix_x);
		lines_to_project.push_back(ix_y);
		lines_to_project.push_back(ix_z);

		return true;
	}

	bool user_render() override {
		Clear(olc::Pixel(51, 51, 51));

		resetBuffers();

		for(const auto& t:tris_to_draw) {
			//texture triangles if id is valid
			if(t.id==-1) FillDepthTriangle(
					t.p[0].x, t.p[0].y, t.t[0].w,
					t.p[1].x, t.p[1].y, t.t[1].w,
					t.p[2].x, t.p[2].y, t.t[2].w,
					t.col, t.id
				);
			else FillTexturedDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
				cams[t.id].curr_spr, t.col, t.id
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
	TrackingUI tui;
	if(tui.Construct(640, 480, 1, 1, false, true)) tui.Start();

	return 0;
}