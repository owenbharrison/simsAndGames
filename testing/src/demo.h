#define SOKOL_GLCORE
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "shd.glsl.h"

#include"cmn/math/v3d.h"
#include"cmn/math/mat4.h"
using cmn::vf3d;
using cmn::mat4;

#include "cmn/utils.h"

//for memcpy
#include <string>

//y p => x y z
//0 0 => 0 0 1
static vf3d polarToCartesian(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

class DepthDemo : public cmn::SokolEngine {
	sg_sampler sampler{};
	
	struct {
		sg_pass_action pass_action{};

		sg_sampler sampler{};

		sg_pipeline pip{};

		sg_view depth_attach{};
		sg_view depth_tex{};

		mat4 proj;
	} shadow_map;

	sg_pass_action display_pass_action{};
	
	struct {
		sg_pipeline pip{};
		
		sg_buffer vbuf{};
		sg_buffer ibuf{};
	} cube;

	sg_buffer quad_vbuf{};
	sg_pipeline depthview_pip{};

	struct {
		vf3d pos;
		vf3d dir;
		float yaw=0, pitch=0;
		
		const float near=.001f;
		const float far=100.f;
		
		mat4 proj, view;
		mat4 view_proj;
	} cam;

public:
	void setupEnvironment() {
		sg_desc desc{};
		desc.environment=sglue_environment();
		sg_setup(desc);
	}

	void setupSampler() {
		sg_sampler_desc sampler_desc{};
		sampler_desc.compare=SG_COMPAREFUNC_GREATER;
		sampler=sg_make_sampler(sampler_desc);
	}

	//make pipeline, make render targets, & orient view matrixes
	void setupShadowMap() {
		shadow_map.pass_action.depth.load_action=SG_LOADACTION_CLEAR;
		shadow_map.pass_action.depth.store_action=SG_STOREACTION_STORE;
		shadow_map.pass_action.depth.clear_value=0;

		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_shadow_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_shadow_i_col].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.shader=sg_make_shader(shadow_shader_desc(sg_query_backend()));
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode=SG_CULLMODE_NONE;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_GREATER_EQUAL;
		pip_desc.depth.pixel_format=SG_PIXELFORMAT_DEPTH;
		pip_desc.colors[0].pixel_format=SG_PIXELFORMAT_NONE;
		shadow_map.pip=sg_make_pipeline(pip_desc);

		//make depth image
		sg_image_desc image_desc{};
		image_desc.usage.depth_stencil_attachment=true;
		image_desc.width=1024;
		image_desc.height=1024;
		image_desc.pixel_format=SG_PIXELFORMAT_DEPTH;
		sg_image depth_img=sg_make_image(image_desc);

		//make depth attachment
		{
			sg_view_desc view_desc{};
			view_desc.depth_stencil_attachment.image=depth_img;
			shadow_map.depth_attach=sg_make_view(view_desc);
		}

		//make depth tex
		{
			sg_view_desc view_desc{};
			view_desc.texture.image=depth_img;
			shadow_map.depth_tex=sg_make_view(view_desc);
		}

		//make projection matrix
		shadow_map.proj=mat4::makePerspective(90.f, 1, cam.near, cam.far);
	}

	void setupDisplayPassAction() {
		display_pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
		display_pass_action.colors[0].clear_value={0, 0, 0, 1};
	}

	void setupCube() {
		//pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_cube_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_cube_i_col].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.shader=sg_make_shader(cube_shader_desc(sg_query_backend()));
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode=SG_CULLMODE_FRONT;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		cube.pip=sg_make_pipeline(pip_desc);
		
		//vertex buffer
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
			cube.vbuf=sg_make_buffer(buffer_desc);
		}

		//index buffer
		{
			std::uint32_t indexes[12][3]{
				{0, 2, 1}, {1, 2, 3},
				{1, 3, 5}, {5, 3, 7},
				{4, 5, 6}, {5, 7, 6},
				{0, 4, 2}, {2, 4, 6},
				{2, 6, 3}, {3, 6, 7},
				{0, 1, 4}, {1, 5, 4}
			};
			sg_buffer_desc buffer_desc{};
			buffer_desc.usage.index_buffer=true;
			buffer_desc.data=SG_RANGE(indexes);
			cube.ibuf=sg_make_buffer(buffer_desc);
		}
	}

	void setupQuadVertexBuffer() {
		//xyuv: flip v
		float vertexes[4][2][2]{
			{{-1, -1}, {0, 0}},
			{{1, -1}, {1, 0}},
			{{-1, 1}, {0, 1}},
			{{1, 1}, {1, 1}}
		};
		sg_buffer_desc vbuf_desc{};
		vbuf_desc.data.ptr=vertexes;
		vbuf_desc.data.size=sizeof(vertexes);
		quad_vbuf=sg_make_buffer(vbuf_desc);
	}

	void setupDepthviewPipeline() {
		//2d tristrip
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_depthview_i_pos].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_depthview_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(depthview_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		depthview_pip=sg_make_pipeline(pip_desc);
	}

	void userCreate() override {
		setupEnvironment();

		setupDepthviewPipeline();

		setupSampler();

		setupShadowMap();

		setupDisplayPassAction();
		
		setupQuadVertexBuffer();

		setupCube();
	}

