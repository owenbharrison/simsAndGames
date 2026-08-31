#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "vendor/sokol/sokol_app.h"
#include "vendor/sokol/sokol_gfx.h"
#include "vendor/sokol/sokol_glue.h"

#include "vendor/sokol/sokol_gl.h"

#include "common/math/v3d.h"

#include "common/utils.h"

static float in2m(float i) {
	float cm=2.54f*i;
	return cm/100;
}

static float m2in(float m) {
	float cm=100*m;
	return cm/2.54f;
}

#include "common/imgui/imgui_singleheader.h"
#include "vendor/sokol/sokol_imgui.h"

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb/stb_image.h"

using cmn::vf3d;

#include <vector>
#include <algorithm>

struct {
	vf3d cam_pos{in2m(13), in2m(7), in2m(-4)};

	sg_view bg_tex{};

	struct {
		vf3d pos;
		vf3d dir{0, 0, -1};

		float anim=0;
		float anim_spd=5;
		float arg_scl=3.6f;
		int num=6;

		float w=in2m(24);
		float h=in2m(13);
		float breadth=in2m(1.3f);

		sg_view tex{};
	} fish;

	bool debug_viz=false;

	sgl_pipeline pip{};
	sg_sampler smp{};
} static state;

static void init() {
	//sokol environment
	{
		sg_desc desc{};
		desc.environment=sglue_environment();
		sg_setup(desc);
	}

	//imgui
	{
		simgui_desc_t simgui_desc{};
		simgui_desc.ini_filename="assets/fish.imgui";
		simgui_setup(simgui_desc);
	}

	//background texture
	{
		int width, height, comp;
		stbi_uc* pixels=stbi_load("assets/background.png", &width, &height, &comp, 4);
		if(pixels) {
			sg_image_desc img_desc{};
			img_desc.width=width;
			img_desc.height=height;
			img_desc.data.mip_levels[0].ptr=pixels;
			img_desc.data.mip_levels[0].size=sizeof(stbi_uc)*4*width*height;
			sg_image img=sg_make_image(img_desc);

			sg_view_desc view_desc{};
			view_desc.texture.image=img;
			state.bg_tex=sg_make_view(view_desc);
		}
	}

	//fish texture
	{
		int width, height, comp;
		stbi_uc* pixels=stbi_load("assets/brookie.png", &width, &height, &comp, 4);
		if(pixels) {
			sg_image_desc img_desc{};
			img_desc.width=width;
			img_desc.height=height;
			img_desc.data.mip_levels[0].ptr=pixels;
			img_desc.data.mip_levels[0].size=sizeof(stbi_uc)*4*width*height;
			sg_image img=sg_make_image(img_desc);

			sg_view_desc view_desc{};
			view_desc.texture.image=img;
			state.fish.tex=sg_make_view(view_desc);
		}
	}

	//sokol gl
	{
		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);
	}

	//pipeline
	{
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
		state.pip=sgl_make_pipeline(pip_desc);
	}

	//sampler
	{
		sg_sampler_desc smp_desc{};
		state.smp=sg_make_sampler(smp_desc);
	}
}

static void event(const sapp_event* e) {
	simgui_handle_event(e);
}

void renderBackground() {
	sgl_defaults();
	sgl_matrix_mode_projection();
	sgl_ortho(-1, 1, -1, 1, -1, 1);

	sgl_enable_texture();
	sgl_texture(state.bg_tex, state.smp);
	sgl_begin_quads();
	sgl_v2f_t2f(-1, 1, 0, 1);
	sgl_v2f_t2f(1, 1, 1, 1);
	sgl_v2f_t2f(1, -1, 1, 0);
	sgl_v2f_t2f(-1, -1, 0, 0);
	sgl_end();
	sgl_disable_texture();
}

static void renderFish() {
	vf3d up(0, 1, 0);//pseudo
	vf3d fwd=state.fish.dir;
	vf3d rgt=normalize(cross(fwd, up));
	up=cross(rgt, fwd);

	sgl_texture(state.fish.tex, state.smp);

	vf3d t_p, b_p;
	float u_p;
	for(int i=0; i<state.fish.num; i++) {
		float u=i/(state.fish.num-1.f);
		float arg=state.fish.anim+state.fish.arg_scl*u;
		float dr=state.fish.breadth*std::sin(arg);
		vf3d m=state.fish.pos+state.fish.w*(u-.5f)*fwd+dr*rgt;
		vf3d t=m+.5f*state.fish.h*up, b=m-.5f*state.fish.h*up;
		if(i>0) {
			sgl_enable_texture();
			sgl_begin_quads();
			sgl_c3f(1, 1, 1);
			sgl_v3f_t2f(t_p.x, t_p.y, t_p.z, u_p, 0);
			sgl_v3f_t2f(t.x, t.y, t.z, u, 0);
			sgl_v3f_t2f(b.x, b.y, b.z, u, 1);
			sgl_v3f_t2f(b_p.x, b_p.y, b_p.z, u_p, 1);
			sgl_v3f_t2f(t_p.x, t_p.y, t_p.z, u_p, 0);
			sgl_v3f_t2f(b_p.x, b_p.y, b_p.z, u_p, 1);
			sgl_v3f_t2f(b.x, b.y, b.z, u, 1);
			sgl_v3f_t2f(t.x, t.y, t.z, u, 0);
			sgl_end();
			sgl_disable_texture();

			if(state.debug_viz) {
				sgl_begin_lines();
				sgl_c3f(0, 0, 0);
				sgl_v3f(t_p.x, t_p.y, t_p.z), sgl_v3f(t.x, t.y, t.z);
				sgl_v3f(t.x, t.y, t.z), sgl_v3f(b.x, b.y, b.z);
				sgl_v3f(b.x, b.y, b.z), sgl_v3f(b_p.x, b_p.y, b_p.z);
				sgl_v3f(b_p.x, b_p.y, b_p.z), sgl_v3f(t_p.x, t_p.y, t_p.z);
				sgl_v3f(t_p.x, t_p.y, t_p.z), sgl_v3f(b.x, b.y, b.z);
				sgl_end();
			}
		}
		t_p=t, b_p=b, u_p=u;
	}
}

