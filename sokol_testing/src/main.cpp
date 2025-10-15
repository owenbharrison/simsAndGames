#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "shd.h"

#include "v3d.h"

#include "mat4.h"

float randFloat(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=rand()/rand_max;
	return a+t*(b-a);
}

//generalized struct initializer
template<typename T>
void zeroMem(T& t) {
	std::memset(&t, 0, sizeof(T));
}

static sg_bindings bindings;
static sg_pipeline pipeline;

static int num_cubes;
#define MAX_CUBES 1000000
static vf3d cube_pos[MAX_CUBES];
static vf3d cube_vel[MAX_CUBES];

constexpr float Pi=3.1415927f;

#define NUM_KEYS 512
static bool keys_down[NUM_KEYS];

static float delta_time;
static vf3d cam_pos;
static vf3d cam_dir;
static float cam_yaw;
static float cam_pitch;
	
static void create() {
	sg_desc desc; zeroMem(desc);
	desc.environment=sglue_environment();
	sg_setup(desc);

	struct Vertex {
		float x, y, z;
		float r, g, b, a;
	};

	//rgb cube
	float sz=.1f;
	const Vertex vertexes[]{
		-sz, -sz, -sz, 0, 0, 0, 1,
		-sz, -sz, sz, 0, 0, 1, 1,
		-sz, sz, -sz, 0, 1, 0, 1,
		-sz, sz, sz, 0, 1, 1, 1,
		sz, -sz, -sz, 1, 0, 0, 1,
		sz, -sz, sz, 1, 0, 1, 1,
		sz, sz, -sz, 1, 1, 0, 1,
		sz, sz, sz, 1, 1, 1, 1
	};
	sg_buffer_desc vert_buf_desc; zeroMem(vert_buf_desc);
	vert_buf_desc.data=SG_RANGE(vertexes);
	bindings.vertex_buffers[0]=sg_make_buffer(vert_buf_desc);

	//from blender cube.obj w/ i-1
	const std::uint16_t indexes[]{
		0, 2, 3, 0, 3, 1,
		1, 3, 7, 1, 7, 5,
		5, 7, 6, 5, 6, 4,
		4, 6, 2, 4, 2, 0,
		6, 3, 2, 6, 7, 3,
		0, 1, 5, 0, 5, 4
	};
	sg_buffer_desc ix_buf_desc; zeroMem(ix_buf_desc);
	ix_buf_desc.usage.index_buffer=true;
	ix_buf_desc.data=SG_RANGE(indexes);
	bindings.index_buffer=sg_make_buffer(ix_buf_desc);

	sg_buffer_desc inst_buf_desc; zeroMem(inst_buf_desc);
	inst_buf_desc.size=MAX_CUBES*sizeof(vf3d);
	//update often
	inst_buf_desc.usage.stream_update=true;
	bindings.vertex_buffers[1]=sg_make_buffer(inst_buf_desc);

	sg_pipeline_desc pipeline_desc; zeroMem(pipeline_desc);
	pipeline_desc.shader=sg_make_shader(shd_shader_desc(sg_query_backend()));
	pipeline_desc.layout.buffers[1].step_func=SG_VERTEXSTEP_PER_INSTANCE;
	pipeline_desc.layout.attrs[ATTR_shd_pos].format=SG_VERTEXFORMAT_FLOAT3;
	pipeline_desc.layout.attrs[ATTR_shd_pos].buffer_index=0;
	pipeline_desc.layout.attrs[ATTR_shd_col0].format=SG_VERTEXFORMAT_FLOAT4;
	pipeline_desc.layout.attrs[ATTR_shd_col0].buffer_index=0;
	pipeline_desc.layout.attrs[ATTR_shd_inst_pos].format=SG_VERTEXFORMAT_FLOAT3;
	pipeline_desc.layout.attrs[ATTR_shd_inst_pos].buffer_index=1;
	pipeline_desc.index_type=SG_INDEXTYPE_UINT16;
	pipeline_desc.cull_mode=SG_CULLMODE_BACK;
	pipeline_desc.depth.write_enabled=true;
	pipeline_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
	pipeline=sg_make_pipeline(pipeline_desc);

	num_cubes=0;
	cam_yaw=0;
	cam_pitch=0;
	cam_dir={1, 0, 0};
	std::memset(keys_down, false, sizeof(bool)*NUM_KEYS);
}

