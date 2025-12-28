#define SOKOL_GLCORE
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "shd.glsl.h"

#include "cmn/math/v3d.h"
#include "cmn/math/mat4.h"

#include "cmn/utils.h"

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

//basic model matrix: scale then rotate then translate
static mat4 makeSRTMatrix(const vf3d& s, const vf3d& r, const vf3d& t) {
	mat4 scale=mat4::makeScale(s);
	mat4 rot_x=mat4::makeRotX(r.x);
	mat4 rot_y=mat4::makeRotY(r.y);
	mat4 rot_z=mat4::makeRotZ(r.z);
	mat4 rot=mat4::mul(rot_z, mat4::mul(rot_y, rot_x));
	mat4 trans=mat4::makeTranslation(t);
	return mat4::mul(trans, mat4::mul(rot, scale));
}

class RubiksCubeUI : public SokolEngine {
	sg_sampler sampler{};
	
	sg_view tex_blank{};
	sg_view tex_uv{};

	sg_pass_action display_pass_action{};
	
	sg_pipeline default_pip{};

	struct {
		sg_pipeline pip{};
		
		sg_buffer vbuf{};

		mat4 model[6];

		sg_view tex[6];
	} skybox;

	struct {
		const int num=3;
		const float gap=.1f;

		sg_pipeline pip{};

		struct {
			sg_buffer vbuf{};
			sg_buffer ibuf{};

			mat4 rot[6];

			const float size=1;
			
			const sg_color cols[6]{
				{1, 0, 0, 1},//+x red
				{0, 0, 1, 1},//+y blue
				{1, 1, 1, 1},//+z white
				{1, .392f, 0, 1},//-x orange
				{0, 1, 0, 1},//-y green
				{1, 1, 0, 1}//-z yellow
			};
		} block;
	} cube;

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

	void setupTextures() {
		tex_blank=makeBlankTexture();
		tex_uv=makeUVTexture(256, 256);
	}

	//clear to bluish
	void setupDisplayPassAction() {
		display_pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
		display_pass_action.colors[0].clear_value={.25f, .45f, .65f, 1.f};
	}

	//make pipeline, orient meshes, & load textures 
	void setupSkybox() {
		//pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_skybox_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_skybox_v_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(skybox_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		skybox.pip=sg_make_pipeline(pip_desc);

		//vertex buffer
		{
			//xyzuv
			float vertexes[4][5] {
				{-1, 0, -1, 0, 0},
				{1, 0, -1, 1, 0},
				{-1, 0, 1, 0, 1},
				{1, 0, 1, 1, 1}
			};
			sg_buffer_desc buffer_desc{};
			buffer_desc.data=SG_RANGE(vertexes);
			skybox.vbuf=sg_make_buffer(buffer_desc);
		}

		//orient faces
		const vf3d rot_trans[6][2]{
			{cmn::Pi*vf3d(.5f, -.5f, 0), {1, 0, 0}},
			{cmn::Pi*vf3d(.5f, .5f, 0), {-1, 0, 0}},
			{cmn::Pi*vf3d(0, 0, 1), {0, 1, 0}},
			{cmn::Pi*vf3d(1, 0, 1), {0, -1, 0}},
			{cmn::Pi*vf3d(.5f, 1, 0), {0, 0, 1}},
			{cmn::Pi*vf3d(.5f, 0, 0), {0, 0, -1}}
		};
		for(int i=0; i<6; i++) {
			vf3d rot=rot_trans[i][0];
			vf3d trans=rot_trans[i][1];
			skybox.model[i]=makeSRTMatrix({1, 1, 1}, rot, trans);
		}

		//textures
		const std::string filenames[6]{
			"assets/px.png",
			"assets/nx.png",
			"assets/py.png",
			"assets/ny.png",
			"assets/pz.png",
			"assets/nz.png"
		};
		for(int i=0; i<6; i++) {
			sg_view& tex=skybox.tex[i];
			if(!makeTextureFromFile(tex, filenames[i]).valid) tex=tex_uv;
		}
	}

