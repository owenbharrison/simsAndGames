#define SOKOL_IMPL
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "vendor/sokol/sokol_app.h"
#include "vendor/sokol/sokol_gfx.h"
#include "vendor/sokol/sokol_glue.h"

#include "common/sokol/sokol_engine.h"

#include "vendor/sokol/sokol_gl.h"

#include <vector>

#include "fish.h"

//for sort
#include <algorithm>

//for time
#include <ctime>

#include "common/utils.h"
#include "common/geom/aabb3.h"

#include "common/imgui/imgui_singleheader.h"
#include "vendor/sokol/sokol_imgui.h"

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb/stb_image.h"

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
		vf3d pos{2.4f, 2.3f, 3.7f};
		float yaw=0, pitch=0;
		vf3d dir;

		float fov_deg=90;
	} cam;

	cmn::AABBf3 bounds{{-3, -2.5f, -2.5f}, {3, 2.5f, 2.5f}};
	std::vector<Fish> fish;

	//graphics
	sgl_pipeline depth_pip{};
	sgl_pipeline fish_pip{};

	sg_sampler smp{};

	bool show_wireframe=false;

	bool show_gui=true;

public:
	//HUGE overestimate
	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_desc.max_commands=500000;
		sgl_desc.max_vertices=500000;
		sgl_setup(sgl_desc);
	}

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

	bool setupFish() {
		//load fish images
		const std::vector<std::string> filenames{
			"assets/bass.png",
			"assets/brookie.png",
			"assets/carp.png",
			"assets/cod.png",
			"assets/roach.png",
			"assets/salmon.png",
			"assets/tuna.png"
		};
		std::vector<sg_view> textures;
		for(const auto& f:filenames) {
			int width, height, comp;
			stbi_uc* pixels=stbi_load(f.c_str(), &width, &height, &comp, 4);
			if(!pixels) return false;

			sg_image_desc img_desc{};
			img_desc.width=width;
			img_desc.height=height;
			img_desc.data.mip_levels[0].ptr=pixels;
			img_desc.data.mip_levels[0].size=sizeof(stbi_uc)*4*width*height;
			sg_image img=sg_make_image(img_desc);

			delete[] pixels;

			sg_view_desc view_desc{};
			view_desc.texture.image=img;
			sg_view tex=sg_make_view(view_desc);

			textures.push_back(tex);
		}

		int num=cmn::randInt(250, 750);
		for(int i=0; i<num; i++) {
			Fish f;

			//random position
			vf3d pos01(
				cmn::randFloat(),
				cmn::randFloat(),
				cmn::randFloat()
			);
			f.pos=bounds.min+(bounds.max-bounds.min)*pos01;

			//random speed
			float speed=cmn::randFloat(Fish::min_speed, Fish::max_speed);
			vf3d dir=normalize(vf3d(
				.5f-cmn::randFloat(),
				.5f-cmn::randFloat(),
				.5f-cmn::randFloat()
			));
			f.vel=speed*dir;

			//random size
			f.length=.01f*cmn::randFloat(10, 30);
			f.height=cmn::randFloat(.4f, .6f)*f.length;
			f.breadth=cmn::randFloat(.1f, .2f)*f.length;

			//init avoidance radius
			f.avoid_rad=length(vf3d(.5f*f.length, .5f*f.height, f.breadth));

			//random animation properties
			f.anim_speed=cmn::randFloat(4, 10);
			f.arg_scl=cmn::randFloat(3, 4);

			//random fish texture
			int ix=cmn::randInt(0, textures.size()-1);
			f.tex=textures[ix];

			fish.push_back(f);
		}

		return true;
	}

	void setupDepthPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.face_winding=SG_FACEWINDING_CCW;
		pip_desc.cull_mode=SG_CULLMODE_BACK;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		depth_pip=sgl_make_pipeline(pip_desc);
	}

	void setupFishPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.face_winding=SG_FACEWINDING_CCW;
		pip_desc.cull_mode=SG_CULLMODE_BACK;
		pip_desc.depth.write_enabled=false;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		//with alpha blending
		pip_desc.colors[0].blend.enabled=true;
		pip_desc.colors[0].blend.src_factor_rgb=SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha=SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		fish_pip=sgl_make_pipeline(pip_desc);
	}

	void setupSampler() {
		sg_sampler_desc smp_desc{};
		smp=sg_make_sampler(smp_desc);
	}

	bool user_create() override {
		app_title="Boids";

		std::srand(std::time(0));

		setupSGL();

		setupImGui();

		setupCamera();

		if(!setupFish()) return false;

		setupDepthPipeline();

		setupFishPipeline();

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

		if(GetKey(SAPP_KEYCODE_ESCAPE).pressed) show_gui^=true;
	}
