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
	bool _keys_old[NUM_KEYS],
		_keys_curr[NUM_KEYS],
		_keys_new[NUM_KEYS];

	//scene info as structure of arrays
	int num_cubes=0;
	const int max_num_cubes=1000000;
	vf3d* cube_pos=nullptr;
	vf3d* cube_rot=nullptr;
	vf3d* cube_vel=nullptr;
	vf3d* cube_rot_vel=nullptr;
	struct inst_data_t { vf3d pos, rot; };
	inst_data_t* cube_instances=nullptr;

	//cam info
	vf3d cam_pos;
	vf3d cam_dir{1, 0, 0};
	float cam_yaw=0;
	float cam_pitch=0;

	bool _mouse_moving_new=false;
	bool mouse_moving=false;

	float _mouse_x_new=0;
	float _mouse_y_new=0;
	float mouse_x=0;
	float mouse_y=0;

	float _mouse_dx_new=0;
	float _mouse_dy_new=0;
	float mouse_dx=0;
	float mouse_dy=0;

	bool mouse_locked=false;

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

		std::memset(_keys_old, false, sizeof(bool)* NUM_KEYS);
		std::memset(_keys_curr, false, sizeof(bool)* NUM_KEYS);
		std::memset(_keys_new, false, sizeof(bool)* NUM_KEYS);

		//allocate heap for these since there are so many
		cube_pos=new vf3d[max_num_cubes];
		cube_rot=new vf3d[max_num_cubes];
		cube_vel=new vf3d[max_num_cubes];
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

	//dont immediately send new key states.
	void input(const sapp_event* e) {
		switch(e->type) {
			case SAPP_EVENTTYPE_KEY_DOWN:
				_keys_new[e->key_code]=true;
				break;
			case SAPP_EVENTTYPE_KEY_UP:
				_keys_new[e->key_code]=false;
				break;
			case SAPP_EVENTTYPE_MOUSE_MOVE:
				mouse_moving=true;
				//apparently these are invalid?
				if(sapp_mouse_locked()) {
					_mouse_x_new=e->mouse_x;
					_mouse_y_new=e->mouse_y;
				}
				//always valid...
				_mouse_dx_new=e->mouse_dx;
				_mouse_dy_new=e->mouse_dy;
				break;
		}
	}

	//fun little helper.
	struct KeyState { bool pressed, held, released; };
	KeyState getKey(const sapp_keycode& kc) const {
		bool held=_keys_curr[kc];
		bool pressed=held&&!_keys_old[kc];
		return {pressed, held, !pressed};
	}

	void handleCameraLooking(float dt) {
		//lock mouse position
		if(getKey(SAPP_KEYCODE_L).pressed) {
			mouse_locked^=true;
			sapp_lock_mouse(mouse_locked);
		}

		if(mouse_locked) {
			//move with mouse
			const float sensitivity=0.5f*dt;

			//left/right
			cam_yaw+=sensitivity*mouse_dx;

			//up/down (y backwards
			cam_pitch-=sensitivity*mouse_dy;
		} else {
			//move with keys

			//left/right
			if(getKey(SAPP_KEYCODE_LEFT).held) cam_yaw-=dt;
			if(getKey(SAPP_KEYCODE_RIGHT).held) cam_yaw+=dt;
			
			//up/down
			if(getKey(SAPP_KEYCODE_UP).held) cam_pitch+=dt;
			if(getKey(SAPP_KEYCODE_DOWN).held) cam_pitch-=dt;
		}
		
		//clamp camera pitch
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;
	}

	void handleCameraMovement(float dt) {
		//move up, down
		if(getKey(SAPP_KEYCODE_SPACE).held) cam_pos.y+=4.f*dt;
		if(getKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::cos(cam_yaw), 0, std::sin(cam_yaw));
		if(getKey(SAPP_KEYCODE_W).held) cam_pos+=5.f*dt*fb_dir;
		if(getKey(SAPP_KEYCODE_S).held) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(getKey(SAPP_KEYCODE_A).held) cam_pos+=4.f*dt*lr_dir;
		if(getKey(SAPP_KEYCODE_D).held) cam_pos-=4.f*dt*lr_dir;
	}

	void emitCube() {
		if(num_cubes>max_num_cubes) return;
		
		//start at origin
		cube_pos[num_cubes]={0, 0, 0};

		//random rotation
		cube_rot[num_cubes]={
			2*Pi*randFloat(),
			2*Pi*randFloat(),
			2*Pi*randFloat()
		};

		float speed=randFloat(1.3f, 2.7f);
		//hemispherical direction
		vf3d dir=randDir();
		if(dir.dot({0, 1, 0})<0) dir*=-1;
		cube_vel[num_cubes]=speed*dir;

		//random rotational speed
		cube_rot_vel[num_cubes]={
			Pi*randFloat(-.5f, .5f),
			Pi*randFloat(-.5f, .5f),
			Pi*randFloat(-.5f, .5f)
		};

		num_cubes++;
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);
		handleCameraMovement(dt);

		//"reset" cubes
		if(getKey(SAPP_KEYCODE_C).pressed) num_cubes=0;

		//emit 100 cubes
		if(getKey(SAPP_KEYCODE_E).held) {
			for(int i=0; i<100; i++) emitCube();
		}

		//exit
		if(getKey(SAPP_KEYCODE_ESCAPE).pressed) sapp_request_quit();
	}

	void update(float dt) {
		handleUserInput(dt);

		//polar to cartesian
		cam_dir=vf3d(
			std::cos(cam_yaw)*std::cos(cam_pitch),
			std::sin(cam_pitch),
			std::sin(cam_yaw)*std::cos(cam_pitch)
		);

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

	void render() {
		//update instance data
		for(int i=0; i<num_cubes; i++) {
			cube_instances[i].pos=cube_pos[i];
			cube_instances[i].rot=cube_rot[i];
		}
		sg_range inst_range; zeroMem(inst_range);
		inst_range.ptr=cube_instances;
		inst_range.size=sizeof(inst_data_t)*num_cubes;
		sg_update_buffer(bindings.vertex_buffers[1], inst_range);

		//compute transform matrixes
		mat4 model=mat4::makeIdentity();

		mat4 look_at=mat4::makeLookAt(cam_pos, cam_pos+cam_dir, {0, 1, 0});
		mat4 view=inverse(look_at);

		//perspective
		mat4 proj=mat4::makeProjection(90.f, sapp_widthf()/sapp_heightf(), .001f, 1000.f);

		//premultiply transform
		mat4 mvp=mul(proj, mul(view, model));
		
		sg_pass pass; zeroMem(pass);
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={.2f, .3f, .4f, 1.f};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);
		sg_apply_pipeline(pipeline);
		sg_apply_bindings(bindings);

		//send transform matrix to shader
		vs_params_t vs_params; zeroMem(vs_params);
		std::memcpy(vs_params.mvp, mvp.m, sizeof(vs_params.mvp));
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

		//draw 36 verts=12 tris
		sg_draw(0, 36, num_cubes);
		sg_end_pass();
		sg_commit();
	}

	void frame() {
		//update curr key values
		std::memcpy(_keys_curr, _keys_new, sizeof(bool)*NUM_KEYS);
		
		//hmm...
		if(mouse_moving) {
			mouse_dx=_mouse_dx_new;
			mouse_dy=_mouse_dy_new;
		} else {
			mouse_dx=0;
			mouse_dy=0;
		}

		const float dt=sapp_frame_duration();

		update(dt);

		render();

		mouse_moving=false;

		//update prev key values
		std::memcpy(_keys_old, _keys_curr, sizeof(bool)*NUM_KEYS);
	}
};