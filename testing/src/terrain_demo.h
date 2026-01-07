#define SOKOL_GLCORE
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "shd.glsl.h"

#include "cmn/math/v3d.h"
#include "cmn/math/mat4.h"

#include "mesh.h"
#include "linemesh.h"

//for srand
#include <ctime>

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

class TerrainDemo : public SokolEngine {
	sg_sampler sampler{};

	sg_view tex_uv{};
	sg_view tex_blank{};
	
	sg_pass_action display_pass_action{};

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};

		mat4 model[6];

		sg_view tex[6];
	} skybox;
	
	sg_pipeline line_pip{};

	struct {
		sg_pipeline pip{};

		Mesh mesh;
		LineMesh linemesh;

		mat4 model;

		sg_view tex{};

		bool render_outlines=false;
	} terrain;

	struct {
		vf3d pos;
		vf3d dir;
		float yaw=0, pitch=0;
		mat4 proj, view;
		//view, then project
		mat4 view_proj;
	} cam;

	vf3d light_pos;

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
		pip_desc.layout.attrs[ATTR_skybox_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_skybox_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(skybox_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		skybox.pip=sg_make_pipeline(pip_desc);

		//vertex buffer
		//xyzuv
		float vertexes[4][5]{
			{-1, 0, -1, 0, 0},
			{1, 0, -1, 1, 0},
			{-1, 0, 1, 0, 1},
			{1, 0, 1, 1, 1}
		};
		sg_buffer_desc buffer_desc{};
		buffer_desc.data=SG_RANGE(vertexes);
		skybox.vbuf=sg_make_buffer(buffer_desc);

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
			mat4 rot_x=mat4::makeRotX(rot_trans[i][0].x);
			mat4 rot_y=mat4::makeRotY(rot_trans[i][0].y);
			mat4 rot_z=mat4::makeRotZ(rot_trans[i][0].z);
			mat4 rot=mat4::mul(rot_z, mat4::mul(rot_y, rot_x));
			mat4 trans=mat4::makeTranslation(rot_trans[i][1]);
			skybox.model[i]=mat4::mul(trans, rot);
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
			if(!makeTextureFromFile(tex, filenames[i])) tex=tex_uv;
		}
	}

	//works with linemesh
	void setupLinePipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_line_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_line_i_col].format=SG_VERTEXFORMAT_FLOAT4;
		pip_desc.shader=sg_make_shader(line_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_LINES;
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		line_pip=sg_make_pipeline(pip_desc);
	}

	void setupTerrain() {
		//pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_terrain_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_terrain_i_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_terrain_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(terrain_shader_desc(sg_query_backend()));
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.face_winding=SG_FACEWINDING_CCW;
		pip_desc.cull_mode=SG_CULLMODE_BACK;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.pixel_format=SG_PIXELFORMAT_DEPTH;
		terrain.pip=sg_make_pipeline(pip_desc);

		Mesh& m=terrain.mesh;
		if(!Mesh::loadFromOBJ(m, "assets/terrain.txt")) m=Mesh::makeCube();
		
		LineMesh& l=terrain.linemesh;
		l=LineMesh::makeFromMesh(m);
		l.randomizeColors();
		l.updateVertexBuffer();

		terrain.model=mat4::makeScale({5, 5, 5});

		sg_view& t=terrain.tex;
		if(!makeTextureFromFile(t, "assets/grass_falloff.png")) t=tex_blank;
	}

	void setupCamera() {
		//randomize camera placement
		float rad=cmn::randFloat(5, 8);
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

		setupDisplayPassAction();

		setupSkybox();

		setupLinePipeline();

		setupTerrain();

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

		if(getKey(SAPP_KEYCODE_L).held) light_pos=cam.pos;

		if(getKey(SAPP_KEYCODE_O).pressed) terrain.render_outlines^=true;
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

#pragma region RENDER HELPERS
	void renderSkybox() {
		//view from eye at origin + camera projection
		mat4 look_at=mat4::makeLookAt({0, 0, 0}, cam.dir, {0, 1, 0});
		mat4 view=mat4::inverse(look_at);
		mat4 view_proj=mat4::mul(cam.proj, view);

		for(int i=0; i<6; i++) {
			sg_apply_pipeline(skybox.pip);

			sg_bindings bind{};
			bind.vertex_buffers[0]=skybox.vbuf;
			bind.samplers[SMP_u_skybox_smp]=sampler;
			bind.views[VIEW_u_skybox_tex]=skybox.tex[i];
			sg_apply_bindings(bind);

			mat4 mvp=mat4::mul(view_proj, skybox.model[i]);

			vs_skybox_params_t vs_skybox_params{};
			std::memcpy(vs_skybox_params.u_mvp, mvp.m, sizeof(mvp.m));
			sg_apply_uniforms(UB_vs_skybox_params, SG_RANGE(vs_skybox_params));

			sg_draw(0, 4, 1);
		}
	}

	void renderTerrain() {
		mat4 mvp=mat4::mul(cam.view_proj, terrain.model);
		
		{
			sg_apply_pipeline(terrain.pip);

			sg_bindings bind{};
			bind.vertex_buffers[0]=terrain.mesh.vbuf;
			bind.index_buffer=terrain.mesh.ibuf;
			bind.samplers[SMP_u_terrain_smp]=sampler;
			bind.views[VIEW_u_terrain_tex]=terrain.tex;
			sg_apply_bindings(bind);
		
			vs_terrain_params_t vs_terrain_params{};
			std::memcpy(vs_terrain_params.u_model, terrain.model.m, sizeof(terrain.model.m));
			std::memcpy(vs_terrain_params.u_mvp, mvp.m, sizeof(mvp.m));
			sg_apply_uniforms(UB_vs_terrain_params, SG_RANGE(vs_terrain_params));

			fs_terrain_params_t fs_terrain_params{};
			fs_terrain_params.u_eye_pos[0]=cam.pos.x;
			fs_terrain_params.u_eye_pos[1]=cam.pos.y;
			fs_terrain_params.u_eye_pos[2]=cam.pos.z;
			fs_terrain_params.u_light_pos[0]=light_pos.x;
			fs_terrain_params.u_light_pos[1]=light_pos.y;
			fs_terrain_params.u_light_pos[2]=light_pos.z;
			sg_apply_uniforms(UB_fs_terrain_params, SG_RANGE(fs_terrain_params));

			sg_draw(0, 3*terrain.mesh.tris.size(), 1);
		}

		if(terrain.render_outlines) {
			sg_apply_pipeline(line_pip);

			sg_bindings bind{};
			bind.vertex_buffers[0]=terrain.linemesh.vbuf;
			bind.index_buffer=terrain.linemesh.ibuf;
			sg_apply_bindings(bind);

			vs_line_params_t vs_line_params{};
			std::memcpy(vs_line_params.u_mvp, mvp.m, sizeof(mvp.m));
			sg_apply_uniforms(UB_vs_line_params, SG_RANGE(vs_line_params));

			sg_draw(0, 2*terrain.linemesh.lines.size(), 1);
		}
	}
#pragma endregion

	void userRender() {
		sg_pass pass{};
		pass.action=display_pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		renderSkybox();

		renderTerrain();

		sg_end_pass();

		sg_commit();
	}

	TerrainDemo() {
		app_title="terrain demo";
	}
};