#pragma endregion

	bool user_update(float dt) override {
		handleUserInput(dt);

		cam.dir=vf3d::polar({1, cam.yaw, cam.pitch});

		//update flocks
		for(auto& a:fish) {
			int num=0;
			vf3d pos, dir, sep;
			for(auto& b:fish) {
				if(&b==&a) continue;

				vf3d sub=b.pos-a.pos;
				float d_sq=dot(sub, sub);
				if(d_sq<Fish::flock_rad*Fish::flock_rad) {
					num++;
					pos+=b.pos;
					dir+=b.dir;
					float r_tot=a.avoid_rad+b.avoid_rad;
					if(d_sq<r_tot*r_tot) {
						sep-=sub/std::sqrt(d_sq);
					}
				}
			}
			a.flock_valid=num;
			if(a.flock_valid) {
				a.flock_pos=pos/num;
				a.flock_dir=normalize(dir);
				a.flock_sep=sep/num;
			}
		}

		//update individuals
		for(auto& f:fish) {
			f.update(dt);
		}

		//wrap coords?
		for(auto& f:fish) {
			if(f.pos.x<bounds.min.x) f.pos.x=bounds.max.x;
			if(f.pos.y<bounds.min.y) f.pos.y=bounds.max.y;
			if(f.pos.z<bounds.min.z) f.pos.z=bounds.max.z;
			if(f.pos.x>bounds.max.x) f.pos.x=bounds.min.x;
			if(f.pos.y>bounds.max.y) f.pos.y=bounds.min.y;
			if(f.pos.z>bounds.max.z) f.pos.z=bounds.min.z;
		}

		return true;
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

	void renderFish(const Fish& f, bool wireframe) {
		static float u_arr[Fish::max_seg];
		static vf3d v_arr[2*Fish::max_seg];

		//get basis vectors
		vf3d fwd=f.dir_smooth;
		vf3d rgt=normalize(cross(fwd, {0, 1, 0}));
		vf3d up=cross(rgt, fwd);

		//scan thru fish, fill up arrays
		for(int i=0; i<Fish::num_seg; i++) {
			float u=i/(Fish::num_seg-1.f);
			float arg=f.anim+f.arg_scl*u;
			float dr=f.breadth*std::sin(arg);
			vf3d m=f.pos+f.length*(u-.5f)*fwd+dr*rgt;
			vf3d du=.5f*f.height*up;
			u_arr[i]=u;
			v_arr[2*i]=m+du;//top
			v_arr[1+2*i]=m-du;//btm
		}

		//render twofaced quads
		sgl_enable_texture();
		sgl_texture(f.tex, smp);
		sgl_begin_quads();
		sgl_c3f(1, 1, 1);
		for(int i=1; i<Fish::num_seg; i++) {
			const auto& u_p=u_arr[i-1], & u=u_arr[i];
			const auto& t_p=v_arr[2*(i-1)], & t=v_arr[2*i];
			const auto& b_p=v_arr[1+2*(i-1)], & b=v_arr[1+2*i];
			sgl_v3f_t2f(t_p.x, t_p.y, t_p.z, u_p, 0);
			sgl_v3f_t2f(t.x, t.y, t.z, u, 0);
			sgl_v3f_t2f(b.x, b.y, b.z, u, 1);
			sgl_v3f_t2f(b_p.x, b_p.y, b_p.z, u_p, 1);
			sgl_v3f_t2f(t_p.x, t_p.y, t_p.z, u_p, 0);
			sgl_v3f_t2f(b_p.x, b_p.y, b_p.z, u_p, 1);
			sgl_v3f_t2f(b.x, b.y, b.z, u, 1);
			sgl_v3f_t2f(t.x, t.y, t.z, u, 0);
		}
		sgl_end();
		sgl_disable_texture();

		//render quad lines
		if(wireframe) {
			sgl_begin_lines();
			sgl_c3f(0, 0, 0);
			for(int i=0; i<Fish::num_seg; i++) {
				const auto& t=v_arr[2*i], & b=v_arr[1+2*i];
				sgl_v3f(t.x, t.y, t.z), sgl_v3f(b.x, b.y, b.z);
				if(i>0) {
					const auto& t_p=v_arr[2*(i-1)], & b_p=v_arr[1+2*(i-1)];
					sgl_v3f(t.x, t.y, t.z), sgl_v3f(t_p.x, t_p.y, t_p.z);
					sgl_v3f(b.x, b.y, b.z), sgl_v3f(b_p.x, b_p.y, b_p.z);
					sgl_v3f(t.x, t.y, t.z), sgl_v3f(b_p.x, b_p.y, b_p.z);
				}
			}
			sgl_end();
		}
	}

	void renderImGui() {
		simgui_frame_desc_t simgui_frame_desc{};
		simgui_frame_desc.width=sapp_width();
		simgui_frame_desc.height=sapp_height();
		simgui_frame_desc.delta_time=sapp_frame_duration();
		simgui_frame_desc.dpi_scale=sapp_dpi_scale();
		simgui_new_frame(simgui_frame_desc);

		ImGui::Begin("Controls");
		{
			ImGui::Text("W/A/S/D to move camera around");
			ImGui::Text("Space/LShift to move camera up/down");
			ImGui::Text("ARROWS to look around");
			ImGui::Text("ESC to toggle GUI");
		}
		ImGui::End();

		ImGui::Begin("Options");
		{
			if(ImGui::TreeNode("Weights")) {
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("alignment", &Fish::alignment_wgt, 0, 1);
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("cohesion", &Fish::cohesion_wgt, 0, 1);
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("separation", &Fish::separation_wgt, 0, 1);
				
				ImGui::TreePop();
			}

			if(ImGui::TreeNode("Limits")) {
				float min_speed_cm=100*Fish::min_speed;
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("min speed[cm/s]", &min_speed_cm, 0, 20);
				Fish::min_speed=min_speed_cm/100;
				float max_speed_cm=100*Fish::max_speed;
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("max speed[cm/s]", &max_speed_cm, 0, 250);
				Fish::max_speed=max_speed_cm/100;
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("max force[N?]", &Fish::max_force, 0, 200);

				ImGui::TreePop();
			}

			if(ImGui::TreeNode("Sensing")) {
				float flock_rad_cm=100*Fish::flock_rad;
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("flock rad[cm]", &flock_rad_cm, 0, 100);
				Fish::flock_rad=flock_rad_cm/100;

				ImGui::TreePop();
			}


			if(ImGui::TreeNode("Fish")) {
				ImGui::SetNextItemWidth(100);
				ImGui::SliderInt(
					"segments", &Fish::num_seg, 2, Fish::max_seg, "%d",
					ImGuiSliderFlags_AlwaysClamp
				);
				ImGui::Checkbox("wireframe", &show_wireframe);

				ImGui::TreePop();
			}

			if(ImGui::TreeNode("Bounds")) {
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

				ImGui::TreePop();
			}
		}
		ImGui::End();

		simgui_render();
	}

	bool user_render() override {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={0, .514f, .812f, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(depth_pip);
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

		sgl_c3f(1, 1, 1);
		renderBox(bounds.min, bounds.max);

		//render sorted fish
		{
			std::vector<const Fish*> draw_order;
			for(const auto& f:fish) draw_order.push_back(&f);

			//farthest first
			std::sort(draw_order.begin(), draw_order.end(),
				[&] (const Fish* a, const Fish* b) {
				vf3d da=a->pos-cam.pos, db=b->pos-cam.pos;
				return dot(da, da)>dot(db, db);
			});

			sgl_load_pipeline(fish_pip);
			for(const auto& f:draw_order) {
				renderFish(*f, show_wireframe);
			}
		}

		sgl_draw();

		if(show_gui) renderImGui();

		sg_end_pass();

		sg_commit();

		return true;
	}
};