#pragma region UPDATE HELPERS
	void handleOrbit(float dt) {
		const auto action=getMouse(SAPP_MOUSEBUTTON_LEFT);
		if(action.pressed) sapp_lock_mouse(true);
		if(action.held) {
			cam.yaw-=mouse_dx*dt;
			cam.pitch-=mouse_dy*dt;
		}
		if(action.released) sapp_lock_mouse(false);
	}
	
	void handleCameraLooking(float dt) {
		//left/right
		if(getKey(SAPP_KEYCODE_LEFT).held) cam.yaw+=dt;
		if(getKey(SAPP_KEYCODE_RIGHT).held) cam.yaw-=dt;

		//up/down
		if(getKey(SAPP_KEYCODE_UP).held) cam.pitch+=dt;
		if(getKey(SAPP_KEYCODE_DOWN).held) cam.pitch-=dt;

		handleOrbit(dt);

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
		cam.proj=mat4::makePerspective(60, asp, cam.near, cam.far);

		cam.view_proj=mat4::mul(cam.proj, cam.view);
	}
#pragma endregion

	void userUpdate(float dt) override {
		handleUserInput(dt);

		updateCameraMatrixes();
	}

	void renderCubeIntoShadowMap() {
		sg_pass pass{};
		pass.action=shadow_map.pass_action;
		pass.attachments.depth_stencil=shadow_map.depth_attach;
		sg_begin_pass(pass);

		sg_apply_pipeline(shadow_map.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=cube.vbuf;
		bind.index_buffer=cube.ibuf;
		sg_apply_bindings(bind);

		vs_shadow_params_t vs_shadow_params{};
		mat4 mvp=mat4::mul(shadow_map.proj, cam.view);
		std::memcpy(vs_shadow_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_shadow_params, SG_RANGE(vs_shadow_params));

		sg_draw(0, 3*12, 1);

		sg_end_pass();
	}

	void renderCube() {
		sg_apply_pipeline(cube.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=cube.vbuf;
		bind.index_buffer=cube.ibuf;
		sg_apply_bindings(bind);

		vs_cube_params_t vs_cube_params{};
		const auto& mvp=cam.view_proj;
		std::memcpy(vs_cube_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_cube_params, SG_RANGE(vs_cube_params));

		sg_draw(0, 3*12, 1);
	}

	void renderShadowMap() {
		sg_apply_pipeline(depthview_pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=quad_vbuf;
		bind.samplers[SMP_u_depthview_smp]=sampler;
		bind.views[VIEW_u_depthview_tex]=shadow_map.depth_tex;
		sg_apply_bindings(bind);

		fs_depthview_params_t fs_depthview_params{};
		fs_depthview_params.u_near=cam.near;
		fs_depthview_params.u_far=cam.far;
		sg_apply_uniforms(UB_fs_depthview_params, SG_RANGE(fs_depthview_params));

		sg_apply_viewport(0, 0, 50, 50, true);

		sg_draw(0, 4, 1);
	}

	void userRender() override {
		renderCubeIntoShadowMap();
		
		//start display pass
		sg_pass pass{};
		pass.action=display_pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		renderCube();

		renderShadowMap();

		sg_end_pass();

		sg_commit();
	}

	DepthDemo() {
		app_title="Depth Demo";
	}
};