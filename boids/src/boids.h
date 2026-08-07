//@lalaoopybee m8d3y2026
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

#include <list>

#include "boid.h"

//for time
#include <ctime>

#include "cmn/utils.h"
#include "cmn/geom/aabb3.h"

#include "imgui/include/imgui_singleheader.h"
#include "sokol/include/sokol_imgui.h"

//makes some perpendicular vector
cmn::vf3d getPerp(const cmn::vf3d& x) {
	return
		std::abs(x.x)>std::abs(x.z)?
		cmn::vf3d(-x.y, x.x, 0):
		cmn::vf3d(0, -x.z, x.y);
}

using cmn::vf3d;

class Boids : public cmn::SokolEngine {
	//user input
	struct {
		vf3d pos{3, 2.5f, 3.5f};
		float yaw=0, pitch=0;
		vf3d dir;

		float fov_deg=90;
	} cam;

	cmn::AABBf3 bounds{{-2.5f, -2, -2}, {2.5f, 2, 2}};
	std::list<boid> boids;

	//graphics
	sgl_pipeline pip{};

	sg_sampler smp{};

public:
	void setupImGui() {
		simgui_desc_t simgui_desc{};
		simgui_desc.ini_filename="assets/imgui.ini";
		simgui_setup(simgui_desc);
	}

	void setupCamera() {
		vf3d ryp=vf3d::cartesian(-cam.pos);
		cam.yaw=ryp.y;
		cam.pitch=ryp.z;
	}

	void setupBoids() {
		int num=cmn::randInt(500, 1000);
		for(int i=0; i<num; i++) {
			vf3d pos01(
				cmn::randFloat(),
				cmn::randFloat(),
				cmn::randFloat()
			);
			vf3d pos=bounds.min+(bounds.max-bounds.min)*pos01;
			float speed=cmn::randFloat(boid::min_speed, boid::max_speed);
			vf3d dir=normalize(vf3d(
				.5f-cmn::randFloat(),
				.5f-cmn::randFloat(),
				.5f-cmn::randFloat()
			));
			boids.push_back({pos, speed*dir});
		}
	}

	//HUGE overestimate
	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_desc.max_commands=500000;
		sgl_desc.max_vertices=500000;
		sgl_setup(sgl_desc);
	}

	void setupPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.face_winding=SG_FACEWINDING_CCW;
		pip_desc.cull_mode=SG_CULLMODE_BACK;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		//with alpha blending
		pip_desc.colors[0].blend.enabled=true;
		pip_desc.colors[0].blend.src_factor_rgb=SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha=SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip=sgl_make_pipeline(pip_desc);
	}

	void setupSampler() {
		sg_sampler_desc smp_desc{};
		smp=sg_make_sampler(smp_desc);
	}

	bool user_create() override {
		app_title="[boids]";

		std::srand(std::time(0));

		setupImGui();

		setupCamera();

		setupBoids();

		setupSGL();

		setupPipeline();

		setupSampler();

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
		//up/down
		if(GetKey(SAPP_KEYCODE_UP).held) cam.pitch+=dt;
		if(GetKey(SAPP_KEYCODE_DOWN).held) cam.pitch-=dt;
		cam.pitch=cmn::clamp(cam.pitch, .001f-cmn::Pi/2, cmn::Pi/2-.001f);

		//left/right
		if(GetKey(SAPP_KEYCODE_LEFT).held) cam.yaw-=dt;
		if(GetKey(SAPP_KEYCODE_RIGHT).held) cam.yaw+=dt;
	}

	void handleUserInput(float dt) {
		handleCameraMovement(dt);

		handleCameraLooking(dt);
	}
