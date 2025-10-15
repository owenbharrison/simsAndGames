#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "shd.h"

//for memset, memcpy
#include <string>

#include "mat4.h"

static struct {
	sg_pipeline pip;
	sg_bindings bind;
	float rotation=0;
	float mouse_x=0;
	float mouse_y=0;
} state;

//generalized struct initializer
template<typename T>
void zeroMem(T& t) {
	std::memset(&t, 0, sizeof(T));
}

static void input(const sapp_event* e) {
	switch(e->type) {
		case SAPP_EVENTTYPE_MOUSE_MOVE:
			state.mouse_x=e->mouse_x;
			state.mouse_y=e->mouse_y;
			break;
	}
}

static void create() {
	sg_desc desc; zeroMem(desc);
	desc.environment=sglue_environment();
	sg_setup(desc);

	sg_pipeline_desc pipeline_desc; zeroMem(pipeline_desc);
	pipeline_desc.shader=sg_make_shader(shd_shader_desc(sg_query_backend()));
	pipeline_desc.layout.attrs[ATTR_shd_pos].format=SG_VERTEXFORMAT_FLOAT3;
	pipeline_desc.layout.attrs[ATTR_shd_col].format=SG_VERTEXFORMAT_FLOAT4;
	pipeline_desc.index_type=SG_INDEXTYPE_UINT16;
	pipeline_desc.cull_mode=SG_CULLMODE_BACK;
	pipeline_desc.depth.write_enabled=true;
	pipeline_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
	state.pip=sg_make_pipeline(pipeline_desc);

	struct Vertex {
		float x, y, z;
		float r, g, b, a;
	};

	//rgb cube
	const Vertex vertexes[]{
		-1, -1, -1, 0, 0, 0, 1,
		-1, -1, 1, 0, 0, 1, 1,
		-1, 1, -1, 0, 1, 0, 1,
		-1, 1, 1, 0, 1, 1, 1,
		1, -1, -1, 1, 0, 0, 1,
		1, -1, 1, 1, 0, 1, 1,
		1, 1, -1, 1, 1, 0, 1,
		1, 1, 1, 1, 1, 1, 1
	};
	sg_buffer_desc vbuf_desc; zeroMem(vbuf_desc);
	vbuf_desc.usage.vertex_buffer=true;
	vbuf_desc.data=SG_RANGE(vertexes);
	state.bind.vertex_buffers[0]=sg_make_buffer(vbuf_desc);

	//from blender cube.obj w/ i-1
	const std::uint16_t indexes[]{
		0, 2, 3, 0, 3, 1,
		1, 3, 7, 1, 7, 5,
		5, 7, 6, 5, 6, 4,
		4, 6, 2, 4, 2, 0,
		6, 3, 2, 6, 7, 3,
		0, 1, 5, 0, 5, 4
	};
	sg_buffer_desc ibuf_desc; zeroMem(ibuf_desc);
	ibuf_desc.usage.index_buffer=true;
	ibuf_desc.data=SG_RANGE(indexes);
	state.bind.index_buffer=sg_make_buffer(ibuf_desc);
}

void update() {
	float dt=sapp_frame_duration();
	//set title to framerate
	{
		char title[64];
		std::snprintf(title, sizeof(title), "Hello World Cube - FPS: %d", int(1/dt));
		sapp_set_window_title(title);
	}
	state.rotation+=dt;

	sg_pass pass; zeroMem(pass);
	pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
	pass.action.colors[0].clear_value={.2f, .3f, .4f, 1.f};
	pass.swapchain=sglue_swapchain();
	sg_begin_pass(pass);
	sg_apply_pipeline(state.pip);
	sg_apply_bindings(state.bind);

	mat4 scale=mat4::makeScale({.5f, .5f, .5f});

	//xyz rotation
	mat4 rot_x=mat4::makeRotX(0);
	mat4 rot_y=mat4::makeRotY(0);
	mat4 rot_z=mat4::makeRotZ(state.rotation);
	mat4 rot=mul(rot_z, mul(rot_y, rot_x));
	
	mat4 trans=mat4::makeTranslation({0, 0, 0});

	mat4 model=mul(trans, mul(rot, scale));

	//eye on z=4 plane at origin with y up
	float x01=state.mouse_x/sapp_widthf();
	float y01=state.mouse_y/sapp_heightf();
	mat4 look_at=mat4::makeLookAt({4*x01-2, 2-4*y01, 2}, {0, 0, 0}, {0, 1, 0});
	mat4 view=inverse(look_at);

	//perspective
	mat4 proj=mat4::makeProjection(90.f, sapp_widthf()/sapp_heightf(), .001f, 1000.f);

	//premultiply transform
	mat4 mvp=mul(proj, mul(view, model));

	//send transform matrix to shader
	vs_params_t vs_params; zeroMem(vs_params);
	std::memcpy(vs_params.mvp, mvp.m, sizeof(vs_params.mvp));
	sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

	//draw 36 verts=12 tris
	sg_draw(0, 36, 1);
	sg_end_pass();
	sg_commit();
}

void destroy() {
	sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
	sapp_desc app_desc; zeroMem(app_desc);
	app_desc.init_cb=create;
	app_desc.frame_cb=update;
	app_desc.cleanup_cb=destroy;
	app_desc.event_cb=input;
	app_desc.width=400;
	app_desc.height=400;
	app_desc.icon.sokol_default=true;
	return app_desc;
}