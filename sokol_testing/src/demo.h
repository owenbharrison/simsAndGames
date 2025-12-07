#include "sokol_engine.h"
#include "shd.glsl.h"

#include "math/mat4.h"

#include "mesh.h"

struct Demo : SokolEngine {
	mat4 view_proj;

	sg_image color_img;

	struct {
		sg_pass pass{};
		sg_pipeline pip{};
		sg_bindings bind{};
	} offscreen;
	
	struct {
		sg_pass_action pass_action{};
		sg_pipeline pip{};
		sg_bindings bind{};
	} display;

	Mesh donut;
	Mesh sphere;

	float rx=0, ry=0;

	void setupDisplayPass() {
		//clear to bluish
		display.pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
		display.pass_action.colors[0].clear_value={0.25f, 0.45f, 0.65f, 1.0f};
	}

	void setupOffscreenPass() {
		{
			sg_image_desc image_desc{};
			image_desc.usage.color_attachment=true;
			image_desc.width=256;
			image_desc.height=256;
			image_desc.pixel_format=SG_PIXELFORMAT_RGBA8;
			color_img=sg_make_image(image_desc);

			sg_view_desc view_desc{};
			view_desc.color_attachment.image=color_img;
			offscreen.pass.attachments.colors[0]=sg_make_view(view_desc);
		}

		{
			sg_image_desc image_desc{};
			image_desc.usage.depth_stencil_attachment=true;
			image_desc.width=256;
			image_desc.height=256;
			image_desc.pixel_format=SG_PIXELFORMAT_DEPTH;
			sg_image depth_img=sg_make_image(image_desc);

			sg_view_desc view_desc{};
			view_desc.depth_stencil_attachment.image=depth_img;
			offscreen.pass.attachments.depth_stencil=sg_make_view(view_desc);
		}

		offscreen.pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		offscreen.pass.action.colors[0].clear_value={0.25f, 0.25f, 0.25f, 1.0f};
	}

	void setupMeshes() {
		donut=Mesh::makeTorus(.5f, 24, .3f, 12);
		sphere=Mesh::makeUVSphere(.5f, 36, 24);
	}

	void setupOffscreenPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_offscreen_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_offscreen_v_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_offscreen_v_uv].format=SG_VERTEXFORMAT_SHORT2N;
		pip_desc.shader=sg_make_shader(offscreen_shader_desc(sg_query_backend())),
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode=SG_CULLMODE_FRONT;
		pip_desc.depth.pixel_format=SG_PIXELFORMAT_DEPTH;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.write_enabled=true;
		pip_desc.colors[0].pixel_format=SG_PIXELFORMAT_RGBA8;
		offscreen.pip=sg_make_pipeline(pip_desc);
	}

	void setupDisplayPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_default_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_default_v_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_default_v_uv].format=SG_VERTEXFORMAT_SHORT2N;
		pip_desc.shader=sg_make_shader(default_shader_desc(sg_query_backend()));
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode=SG_CULLMODE_FRONT;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.write_enabled=true;
		display.pip=sg_make_pipeline(pip_desc);
	}

	void setupOffscreenBindings() {
		offscreen.bind.vertex_buffers[0]=donut.vbuf;
		offscreen.bind.index_buffer=donut.ibuf;
	}

	void setupDisplayBindings() {
		display.bind.vertex_buffers[0]=sphere.vbuf;
		display.bind.index_buffer=sphere.ibuf;

		sg_view_desc view_desc{};
		view_desc.texture.image=color_img;
		display.bind.views[VIEW_u_tex]=sg_make_view(view_desc);

		sg_sampler_desc sampler_desc{};
		sampler_desc.min_filter=SG_FILTER_LINEAR;
		sampler_desc.mag_filter=SG_FILTER_LINEAR;
		sampler_desc.wrap_u=SG_WRAP_REPEAT;
		sampler_desc.wrap_v=SG_WRAP_REPEAT;
		display.bind.samplers[SMP_u_smp]=sg_make_sampler(sampler_desc);
	}

	void userCreate() override {
		app_title="Offscreen Demo";

		setupDisplayPass();

		setupOffscreenPass();

		setupMeshes();

		setupOffscreenPipeline();

		setupDisplayPipeline();

		setupOffscreenBindings();

		setupDisplayBindings();
	}

	void userUpdate(float dt) {
		rx+=dt;
		ry+=2*dt;
	}

	void renderOffscreen() {
		mat4 rot_x=mat4::makeRotX(rx);
		mat4 rot_y=mat4::makeRotY(ry);
		mat4 model=mat4::mul(rot_y, rot_x);
		
		mat4 look_at=mat4::makeLookAt({0, 0, 2.5f}, {0, 0, 0}, {0, 1, 0});
		mat4 view=mat4::inverse(look_at);

		mat4 proj=mat4::makePerspective(45, 1, .01f, 10.f);

		mat4 mvp=mat4::mul(proj, mat4::mul(view, model));

		vs_params_t vs_params{};
		std::memcpy(vs_params.u_mvp, mvp.m, sizeof(vs_params.u_mvp));

		sg_begin_pass(offscreen.pass);
		sg_apply_pipeline(offscreen.pip);
		sg_apply_bindings(offscreen.bind);
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));
		sg_draw(0, 3*donut.tris.size(), 1);
		sg_end_pass();
	}

	void renderDisplay() {
		mat4 rot_x=mat4::makeRotX(-.25f*rx);
		mat4 rot_y=mat4::makeRotY(.25f*ry);
		mat4 model=mat4::mul(rot_y, rot_x);

		mat4 look_at=mat4::makeLookAt({0, 0, 1.5f}, {0, 0, 0}, {0, 1, 0});
		mat4 view=mat4::inverse(look_at);

		float asp=sapp_widthf()/sapp_heightf();
		mat4 proj=mat4::makePerspective(45, asp, .01f, 10.f);

		mat4 mvp=mat4::mul(proj, mat4::mul(view, model));

		vs_params_t vs_params{};
		std::memcpy(vs_params.u_mvp, mvp.m, sizeof(vs_params.u_mvp));

		sg_pass pass{};
		pass.action=display.pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);
		sg_apply_pipeline(display.pip);
		sg_apply_bindings(display.bind);
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));
		sg_draw(0, 3*sphere.tris.size(), 1);
		sg_end_pass();
	}

	void userRender() {
		renderOffscreen();

		renderDisplay();

		sg_commit();
	}
};