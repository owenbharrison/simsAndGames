#define SOKOL_IMPL
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/include/sokol_app.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "sokol/sokol_engine.h"

#include "sokol/include/sokol_gl.h"

//for time
#include <ctime>

#include "cmn/math/mat4.h"

#include "phys/shape.h"

#include "imgui/include/imgui_singleheader.h"
#include "sokol/include/sokol_imgui.h"

cmn::vf3d rayIntersectPlane(
	const cmn::vf3d& orig, const cmn::vf3d& dir,
	const cmn::vf3d& ctr, const cmn::vf3d& norm
) {
	float dist=dot(norm, ctr-orig)/dot(norm, dir);
	return orig+dist*dir;
}

using cmn::vf3d;
using cmn::mat4;
using cmn::AABBf3;

class Phys3D : public cmn::SokolEngine {
	//scene
	std::list<Shape> shapes;

	vf3d gravity{0, -9.8f, 0};

	AABBf3 bounds{{-15, 0, -15}, {15, 10, 15}};

	const float time_step=1/60.f;
	float update_timer=0;
	bool update_phys=false;

	//user input
	struct {
		vf3d pos{3, 4, 3};
		float yaw=0, pitch=0;
		vf3d dir;

		float fov_deg=90;

		mat4 proj;
		mat4 view;
	} cam;

	float prev_mouse_x=0;
	float prev_mouse_y=0;

	vf3d mouse_dir;

	float select_rad=5;

	Particle* grab_ptc=nullptr;
	vf3d grab_ctr;
	vf3d grab_norm;
	vf3d grab_pt;

	//graphics
	sgl_pipeline pip{};

	bool show_particles=false;
	bool show_constraints=false;
	bool show_triangles=true;
	bool show_bounds=false;

public:
	void setupScene() {
		//cyan cylinder
		shapes.push_back(Shape::makeCylinder(
			vf3d(-2, 0, -2),
			vf3d(-2, 2, -2),
			1, 16, .01f, 1,
			0, 1, 1
		));
		//white prism
		shapes.push_back(Shape::makePrism(
			vf3d(2, 1, -2),
			vf3d(2, 2, 2),
			.01f, 1,
			1, 1, 1
		));
		//magenta torus
		shapes.push_back(Shape::makeTorus(
			vf3d(-2, 1, 2),
			1, 8,
			.6f, 8,
			.4f, 6,
			1, 0, 1
		));
		//yellow cone
		shapes.push_back(Shape::makeCone(
			vf3d(2, 0, 2),
			vf3d(2, 2, 2),
			1, 12,
			.01f, 1,
			1, 1, 0
		));

		//randomize velocities
		for(auto& s:shapes) {
			for(auto& p:s.particles) {
				float speed=cmn::randFloat(.5f, 2.5f);
				vf3d dir=normalize(.5f-vf3d(
					cmn::randFloat(),
					cmn::randFloat(),
					cmn::randFloat()
				));
				p.pos_old=p.pos-time_step*speed*dir;
			}
		}
	}

	//look at origin
	void setupCamera() {
		vf3d ryp=vf3d::cartesian(-cam.pos);
		cam.yaw=ryp.y;
		cam.pitch=ryp.z;
	}

	void setupImGui() {
		simgui_desc_t simgui_desc{};
		simgui_desc.ini_filename="assets/imgui.ini";
		simgui_setup(simgui_desc);
	}

	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);
	}

	void setupPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.face_winding=SG_FACEWINDING_CCW;
		pip_desc.cull_mode=SG_CULLMODE_BACK;
		//with alpha blending
		pip_desc.colors[0].blend.enabled=true;
		pip_desc.colors[0].blend.src_factor_rgb=SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha=SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip=sgl_make_pipeline(pip_desc);
	}

	bool user_create() override {
		app_title="Phys3D";

		std::srand(std::time(0));

		setupScene();

		setupCamera();

		setupImGui();

		setupSGL();

		setupPipeline();

		return true;
	}

	void user_destroy() override {
		simgui_shutdown();
		sgl_shutdown();
	}

	void user_input(const sapp_event* e) override {
		simgui_handle_event(e);
	}