#pragma endregion

	bool user_update(float dt) override {
		handleUserInput(dt);

		cam.dir=vf3d::polar({1, cam.yaw, cam.pitch});

		//update flocks
		for(auto& a:boids) {
			int num=0;
			vf3d pos, dir, sep;
			for(auto& b:boids) {
				if(&b==&a) continue;

				vf3d sub=b.pos-a.pos;
				float d_sq=dot(sub, sub);
				if(d_sq<boid::flock_rad*boid::flock_rad) {
					num++;
					pos+=b.pos;
					dir+=b.dir;
					if(d_sq<boid::avoid_rad*boid::avoid_rad) {
						sep-=sub/std::sqrt(d_sq);
					}
				}
			}
			a.flock_valid=num;
			if(a.flock_valid) {
				a.flock_pos=pos/num;
				a.flock_dir=dir/num;
				a.flock_sep=sep/num;
			}
		}

		//update individuals
		for(auto& b:boids) {
			b.update(dt);
		}

		//wrap coords?
		for(auto& b:boids) {
			if(b.pos.x<bounds.min.x) b.pos.x=bounds.max.x;
			if(b.pos.y<bounds.min.y) b.pos.y=bounds.max.y;
			if(b.pos.z<bounds.min.z) b.pos.z=bounds.max.z;
			if(b.pos.x>bounds.max.x) b.pos.x=bounds.min.x;
			if(b.pos.y>bounds.max.y) b.pos.y=bounds.min.y;
			if(b.pos.z>bounds.max.z) b.pos.z=bounds.min.z;
		}

		return true;
	}

	void renderImGui() {
		simgui_frame_desc_t simgui_frame_desc{};
		simgui_frame_desc.width=sapp_width();
		simgui_frame_desc.height=sapp_height();
		simgui_frame_desc.delta_time=sapp_frame_duration();
		simgui_frame_desc.dpi_scale=sapp_dpi_scale();
		simgui_new_frame(simgui_frame_desc);

		ImGui::Begin("weights");
		{
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("alignment", &boid::alignment_wgt, 0, 1);
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("cohesion", &boid::cohesion_wgt, 0, 1);
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("separation", &boid::separation_wgt, 0, 1);
		}
		ImGui::End();

		ImGui::Begin("limits");
		{
			float min_speed_cm=100*boid::min_speed;
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("min speed[cm/s]", &min_speed_cm, 0, 20);
			boid::min_speed=min_speed_cm/100;
			float max_speed_cm=100*boid::max_speed;
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("max speed[cm/s]", &max_speed_cm, 0, 250);
			boid::max_speed=max_speed_cm/100;
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("max force[N?]", &boid::max_force, 0, 200);
		}
		ImGui::End();

		ImGui::Begin("sensing");
		{
			float flock_rad_cm=100*boid::flock_rad;
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("flock rad[cm]", &flock_rad_cm, 0, 100);
			boid::flock_rad=flock_rad_cm/100;
			float avoid_rad_cm=100*boid::avoid_rad;
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("avoid rad[cm]", &avoid_rad_cm, 0, 100);
			boid::avoid_rad=avoid_rad_cm/100;
		}
		ImGui::End();

		ImGui::Begin("bounds");
		{
			float bounds_min[3]{bounds.min.x, bounds.min.y, bounds.min.z};
			ImGui::DragFloat3("min[m]", bounds_min, .01f, -10, 10, "%.2f");
			bounds.min.x=bounds_min[0];
			bounds.min.y=bounds_min[1];
			bounds.min.z=bounds_min[2];

			float bounds_max[3]{bounds.max.x, bounds.max.y, bounds.max.z};
			ImGui::DragFloat3("max[m]", bounds_max, .01f, -10, 10, "%.2f");
			bounds.max.x=bounds_max[0];
			bounds.max.y=bounds_max[1];
			bounds.max.z=bounds_max[2];

			if(bounds.min.x>bounds.max.x) std::swap(bounds.min.x, bounds.max.x);
			if(bounds.min.y>bounds.max.y) std::swap(bounds.min.y, bounds.max.y);
			if(bounds.min.z>bounds.max.z) std::swap(bounds.min.z, bounds.max.z);
		}
		ImGui::End();

		simgui_render();
	}

	void renderBox(const vf3d& a, const vf3d& b) {
		static const int edges[][2]{
			{0, 1}, {2, 3}, {4, 5}, {6, 7},//thru x
			{0, 2}, {1, 3}, {4, 6}, {5, 7},//thru y
			{0, 4}, {1, 5}, {2, 6}, {3, 7}//thru z
		};

		vf3d v[8];//interpolators: 000, 001, 010...
		for(int i=0; i<8; i++) {
			v[i]=a+vf3d(1&(i>>2), 1&(i>>1), 1&i)*(b-a);
		}

		sgl_begin_lines();
		for(const auto& e:edges) {
			sgl_v3f(v[e[0]].x, v[e[0]].y, v[e[0]].z);
			sgl_v3f(v[e[1]].x, v[e[1]].y, v[e[1]].z);
		}
		sgl_end();
	}

	void renderCone(const vf3d& a, const vf3d& b, float rad) {
		static const int num=8;
		static vf3d ring[num];

		vf3d y=normalize(b-a);
		vf3d x=normalize(getPerp(y));
		vf3d z=cross(x, y);

		for(int i=0; i<num; i++) {
			float angle=2*cmn::Pi*i/num;
			ring[i]=a+
				rad*std::cos(angle)*x+
				rad*std::sin(angle)*z;
		}

		sgl_begin_triangles();
		for(int i=0; i<num; i++) {
			const auto& c=ring[i];
			const auto& n=ring[(1+i)%num];
			//base
			sgl_v3f_c3f(a.x, a.y, a.z, 1, 1, 1);
			sgl_v3f_c3f(c.x, c.y, c.z, 1, 1, 1);
			sgl_v3f_c3f(n.x, n.y, n.z, 1, 1, 1);
			//slant
			sgl_v3f_c3f(b.x, b.y, b.z, 1, 0, 0);
			sgl_v3f_c3f(n.x, n.y, n.z, 1, 1, 1);
			sgl_v3f_c3f(c.x, c.y, c.z, 1, 1, 1);
		}
		sgl_end();
	}

	bool user_render() override {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={.12f, .12f, .12f, .12f};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_projection();
		sgl_perspective(//fov, aspect, range
			sgl_rad(cam.fov_deg),
			sapp_widthf()/sapp_heightf(),
			.01f, 100
		);
		sgl_matrix_mode_modelview();
		sgl_lookat(//eye, target, up
			cam.pos.x, cam.pos.y, cam.pos.z,
			cam.pos.x+cam.dir.x,
			cam.pos.y+cam.dir.y,
			cam.pos.z+cam.dir.z,
			0, 1, 0
		);

		sgl_c3f(0, 0, 0);
		renderBox(bounds.min, bounds.max);

		for(const auto& b:boids) {
			renderCone(b.pos, b.pos+.1f*b.dir, .03f);
		}

		sgl_draw();

		renderImGui();

		sg_end_pass();

		sg_commit();

		return true;
	}
};