static void destroy() {
	sg_shutdown();
}

static void input(const sapp_event* e) {
	switch(e->type){
		case SAPP_EVENTTYPE_KEY_DOWN:
			keys_down[e->key_code]=true;
			break;
		case SAPP_EVENTTYPE_KEY_UP:
			keys_down[e->key_code]=false;
			break;
	}
}

static void updateCamera() {
	//look up, down
	if(keys_down[SAPP_KEYCODE_UP]) {
		cam_pitch+=delta_time;
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
	}
	if(keys_down[SAPP_KEYCODE_DOWN]) {
		cam_pitch-=delta_time;
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;
	}

	//look left, right
	if(keys_down[SAPP_KEYCODE_LEFT]) cam_yaw-=delta_time;
	if(keys_down[SAPP_KEYCODE_RIGHT]) cam_yaw+=delta_time;

	//move up, down
	if(keys_down[SAPP_KEYCODE_SPACE]) cam_pos.y+=4.f*delta_time;
	if(keys_down[SAPP_KEYCODE_LEFT_SHIFT]) cam_pos.y-=4.f*delta_time;

	//move forward, backward
	vf3d fb_dir(std::cos(cam_yaw), 0, std::sin(cam_yaw));
	if(keys_down[SAPP_KEYCODE_W]) cam_pos+=5.f*delta_time*fb_dir;
	if(keys_down[SAPP_KEYCODE_S]) cam_pos-=3.f*delta_time*fb_dir;

	//move left, right
	vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
	if(keys_down[SAPP_KEYCODE_A]) cam_pos+=4.f*delta_time*lr_dir;
	if(keys_down[SAPP_KEYCODE_D]) cam_pos-=4.f*delta_time*lr_dir;

	//polar to cartesian
	cam_dir=vf3d(
		std::cos(cam_yaw)*std::cos(cam_pitch),
		std::sin(cam_pitch),
		std::sin(cam_yaw)*std::cos(cam_pitch)
	);
}

static void update() {
	delta_time=sapp_frame_duration();

	updateCamera();

	//emit particles
	if(keys_down[SAPP_KEYCODE_E]) {
		for(int i=0; i<100; i++) {
			if(num_cubes<MAX_CUBES) {
				cube_pos[num_cubes]={0, 0, 0};
				float vx=randFloat(-.5f, .5f);
				float vy=randFloat(1.5f, 2.5f);
				float vz=randFloat(-.5f, .5f);
				cube_vel[num_cubes]={vx, vy, vz};
				num_cubes++;
			}
		}
	}

	//update particles
	for(int i=0; i<num_cubes; i++) {
		//gravity
		cube_vel[i].y-=delta_time;
		cube_pos[i]+=cube_vel[i]*delta_time;
		//bounce
		if(cube_pos[i].y<0) {
			cube_pos[i].y=0;
			cube_vel[i].y*=-1;
		}
	}

	//update instance data
	sg_range range; zeroMem(range);
	range.ptr=cube_pos;
	range.size=num_cubes*sizeof(vf3d);
	sg_update_buffer(bindings.vertex_buffers[1], range);

	sg_pass pass; zeroMem(pass);
	pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
	pass.action.colors[0].clear_value={.2f, .3f, .4f, 1.f};
	pass.swapchain=sglue_swapchain();
	sg_begin_pass(pass);
	sg_apply_pipeline(pipeline);
	sg_apply_bindings(bindings);

	mat4 model=mat4::makeIdentity();

	mat4 look_at=mat4::makeLookAt(cam_pos, cam_pos+cam_dir, {0, 1, 0});
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
	sg_draw(0, 36, num_cubes);
	sg_end_pass();
	sg_commit();

	//set title to framerate
	{
		char title[64];
		std::snprintf(
			title,
			sizeof(title),
			"%d Instanced Cubes - FPS: %d",
			num_cubes,
			int(1/delta_time)
		);
		sapp_set_window_title(title);
	}
}

sapp_desc sokol_main(int argc, char* argv[]) {
	sapp_desc app_desc; zeroMem(app_desc);
	app_desc.init_cb=create;
	app_desc.cleanup_cb=destroy;
	app_desc.event_cb=input;
	app_desc.frame_cb=update;
	app_desc.width=400;
	app_desc.height=400;
	app_desc.icon.sokol_default=true;
	return app_desc;
}