#pragma region UPDATE_HELPERS
	void handleCameraMovement(float dt) {
		//dont move while grabbing
		if(grab_ptc) return;

		//forward/backward
		vf3d fwd=normalize(vf3d(1, 0, 1)*cam.dir);
		if(GetKey(SAPP_KEYCODE_W).held) cam.pos+=8.f*dt*fwd;
		if(GetKey(SAPP_KEYCODE_S).held) cam.pos-=6.f*dt*fwd;

		//left/right
		vf3d rgt=normalize(cross(fwd, vf3d(0, 1, 0)));
		if(GetKey(SAPP_KEYCODE_A).held) cam.pos-=7.f*dt*rgt;
		if(GetKey(SAPP_KEYCODE_D).held) cam.pos+=7.f*dt*rgt;

		//up/down
		if(GetKey(SAPP_KEYCODE_SPACE).held) cam.pos.y+=6.f*dt;
		if(GetKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam.pos.y-=6.f*dt;
	}

	void handleCameraLooking(float dt) {
		//dont move while grabbing
		if(grab_ptc) return;

		//orbit
		float mouse_x=GetMouseX();
		float mouse_y=GetMouseY();
		if(GetMouse(SAPP_MOUSEBUTTON_RIGHT).held) {
			const float sens=.5f;
			cam.yaw-=sens*dt*(mouse_x-prev_mouse_x);
			cam.pitch+=sens*dt*(mouse_y-prev_mouse_y);
		}
		prev_mouse_x=mouse_x;
		prev_mouse_y=mouse_y;

		//up/down
		if(GetKey(SAPP_KEYCODE_UP).held) cam.pitch+=dt;
		if(GetKey(SAPP_KEYCODE_DOWN).held) cam.pitch-=dt;
		cam.pitch=cmn::clamp(cam.pitch, .001f-cmn::Pi/2, cmn::Pi/2-.001f);

		//left/right
		if(GetKey(SAPP_KEYCODE_LEFT).held) cam.yaw-=dt;
		if(GetKey(SAPP_KEYCODE_RIGHT).held) cam.yaw+=dt;
	}

	void handleGrabAction() {
		const auto grab_action=GetMouse(SAPP_MOUSEBUTTON_LEFT);
		if(grab_action.pressed) {
			grab_ptc=nullptr;

			//angular size of radius pixels
			float fov_rad=cam.fov_deg/180*cmn::Pi;
			float h_view=2*std::tan(.5f*fov_rad);
			float pix2wld=select_rad*h_view/sapp_heightf();

			float record;
			for(auto& s:shapes) {
				for(auto& p:s.particles) {
					//parallel & perp dist
					vf3d cp=p.pos-cam.pos;
					float l=std::abs(dot(mouse_dir, cp));
					float r=length(cross(mouse_dir, cp));

					float max_r=l*pix2wld;
					if(r<max_r) {
						if(!grab_ptc||r<record) {
							record=r;
							grab_ptc=&p;
						}
					}
				}
			}
			if(grab_ptc) {
				grab_ctr=grab_ptc->pos;
				grab_norm=cam.dir;
			}
		}
		if(grab_action.held) {
			if(grab_ptc) {
				grab_pt=rayIntersectPlane(
					cam.pos, mouse_dir,
					grab_ctr, grab_norm
				);
			}
		}
		if(grab_action.released) grab_ptc=nullptr;
	}

	void handleUserInput(float dt) {
		handleCameraMovement(dt);

		handleCameraLooking(dt);

		handleGrabAction();

		handleGrabAction();
	}

	void updateCamera() {
		cam.dir=vf3d::polar({1, cam.yaw, cam.pitch});

		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(cam.fov_deg, asp, .1f, 100);
	}

	void updateMouseRay() {
		mat4 view_proj=mat4::mul(cam.proj, cam.view);
		mat4 inv_vp=mat4::inverse(view_proj);

		//get ray thru screen mouse pos
		float mx01=GetMouseX()/sapp_widthf();
		float my01=GetMouseY()/sapp_heightf();
		float ndc_x=2*mx01-1;
		float ndc_y=1-2*my01;
		vf3d clip(ndc_x, ndc_y, 1);
		float w=1;
		vf3d world=matMulVec(inv_vp, clip, w);
		world/=w;

		mouse_dir=(world-cam.pos).norm();
	}

	void handlePhysics(float dt) {
		update_timer+=dt;

		while(update_timer>time_step) {
			if(grab_ptc) {
				//make & update temp spring
				Particle ptc(grab_pt, 0, 1);
				Spring spr(grab_ptc, &ptc, 1000, 0);
				spr.rest_len=0;
				spr.update();
			}

			for(auto& shp:shapes) {
				//integrate
				for(auto& p:shp.particles) {
					p.applyForce(p.mass*gravity);

					p.update(time_step);
				}

				//constrain
				for(int i=0; i<4; i++) {
					for(auto& c:shp.constraints) {
						c.update();
					}

					for(auto& p:shp.particles) {
						p.keepInside(bounds, time_step);
					}
				}
			}
			
			update_timer-=time_step;
		}
	}
#pragma endregion

	bool user_update(float dt) override {
		handleUserInput(dt);

		if(update_phys) handlePhysics(dt);

		updateCamera();

		updateMouseRay();

		return true;
	}

#pragma region RENDER_HELPERS
	//grid which blends into background
	//THERE IS DEFINITELY AN EASIER WAY TO DO THIS
	//THIS LESBEGUE INTEGRATION
	void renderGrid(
		float rad, float spacing,
		float r, float g, float b
	) {
		//x, z, alpha
		static vf3d* grid=nullptr;
		static int prev_num=0;

		//avoid reallocations
		int num=1+rad/spacing;
		if(prev_num!=num) {
			delete[] grid;
			grid=new vf3d[num*num];
		}
		prev_num=num;

		auto ix=[&] (int i, int k) { return i+num*k; };

		//make grid of points on xz plane
		float recip=1.f/(num-1);
		for(int i=0; i<num; i++) {
			for(int k=0; k<num; k++) {
				float x=rad*recip*i;
				float z=rad*recip*k;
				float d=std::sqrt(x*x+z*z);
				float a=d>rad?0:1-d/rad;
				grid[ix(i, k)]={x, z, a};
			}
		}

		sgl_begin_lines();

		//connect em up in each quadrant
		for(int i=0; i<num; i++) {
			for(int k=0; k<num; k++) {
				const auto& c=grid[ix(i, k)];

				if(i>0) {
					const auto& o=grid[ix(i-1, k)];
					sgl_v3f_c4f(c.x, 0, c.y, r, g, b, c.z);
					sgl_v3f_c4f(o.x, 0, o.y, r, g, b, o.z);
					sgl_v3f_c4f(-c.x, 0, c.y, r, g, b, c.z);
					sgl_v3f_c4f(-o.x, 0, o.y, r, g, b, o.z);
					sgl_v3f_c4f(c.x, 0, -c.y, r, g, b, c.z);
					sgl_v3f_c4f(o.x, 0, -o.y, r, g, b, o.z);
					sgl_v3f_c4f(-c.x, 0, -c.y, r, g, b, c.z);
					sgl_v3f_c4f(-o.x, 0, -o.y, r, g, b, o.z);
				}
				if(k>0) {
					const auto& o=grid[ix(i, k-1)];
					sgl_v3f_c4f(c.x, 0, c.y, r, g, b, c.z);
					sgl_v3f_c4f(o.x, 0, o.y, r, g, b, o.z);
					sgl_v3f_c4f(-c.x, 0, c.y, r, g, b, c.z);
					sgl_v3f_c4f(-o.x, 0, o.y, r, g, b, o.z);
					sgl_v3f_c4f(c.x, 0, -c.y, r, g, b, c.z);
					sgl_v3f_c4f(o.x, 0, -o.y, r, g, b, o.z);
					sgl_v3f_c4f(-c.x, 0, -c.y, r, g, b, c.z);
					sgl_v3f_c4f(-o.x, 0, -o.y, r, g, b, o.z);
				}
			}
		}

		sgl_end();
	}

	void renderAxes() {
		sgl_begin_lines();

		sgl_c3f(1, 0, 0);
		sgl_v3f(0, 0, 0), sgl_v3f(1, 0, 0);
		sgl_c3f(0, 1, 0);
		sgl_v3f(0, 0, 0), sgl_v3f(0, 1, 0);
		sgl_c3f(0, 0, 1);
		sgl_v3f(0, 0, 0), sgl_v3f(0, 0, 1);

		sgl_end();
	}

	void renderShapeParticles(const Shape& s, float sz) {
		sgl_begin_points();

		sgl_point_size(sz);

		sgl_c3f(s.r, s.g, s.b);

		for(const auto& p:s.particles) {
			sgl_v3f(p.pos.x, p.pos.y, p.pos.z);
		}

		sgl_end();
	}

	void renderShapeConstraints(const Shape& s) {
		sgl_begin_lines();

		sgl_c3f(s.r, s.g, s.b);

		for(const auto& c:s.constraints) {
			sgl_v3f(c.a->pos.x, c.a->pos.y, c.a->pos.z);
			sgl_v3f(c.b->pos.x, c.b->pos.y, c.b->pos.z);
		}

		sgl_end();
	}

	void renderShapeTriangles(const Shape& s) {
		sgl_begin_triangles();

		for(const auto& t:s.tris) {
			const auto& a=t.a->pos;
			const auto& b=t.b->pos;
			const auto& c=t.c->pos;

			//simple lighting
			vf3d norm=normalize(cross(b-a, c-a));
			vf3d ctr=(a+b+c)/3;
			vf3d light_dir=normalize(cam.pos-ctr);
			float dp=std::max(.5f, dot(light_dir, norm));
			sgl_c3f(dp*s.r, dp*s.g, dp*s.b);

			sgl_v3f(a.x, a.y, a.z);
			sgl_v3f(b.x, b.y, b.z);
			sgl_v3f(c.x, c.y, c.z);
		}

		sgl_end();
	}

	void renderGrabSpring(float r, float g, float b) {
		if(!grab_ptc) return;

		sgl_begin_lines();

		sgl_c3f(r, g, b);

		sgl_v3f(grab_pt.x, grab_pt.y, grab_pt.z);
		sgl_v3f(grab_ptc->pos.x, grab_ptc->pos.y, grab_ptc->pos.z);

		sgl_end();
	}

	void renderBox(
		const vf3d& min, const vf3d& max,
		float r, float g, float b, float a=1
	) {
		sgl_begin_lines();

		sgl_c4f(r, g, b, a);

		//xy-
		sgl_v3f(min.x, min.y, min.z), sgl_v3f(max.x, min.y, min.z);
		sgl_v3f(max.x, min.y, min.z), sgl_v3f(max.x, max.y, min.z);
		sgl_v3f(max.x, max.y, min.z), sgl_v3f(min.x, max.y, min.z);
		sgl_v3f(min.x, max.y, min.z), sgl_v3f(min.x, min.y, min.z);
		//thru z
		sgl_v3f(min.x, min.y, min.z), sgl_v3f(min.x, min.y, max.z);
		sgl_v3f(max.x, min.y, min.z), sgl_v3f(max.x, min.y, max.z);
		sgl_v3f(min.x, max.y, min.z), sgl_v3f(min.x, max.y, max.z);
		sgl_v3f(max.x, max.y, min.z), sgl_v3f(max.x, max.y, max.z);
		//xy+
		sgl_v3f(min.x, min.y, max.z), sgl_v3f(max.x, min.y, max.z);
		sgl_v3f(max.x, min.y, max.z), sgl_v3f(max.x, max.y, max.z);
		sgl_v3f(max.x, max.y, max.z), sgl_v3f(min.x, max.y, max.z);
		sgl_v3f(min.x, max.y, max.z), sgl_v3f(min.x, min.y, max.z);

		sgl_end();
	}

	void renderBounds(float a) {
		float r=1, g=0, b=0;
		if(update_phys) r=0, g=1, b=0;
		renderBox(bounds.min, bounds.max, r, g, b, a);
	}

	void renderImGui() {
		simgui_frame_desc_t simgui_frame_desc{};
		simgui_frame_desc.width=sapp_width();
		simgui_frame_desc.height=sapp_height();
		simgui_frame_desc.delta_time=sapp_frame_duration();
		simgui_frame_desc.dpi_scale=sapp_dpi_scale();
		simgui_new_frame(simgui_frame_desc);

		ImGui::Begin("options");
		ImGui::Checkbox("update physics", &update_phys);
		ImGui::End();

		ImGui::Begin("graphics");
		ImGui::Checkbox("show particles", &show_particles);
		ImGui::Checkbox("show constraints", &show_constraints);
		ImGui::Checkbox("show triangles", &show_triangles);
		ImGui::Checkbox("show bounds", &show_bounds);
		ImGui::End();

		ImGui::Begin("controls");
		ImGui::Text("W/A/S/D to move camera around");
		ImGui::Text("Shift/Space to move camera up/down");
		ImGui::Text("ARROWS|RMB to look around");
		ImGui::Text("LMB to grab vertexes");
		ImGui::End();

		simgui_render();
	}
#pragma endregion

	bool user_render() override {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		//white
		pass.action.colors[0].clear_value={.12f, .12f, .12f, .12f};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_projection();
		sgl_load_matrix(cam.proj.m);
		sgl_matrix_mode_modelview();
		sgl_load_matrix(cam.view.m);

		//grey
		renderGrid(10, 1, .5f, .5f, .5f);

		renderAxes();

		for(const auto& shp:shapes) {
			if(show_particles) renderShapeParticles(shp, 5);
			if(show_constraints) renderShapeConstraints(shp);
			if(show_triangles) renderShapeTriangles(shp);
			if(show_bounds) {
				auto box=shp.getAABB();
				renderBox(box.min, box.max, shp.r, shp.g, shp.b, .7f);
			}
		}

		renderGrabSpring(.7f, 0, 1);
		
		//render bounds
		{
			float r=1, g=0, b=0;
			if(update_phys) r=0, g=1, b=0;
			renderBox(bounds.min, bounds.max, r, g, b, .5f);
		}

		sgl_draw();

		renderImGui();

		sg_end_pass();

		sg_commit();

		return true;
	}
};