	void setupCube() {
		//pipeline
		{
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_cube_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
			pip_desc.shader=sg_make_shader(cube_shader_desc(sg_query_backend()));
			pip_desc.index_type=SG_INDEXTYPE_UINT32;
			pip_desc.cull_mode=SG_CULLMODE_FRONT;
			pip_desc.depth.write_enabled=true;
			pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
			cube.pip=sg_make_pipeline(pip_desc);
		}
		
		//vertex buffer
		{
			float vertexes[4][3]{
				{-.5f, -.5f, 0},
				{.5f, -.5f, 0},
				{-.5f, .5f, 0},
				{.5f, .5f, 0}
			};
			sg_buffer_desc buffer_desc{};
			buffer_desc.data=SG_RANGE(vertexes);
			cube.block.vbuf=sg_make_buffer(buffer_desc);
		}

		//index buffer
		{
			std::uint32_t indexes[2][3]{
				{0, 2, 1},
				{1, 2, 3}
			};
			sg_buffer_desc buffer_desc{};
			buffer_desc.usage.index_buffer=true;
			buffer_desc.data=SG_RANGE(indexes);
			cube.block.ibuf=sg_make_buffer(buffer_desc);
		}

		//orientations
		const vf3d fwd_up[6][2]{
			{{1, 0, 0}, {0, 1, 0}},
			{{0, 1, 0}, {0, 0, -1}},
			{{0, 0, 1}, {0, 1, 0}},
			{{-1, 0, 0}, {0, 1, 0}},
			{{0, -1, 0}, {0, 0, 1}},
			{{0, 0, -1}, {0, 1, 0}}
		};
		for(int i=0; i<6; i++) {
			const vf3d& fwd=fwd_up[i][0];
			const vf3d& pseudo_up=fwd_up[i][1];
			vf3d rgt=fwd.cross(pseudo_up).norm();
			vf3d up=rgt.cross(fwd);
			mat4 m;
			m(0, 0)=rgt.x, m(0, 1)=up.x, m(0, 2)=fwd.x;
			m(1, 0)=rgt.y, m(1, 1)=up.y, m(1, 2)=fwd.y;
			m(2, 0)=rgt.z, m(2, 1)=up.z, m(2, 2)=fwd.z;
			m(3, 3)=1;
			cube.block.rot[i]=m;
		}
	}

	void setupCamera() {
		//randomize camera placement
		float rad=cmn::randFloat(3, 5);
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

		setupSampler();
		setupTextures();

		setupDisplayPassAction();

		setupSkybox();

		setupCube();

		setupCamera();
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

	void renderSkybox() {
		//view from eye at origin + camera projection
		mat4 look_at=mat4::makeLookAt({0, 0, 0}, cam.dir, {0, 1, 0});
		mat4 view=mat4::inverse(look_at);
		mat4 view_proj=mat4::mul(cam.proj, view);

		for(int i=0; i<6; i++) {
			sg_apply_pipeline(skybox.pip);
			
			sg_bindings bind{};
			bind.vertex_buffers[0]=skybox.vbuf;
			bind.samplers[SMP_skybox_smp]=sampler;
			bind.views[VIEW_skybox_tex]=skybox.tex[i];
			sg_apply_bindings(bind);

			mat4 mvp=mat4::mul(view_proj, skybox.model[i]);

			vs_skybox_params_t vs_skybox_params{};
			std::memcpy(vs_skybox_params.u_mvp, mvp.m, sizeof(mvp.m));
			sg_apply_uniforms(UB_vs_skybox_params, SG_RANGE(vs_skybox_params));

			sg_draw(0, 4, 1);
		}
	}

	void renderCube() {
		float total_sz=cube.block.size*cube.num+cube.gap*(cube.num-1);
		float side_st=.5f*total_sz-.5f*cube.block.size;
		for(int i=0; i<6; i++) {
			for(int j=0; j<cube.num; j++) {
				float x=side_st-(cube.block.size+cube.gap)*j;
				for(int k=0; k<cube.num; k++) {
					float y=side_st-(cube.block.size+cube.gap)*k;

					sg_apply_pipeline(cube.pip);

					sg_bindings bind{};
					bind.vertex_buffers[0]=cube.block.vbuf;
					bind.index_buffer=cube.block.ibuf;
					sg_apply_bindings(bind);

					//send model & camera transforms
					//  NOTE: trans is applied b4 rot ON PURPOSE
					mat4 trans=mat4::makeTranslation({x, y, .5f*total_sz});
					mat4 model=mat4::mul(cube.block.rot[i], trans);
					mat4 mvp=mat4::mul(cam.view_proj, model);
					vs_cube_params_t vs_cube_params{};
					std::memcpy(vs_cube_params.u_mvp, mvp.m, sizeof(mvp.m));
					sg_apply_uniforms(UB_vs_cube_params, SG_RANGE(vs_cube_params));

					//send face col
					fs_cube_params_t fs_cube_params{};
					fs_cube_params.u_col[0]=cube.block.cols[i].r;
					fs_cube_params.u_col[1]=cube.block.cols[i].g;
					fs_cube_params.u_col[2]=cube.block.cols[i].b;
					fs_cube_params.u_col[3]=cube.block.cols[i].a;
					sg_apply_uniforms(UB_fs_cube_params, SG_RANGE(fs_cube_params));

					sg_draw(0, 2*3, 1);
				}
			}
		}
	}

	void userRender() {
		sg_pass pass{};
		pass.action=display_pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		renderSkybox();

		renderCube();

		sg_end_pass();

		sg_commit();
	}

	RubiksCubeUI() {
		app_title="Rubiks Cube UI";
	}
};