void renderAxes(float d, float s) {
	vf3d dir=-normalize(state.cam_pos);
	const auto& c=state.cam_pos+d*dir;
	sgl_begin_lines();
	sgl_c3f(1, 0, 0);
	sgl_v3f(c.x, c.y, c.z);
	sgl_v3f(c.x+s, c.y, c.z);
	sgl_c3f(0, 1, 0);
	sgl_v3f(c.x, c.y, c.z);
	sgl_v3f(c.x, c.y+s, c.z);
	sgl_c3f(0, 0, 1);
	sgl_v3f(c.x, c.y, c.z);
	sgl_v3f(c.x, c.y, c.z+s);
	sgl_end();
}

void renderImGui() {
	simgui_frame_desc_t simgui_frame_desc{};
	simgui_frame_desc.width=sapp_width();
	simgui_frame_desc.height=sapp_height();
	simgui_frame_desc.delta_time=sapp_frame_duration();
	simgui_frame_desc.dpi_scale=sapp_dpi_scale();
	simgui_new_frame(simgui_frame_desc);

	ImGui::Begin("camera pos(in)");
	float x_in=m2in(state.cam_pos.x);
	float y_in=m2in(state.cam_pos.y);
	float z_in=m2in(state.cam_pos.z);
	ImGui::DragFloat("x", &x_in, .5f, -30, 30);
	ImGui::DragFloat("y", &y_in, .5f, -30, 30);
	ImGui::DragFloat("z", &z_in, .5f, -30, 30);
	state.cam_pos.x=in2m(x_in);
	state.cam_pos.y=in2m(y_in);
	state.cam_pos.z=in2m(z_in);
	ImGui::End();

	ImGui::Begin("animation");
	ImGui::SliderFloat("speed", &state.fish.anim_spd, 0, 10);
	ImGui::SliderFloat("argument", &state.fish.arg_scl, 0, 5);
	ImGui::SliderInt("segments", &state.fish.num, 2, 12);
	ImGui::Checkbox("debug visuals", &state.debug_viz);
	ImGui::End();

	ImGui::Begin("size(in)");
	float w_in=m2in(state.fish.w);
	float h_in=m2in(state.fish.h);
	float breadth_in=m2in(state.fish.breadth);
	ImGui::SliderFloat("width", &w_in, 1, 48);
	ImGui::SliderFloat("height", &h_in, 1, 24);
	ImGui::SliderFloat("breadth", &breadth_in, 0, 8);
	state.fish.w=in2m(w_in);
	state.fish.h=in2m(h_in);
	state.fish.breadth=in2m(breadth_in);
	ImGui::End();

	ImGui::Begin("heading");
	ImGui::SliderFloat("x", &state.fish.dir.x, -1, 1);
	ImGui::SliderFloat("y", &state.fish.dir.y, -1, 1);
	ImGui::SliderFloat("z", &state.fish.dir.z, -1, 1);
	state.fish.dir=normalize(state.fish.dir);
	ImGui::End();

	simgui_render();
}

static void frame() {
	state.fish.anim+=state.fish.anim_spd*sapp_frame_duration();

	sg_pass pass{};
	pass.swapchain=sglue_swapchain();
	sg_begin_pass(pass);

	renderBackground();

	sgl_load_pipeline(state.pip);
	sgl_matrix_mode_projection();
	sgl_perspective(//fov, aspect, range
		sgl_rad(90),
		sapp_widthf()/sapp_heightf(),
		.01f, 100
	);
	sgl_matrix_mode_modelview();
	sgl_lookat(//eye, target, up	
		state.cam_pos.x, state.cam_pos.y, state.cam_pos.z,
		0, 0, 0,
		0, 1, 0
	);

	renderFish();

	if(state.debug_viz) renderAxes(in2m(5), in2m(1));

	sgl_draw();

	renderImGui();

	sg_end_pass();

	sg_commit();
}

static void cleanup() {
	simgui_shutdown();
	sgl_shutdown();
	sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
	sapp_desc app_desc{};
	app_desc.init_cb=init;
	app_desc.frame_cb=frame;
	app_desc.event_cb=event;
	app_desc.cleanup_cb=cleanup;
	app_desc.width=720;
	app_desc.height=540;
	app_desc.window_title="[fish]";
	app_desc.icon.sokol_default=true;

	return app_desc;
}