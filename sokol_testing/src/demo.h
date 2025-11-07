#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "shd.glsl.h"

#include "math/v3d.h"
#include "math/mat4.h"

#include <string>
#include <cmath>

//generalized struct initializer
template<typename T>
void zeroMem(T& t) {
	std::memset(&t, 0, sizeof(T));
}

//y p => x y z
//0 0 => 0 0 1
static vf3d polar3D(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

float randFloat(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=std::rand()/rand_max;
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

#include "mesh.h"

struct Demo {
	sg_bindings bindings;
	sg_pipeline pipeline;

#define NUM_KEYS 512
	bool _keys_old[NUM_KEYS],
		_keys_curr[NUM_KEYS],
		_keys_new[NUM_KEYS];

	//cam info
	vf3d cam_pos{2, 2, 4};
	float cam_pos_xyz[3];
	vf3d cam_dir{1, 0, 0};
	float cam_yaw=-2.6f;
	float cam_pitch=-.6f;

	//scene info
	vf3d light_pos{3, 4, -3};
	float light_pos_xyz[3];

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

	bool fps_controls=false;

	Mesh mesh;

	void setupMesh() {
		if(!Mesh::loadFromOBJ(mesh, "assets/suzanne.txt")) {
			//default to cube if mesh loading fails
			Mesh::makeCube(mesh);
		}

		bindings.vertex_buffers[0]=mesh.vbuf;
		bindings.index_buffer=mesh.ibuf;
	}

	void setupTexture() {
		int width=256;
		int height=256;
		std::uint32_t* pixels=new std::uint32_t[width*height];
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				float u=(.5f+i)/width;
				float v=(.5f+j)/height;
				int r=255*u;
				int g=127;
				int b=255*v;
				int a=255;
				pixels[i+width*j]=(a<<24)|(b<<16)|(g<<8)|r;
			}
		}
		sg_image_desc image_desc; zeroMem(image_desc);
		image_desc.width=width;
		image_desc.height=height;
		image_desc.data.mip_levels[0].ptr=pixels;
		image_desc.data.mip_levels[0].size=sizeof(std::uint32_t)*width*height;
		sg_view_desc view_desc; zeroMem(view_desc);
		view_desc.texture.image=sg_make_image(image_desc);
		bindings.views[VIEW_u_tex]=sg_make_view(view_desc);
		delete[] pixels;
	}

	void setupSampler() {
		sg_sampler_desc sampler_desc; zeroMem(sampler_desc);
		bindings.samplers[SMP_u_smp]=sg_make_sampler(sampler_desc);
	}

	void setupPipeline() {
		sg_pipeline_desc pipeline_desc; zeroMem(pipeline_desc);
		pipeline_desc.shader=sg_make_shader(shd_shader_desc(sg_query_backend()));
		pipeline_desc.layout.attrs[ATTR_shd_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pipeline_desc.layout.attrs[ATTR_shd_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pipeline_desc.layout.attrs[ATTR_shd_uv].format=SG_VERTEXFORMAT_SHORT2N;
		pipeline_desc.index_type=SG_INDEXTYPE_UINT32;//use the index buffer
		pipeline_desc.cull_mode=SG_CULLMODE_NONE;
		pipeline_desc.depth.write_enabled=true;//use depth buffer
		pipeline_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pipeline=sg_make_pipeline(pipeline_desc);
	}

	void userInit() {
		setupMesh();

		setupTexture();
		
		setupSampler();

		setupPipeline();
	}

	void init() {
		sg_desc desc; zeroMem(desc);
		desc.environment=sglue_environment();
		sg_setup(desc);

		userInit();

		std::memset(_keys_old, false, sizeof(bool)*NUM_KEYS);
		std::memset(_keys_curr, false, sizeof(bool)*NUM_KEYS);
		std::memset(_keys_new, false, sizeof(bool)*NUM_KEYS);
	}

	void cleanup() {
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
		if(getKey(SAPP_KEYCODE_F).pressed) {
			fps_controls^=true;
			sapp_lock_mouse(fps_controls);
		}

		if(fps_controls) {
			//move with mouse
			const float sensitivity=0.5f*dt;

			//left/right
			cam_yaw-=sensitivity*mouse_dx;

			//up/down (y backwards
			cam_pitch-=sensitivity*mouse_dy;
		} else {
			//move with keys

			//left/right
			if(getKey(SAPP_KEYCODE_LEFT).held) cam_yaw+=dt;
			if(getKey(SAPP_KEYCODE_RIGHT).held) cam_yaw-=dt;

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
		vf3d fb_dir(std::sin(cam_yaw), 0, std::cos(cam_yaw));
		if(getKey(SAPP_KEYCODE_W).held) cam_pos+=5.f*dt*fb_dir;
		if(getKey(SAPP_KEYCODE_S).held) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(getKey(SAPP_KEYCODE_A).held) cam_pos+=4.f*dt*lr_dir;
		if(getKey(SAPP_KEYCODE_D).held) cam_pos-=4.f*dt*lr_dir;
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);
		handleCameraMovement(dt);

		if(getKey(SAPP_KEYCODE_L).pressed) {
			light_pos=cam_pos;
		}

		if(getKey(SAPP_KEYCODE_F11).pressed) {
			sapp_toggle_fullscreen();
		}

		//exit
		if(getKey(SAPP_KEYCODE_ESCAPE).pressed) sapp_request_quit();
	}

	//set title to framerate
	void updateTitle(float dt) {
		char title[64];
		std::snprintf(
			title,
			sizeof(title),
			"Mesh Demo - FPS: %d",
			int(1/dt)
		);
		sapp_set_window_title(title);
	}

	void userUpdate(float dt) {
		handleUserInput(dt);

		//polar to cartesian
		cam_dir=polar3D(cam_yaw, cam_pitch);

		//rotate mesh about x & z
		mesh.rot.x+=.5f*dt;
		mesh.rot.z+=dt;
		mesh.updateMatrixes();

		updateTitle(dt);
	}

	void userRender() {
		//camera transformation matrix
		mat4 look_at=mat4::makeLookAt(cam_pos, cam_pos+cam_dir, {0, 1, 0});
		mat4 view=mat4::inverse(look_at);

		//perspective
		mat4 proj=mat4::makeProjection(90.f, sapp_widthf()/sapp_heightf(), .001f, 1000.f);

		//premultiply transform
		mat4 view_proj=mat4::mul(proj, view);

		sg_pass pass; zeroMem(pass);
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={.62f, .35f, .82f, 1.f};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sg_apply_pipeline(pipeline);
		sg_apply_bindings(bindings);

		//send vertex uniforms
		vs_params_t vs_params; zeroMem(vs_params);
		std::memcpy(vs_params.u_model, mesh.m_model.m, sizeof(vs_params.u_model));
		std::memcpy(vs_params.u_view_proj, view_proj.m, sizeof(vs_params.u_view_proj));
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

		//send fragment uniforms
		fs_params_t fs_params; zeroMem(fs_params);
		fs_params.u_light_pos[0]=light_pos.x;
		fs_params.u_light_pos[1]=light_pos.y;
		fs_params.u_light_pos[2]=light_pos.z;
		fs_params.u_cam_pos[0]=cam_pos.x;
		fs_params.u_cam_pos[1]=cam_pos.y;
		fs_params.u_cam_pos[2]=cam_pos.z;
		sg_apply_uniforms(UB_fs_params, SG_RANGE(fs_params));

		sg_draw(0, 3*mesh.index_tris.size(), 1);

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

		userUpdate(dt);

		userRender();

		mouse_moving=false;

		//update prev key values
		std::memcpy(_keys_old, _keys_curr, sizeof(bool)*NUM_KEYS);
	}
};