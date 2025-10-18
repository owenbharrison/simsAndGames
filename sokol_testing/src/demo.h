#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "shd.h"

#include "math/v3d.h"
#include "math/mat4.h"

//generalized struct initializer
template<typename T>
void zeroMem(T& t) {
	std::memset(&t, 0, sizeof(T));
}

float randFloat(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=rand()/rand_max;
	return a+t*(b-a);
}

constexpr float Pi=3.1415927f;

//https://stackoverflow.com/a/6178290
float randNormal() {
	float rho=std::sqrt(-2*std::log(randFloat()));
	float theta=2*Pi*randFloat();
	return rho*std::cos(theta);
}

//https://math.stackexchange.com/a/1585996
vf3d randDir() {
	return vf3d(
		randNormal(),
		randNormal(),
		randNormal()
	).norm();
}

struct Demo {
	sg_bindings bindings;
	sg_pipeline pipeline;

#define NUM_KEYS 512
	bool keys_down[NUM_KEYS];

	//scene info as structure of arrays
	int num_cubes=0;
	const int max_num_cubes=1000000;
	vf3d* cube_pos=nullptr;
	vf3d* cube_vel=nullptr;
	vf3d* cube_rot=nullptr;
	vf3d* cube_rot_vel=nullptr;
	struct inst_data_t { vf3d pos, rot; };
	inst_data_t* cube_instances=nullptr;

	//cam info
	vf3d cam_pos;
	vf3d cam_dir{1, 0, 0};
	float cam_yaw=0;
	float cam_pitch=0;

