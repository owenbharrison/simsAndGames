#define SOKOL_GLCORE
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "shd.glsl.h"

#include "cmn/math/v3d.h"
#include "cmn/math/mat4.h"

#include "cmn/utils.h"

//for srand
#include <ctime>

#include "texture_utils.h"

using cmn::vf3d;
using cmn::mat4;

//y p => x y z
//0 0 => 0 0 1
static vf3d polarToCartesian(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

//x y z => y p
//0 0 1 => 0 0
static void cartesianToPolar(const vf3d& pt, float& yaw, float& pitch) {
	//flatten onto xz
	yaw=std::atan2(pt.x, pt.z);
	//vertical triangle
	pitch=std::atan2(pt.y, std::sqrt(pt.x*pt.x+pt.z*pt.z));
}

class PostProcessUI : public SokolEngine {
	sg_sampler sampler{};

	sg_view tex_blank{};

	struct {
		sg_image color_img{SG_INVALID_ID};
		sg_view color_attach{SG_INVALID_ID};
		sg_view color_tex{SG_INVALID_ID};

		sg_image depth_img{SG_INVALID_ID};
		sg_view depth_attach{SG_INVALID_ID};
		sg_view depth_tex{SG_INVALID_ID};

		sg_pass_action pass_action{};

		sg_pipeline pip{};

		sg_buffer vbuf{};
		sg_buffer ibuf{};
	} render;

	struct {
		sg_pass_action pass_action{};

		sg_pipeline pip{};

		sg_buffer vbuf{};
	} display;

	struct {
		vf3d pos;
		vf3d dir;
		float yaw=0, pitch=0;
		mat4 proj, view;
		//view, then project
		mat4 view_proj;
	} cam;

public:
#pragma region SETUP HELPERS
	void setupEnvironment() {
		sg_desc desc{};
		desc.environment=sglue_environment();
		sg_setup(desc);
	}

	//bog-standard nearest-filter sampler
	void setupSampler() {
		sg_sampler_desc sampler_desc{};
		sampler=sg_make_sampler(sampler_desc);
	}
	
	//"primitive" texture to work with
	void setupTextures() {
		tex_blank=makeBlankTexture();
	}

	//this will be called on resize,
	//  so it needs to free & remake the resources
	void setupRenderTarget() {
		//make color img
		{
			sg_destroy_image(render.color_img);
			sg_image_desc image_desc{};
			image_desc.usage.color_attachment=true;
			image_desc.width=sapp_width();
			image_desc.height=sapp_height();
			render.color_img=sg_make_image(image_desc);

			//make color attach
			{
				sg_destroy_view(render.color_attach);
				sg_view_desc view_desc{};
				view_desc.color_attachment.image=render.color_img;
				render.color_attach=sg_make_view(view_desc);
			}
			
			//make color tex
			{
				sg_destroy_view(render.color_tex);
				sg_view_desc view_desc{};
				view_desc.texture.image=render.color_img;
				render.color_tex=sg_make_view(view_desc);
			}
		}

		{
			//make depth img
			sg_destroy_image(render.depth_img);
			sg_image_desc image_desc{};
			image_desc.usage.depth_stencil_attachment=true;
			image_desc.width=sapp_width();
			image_desc.height=sapp_height();
			image_desc.pixel_format=SG_PIXELFORMAT_DEPTH;
			render.depth_img=sg_make_image(image_desc);

			//make depth attach
			{
				sg_destroy_view(render.depth_attach);
				sg_view_desc view_desc{};
				view_desc.depth_stencil_attachment.image=render.depth_img;
				render.depth_attach=sg_make_view(view_desc);
			}
			
			//make depth tex
			{
				sg_destroy_view(render.depth_tex);
				sg_view_desc view_desc{};
				view_desc.texture.image=render.depth_img;
				render.depth_tex=sg_make_view(view_desc);
			}
		}
	}

	void setupRender() {
		setupRenderTarget();
		
		{
			//clear to black
			render.pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
			render.pass_action.colors[0].clear_value={.5f, .5f, .5f, 1};
		}

		{
			//3d
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_render_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
			pip_desc.layout.attrs[ATTR_render_i_col].format=SG_VERTEXFORMAT_FLOAT3;
			pip_desc.shader=sg_make_shader(render_shader_desc(sg_query_backend()));
			pip_desc.index_type=SG_INDEXTYPE_UINT32;
			pip_desc.face_winding=SG_FACEWINDING_CCW;
			pip_desc.cull_mode=SG_CULLMODE_BACK;
			pip_desc.depth.write_enabled=true;
			pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
			pip_desc.depth.pixel_format=SG_PIXELFORMAT_DEPTH;
			render.pip=sg_make_pipeline(pip_desc);
		}

		{
			{
				//xyzrgb
				float vertexes[8][6]{
					{0, 0, 0, 0, 0, 0},
					{1, 0, 0, 1, 0, 0},
					{0, 1, 0, 0, 1, 0},
					{1, 1, 0, 1, 1, 0},
					{0, 0, 1, 0, 0, 1},
					{1, 0, 1, 1, 0, 1},
					{0, 1, 1, 0, 1, 1},
					{1, 1, 1, 1, 1, 1}
				};
				sg_buffer_desc buffer_desc{};
				buffer_desc.data=SG_RANGE(vertexes);
				render.vbuf=sg_make_buffer(buffer_desc);
			}

			{
				std::uint32_t indexes[12][3]{
					{0, 2, 1}, {1, 2, 3},
					{1, 3, 5}, {5, 3, 7},
					{4, 5, 6}, {6, 5, 7},
					{0, 4, 2}, {2, 4, 6},
					{2, 6, 3}, {3, 6, 7},
					{0, 1, 4}, {4, 1, 5}
				};
				sg_buffer_desc buffer_desc{};
				buffer_desc.usage.index_buffer=true;
				buffer_desc.data=SG_RANGE(indexes);
				render.ibuf=sg_make_buffer(buffer_desc);
			}
		}
	}

	void setupDisplay() {
		{	
			//clear to black
			display.pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
			display.pass_action.colors[0].clear_value={0, 0, 0, 1};
		}

		{
			//quad
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_display_i_pos].format=SG_VERTEXFORMAT_FLOAT2;
			pip_desc.layout.attrs[ATTR_display_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader=sg_make_shader(display_shader_desc(sg_query_backend()));
			pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
			display.pip=sg_make_pipeline(pip_desc);
		}

		{
			//xyuv
			float vertexes[4][4]{
				{-1, -1, 0, 0},
				{1, -1, 1, 0},
				{-1, 1, 0, 1},
				{1, 1, 1, 1}
			};
			sg_buffer_desc buffer_desc{};
			buffer_desc.data=SG_RANGE(vertexes);
			display.vbuf=sg_make_buffer(buffer_desc);
		}
	}

	void setupCamera() {
		//randomize camera placement
		float rad=cmn::randFloat(2, 5);
		vf3d dir=vf3d(
			cmn::randFloat(-1, 1),
			cmn::randFloat(-1, 1),
			cmn::randFloat(-1, 1)
		).norm();
		cam.pos=rad*dir;

		//point at origin
		cartesianToPolar(-cam.pos, cam.yaw, cam.pitch);
	}
#pragma endregion

	void userCreate() override {
		setupEnvironment();

		std::srand(std::time(0));

		setupSampler();

		setupTextures();

		setupRender();

		setupDisplay();

		setupCamera();
	}

	void userInput(const sapp_event* e) override {
		switch(e->type) {
			case SAPP_EVENTTYPE_RESIZED:
				setupRenderTarget();
				break;
		}
	}

#pragma region UPDATE HELPERS
	void handleCameraLooking(float dt) {
		//left/right
		if(getKey(SAPP_KEYCODE_LEFT).held) cam.yaw+=dt;
		if(getKey(SAPP_KEYCODE_RIGHT).held) cam.yaw-=dt;

		//up/down
		if(getKey(SAPP_KEYCODE_UP).held) cam.pitch+=dt;
		if(getKey(SAPP_KEYCODE_DOWN).held) cam.pitch-=dt;

		//clamp camera pitch
		if(cam.pitch>cmn::Pi/2) cam.pitch=cmn::Pi/2-.001f;
		if(cam.pitch<-cmn::Pi/2) cam.pitch=.001f-cmn::Pi/2;
	}

	void handleCameraMovement(float dt) {
		//move up, down
		if(getKey(SAPP_KEYCODE_SPACE).held) cam.pos.y+=4.f*dt;
		if(getKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam.pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::sin(cam.yaw), 0, std::cos(cam.yaw));
		if(getKey(SAPP_KEYCODE_W).held) cam.pos+=5.f*dt*fb_dir;
		if(getKey(SAPP_KEYCODE_S).held) cam.pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(getKey(SAPP_KEYCODE_A).held) cam.pos+=4.f*dt*lr_dir;
		if(getKey(SAPP_KEYCODE_D).held) cam.pos-=4.f*dt*lr_dir;
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		cam.dir=polarToCartesian(cam.yaw, cam.pitch);

		handleCameraMovement(dt);
	}

	void updateCameraMatrixes() {
		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		//cam proj can change with window resize
		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(90, asp, .001f, 1000.f);

		cam.view_proj=mat4::mul(cam.proj, cam.view);
	}
#pragma endregion

	void userUpdate(float dt) {
		handleUserInput(dt);

		updateCameraMatrixes();
	}

	//bad naming, but render into the render texture...
	void renderRender() {
		sg_pass pass{};
		pass.action=render.pass_action;
		pass.attachments.colors[0]=render.color_attach;
		pass.attachments.depth_stencil=render.depth_attach;
		sg_begin_pass(pass);

		sg_apply_pipeline(render.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=render.vbuf;
		bind.index_buffer=render.ibuf;
		sg_apply_bindings(bind);

		mat4 model=mat4::makeIdentity();
		mat4 mvp=mat4::mul(cam.view_proj, model);

		vs_render_params_t vs_render_params{};
		std::memcpy(vs_render_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_render_params, SG_RANGE(vs_render_params));
		
		sg_draw(0, 3*12, 1);

		sg_end_pass();
	}

	//post processing
	void renderDisplay() {
		sg_pass pass{};
		pass.action=display.pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sg_apply_pipeline(display.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=display.vbuf;
		bind.samplers[SMP_u_display_smp]=sampler;
		bind.views[VIEW_u_display_tex]=render.color_tex;
		sg_apply_bindings(bind);

		sg_draw(0, 4, 1);

		sg_end_pass();
	}

	void userRender() {
		renderRender();

		renderDisplay();
	
		sg_commit();
	}

	PostProcessUI() {
		app_title="post process testing";
	}
};