	void init() {
		sg_desc desc; zeroMem(desc);
		desc.environment=sglue_environment();
		sg_setup(desc);

		struct Vertex {
			float x, y, z;
			float r, g, b, a;
		};

		//rgb cube
		float sz=.1f;
		const Vertex cube_vertexes[]{
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
		vert_buf_desc.data=SG_RANGE(cube_vertexes);
		bindings.vertex_buffers[0]=sg_make_buffer(vert_buf_desc);

		//from blender cube.obj w/ i-1
		const std::uint16_t cube_indexes[]{
			0, 2, 3, 0, 3, 1,
			1, 3, 7, 1, 7, 5,
			5, 7, 6, 5, 6, 4,
			4, 6, 2, 4, 2, 0,
			6, 3, 2, 6, 7, 3,
			0, 1, 5, 0, 5, 4
		};
		sg_buffer_desc ix_buf_desc; zeroMem(ix_buf_desc);
		ix_buf_desc.usage.index_buffer=true;
		ix_buf_desc.data=SG_RANGE(cube_indexes);
		bindings.index_buffer=sg_make_buffer(ix_buf_desc);

		sg_buffer_desc inst_buf_desc; zeroMem(inst_buf_desc);
		inst_buf_desc.size=sizeof(inst_data_t)*max_num_cubes;
		inst_buf_desc.usage.stream_update=true;
		bindings.vertex_buffers[1]=sg_make_buffer(inst_buf_desc);

		sg_pipeline_desc pipeline_desc; zeroMem(pipeline_desc);
		pipeline_desc.shader=sg_make_shader(shd_shader_desc(sg_query_backend()));
		pipeline_desc.layout.buffers[1].step_func=SG_VERTEXSTEP_PER_INSTANCE;
		pipeline_desc.layout.attrs[ATTR_shd_vert_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pipeline_desc.layout.attrs[ATTR_shd_vert_pos].buffer_index=0;
		pipeline_desc.layout.attrs[ATTR_shd_vert_col0].format=SG_VERTEXFORMAT_FLOAT4;
		pipeline_desc.layout.attrs[ATTR_shd_vert_col0].buffer_index=0;
		pipeline_desc.layout.attrs[ATTR_shd_cube_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pipeline_desc.layout.attrs[ATTR_shd_cube_pos].buffer_index=1;
		pipeline_desc.layout.attrs[ATTR_shd_cube_rot].format=SG_VERTEXFORMAT_FLOAT3;
		pipeline_desc.layout.attrs[ATTR_shd_cube_rot].buffer_index=1;
		pipeline_desc.index_type=SG_INDEXTYPE_UINT16;
		pipeline_desc.cull_mode=SG_CULLMODE_BACK;
		pipeline_desc.depth.write_enabled=true;
		pipeline_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pipeline=sg_make_pipeline(pipeline_desc);

		std::memset(keys_down, false, sizeof(bool)*NUM_KEYS);

		cube_pos=new vf3d[max_num_cubes];
		cube_vel=new vf3d[max_num_cubes];
		cube_rot=new vf3d[max_num_cubes];
		cube_rot_vel=new vf3d[max_num_cubes];
		cube_instances=new inst_data_t[max_num_cubes];
	}

	void cleanup() {
		delete[] cube_pos;
		delete[] cube_vel;
		delete[] cube_rot;
		delete[] cube_rot_vel;

		sg_shutdown();
	}

	void input(const sapp_event* e) {
		switch(e->type) {
			case SAPP_EVENTTYPE_KEY_DOWN:
				keys_down[e->key_code]=true;
				break;
			case SAPP_EVENTTYPE_KEY_UP:
				keys_down[e->key_code]=false;
				break;
		}
	}

	void updateCamera(float dt) {
		//look up, down
		if(keys_down[SAPP_KEYCODE_UP]) {
			cam_pitch+=dt;
			if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
		}
		if(keys_down[SAPP_KEYCODE_DOWN]) {
			cam_pitch-=dt;
			if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;
		}

		//look left, right
		if(keys_down[SAPP_KEYCODE_LEFT]) cam_yaw-=dt;
		if(keys_down[SAPP_KEYCODE_RIGHT]) cam_yaw+=dt;

		//move up, down
		if(keys_down[SAPP_KEYCODE_SPACE]) cam_pos.y+=4.f*dt;
		if(keys_down[SAPP_KEYCODE_LEFT_SHIFT]) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::cos(cam_yaw), 0, std::sin(cam_yaw));
		if(keys_down[SAPP_KEYCODE_W]) cam_pos+=5.f*dt*fb_dir;
		if(keys_down[SAPP_KEYCODE_S]) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(keys_down[SAPP_KEYCODE_A]) cam_pos+=4.f*dt*lr_dir;
		if(keys_down[SAPP_KEYCODE_D]) cam_pos-=4.f*dt*lr_dir;

		//polar to cartesian
		cam_dir=vf3d(
			std::cos(cam_yaw)*std::cos(cam_pitch),
			std::sin(cam_pitch),
			std::sin(cam_yaw)*std::cos(cam_pitch)
		);
	}

	void frame() {
		const float dt=sapp_frame_duration();

		updateCamera(dt);

		//emit particles
		if(keys_down[SAPP_KEYCODE_E]) {
			for(int i=0; i<100; i++) {
				if(num_cubes<max_num_cubes) {
					cube_pos[num_cubes]={0, 0, 0};
					float speed=randFloat(1.3f, 2.7f);
					cube_vel[num_cubes]=speed*randDir();
					cube_rot[num_cubes]={
						2*Pi*randFloat(),
						2*Pi*randFloat(),
						2*Pi*randFloat()
					};
					cube_rot_vel[num_cubes]={
						Pi*randFloat(-.5f, .5f),
						Pi*randFloat(-.5f, .5f),
						Pi*randFloat(-.5f, .5f)
					};
					num_cubes++;
				}
			}
		}

		//update particles
		for(int i=0; i<num_cubes; i++) {
			//gravity
			cube_vel[i].y-=dt;
			cube_pos[i]+=cube_vel[i]*dt;

			//bounce
			if(cube_pos[i].y<0) {
				cube_pos[i].y=0;
				cube_vel[i].y*=-1;
			}

			//rotations
			cube_rot[i]+=cube_rot_vel[i]*dt;
		}

		//update instance data
		for(int i=0; i<num_cubes; i++) {
			cube_instances[i].pos=cube_pos[i];
			cube_instances[i].rot=cube_rot[i];
		}
		sg_range inst_range; zeroMem(inst_range);
		inst_range.ptr=cube_instances;
		inst_range.size=sizeof(inst_data_t)*num_cubes;
		sg_update_buffer(bindings.vertex_buffers[1], inst_range);

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
				int(1/dt)
			);
			sapp_set_window_title(title);
		}
	}
};