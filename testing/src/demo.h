#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "shd.glsl.h"

#include "sokol/font.h"

#include "shape.h"

using cmn::mat4;
using cmn::vf3d;

//y p => x y z
//0 0 => 0 0 1
static vf3d polarToCartesian(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

class Demo : public cmn::SokolEngine {
	struct {
		sg_sampler linear{};
		sg_sampler nearest{};
	} smp;

	struct {
		sg_view blank{};
		sg_view uv{};
	} tex;

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};
	} colorview;

	cmn::Font font;

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};

		mat4 model[6];

		sg_view tex[6];
	} skybox;

	struct {
		sg_pipeline pip{};
		
		Shape shp;

		sg_view tex{};
	} terrain;

	struct {
		float yaw=0, pitch=0;
		vf3d dir;
	} sun;

	struct {
		vf3d pos;
		float yaw=0, pitch=0;
		vf3d dir;

		const float near_plane=.001f, far_plane=1000;
		mat4 proj;

		mat4 view;

		mat4 view_proj;
	} cam;

public:
#pragma region SETUP HELPERS
	void setupEnvironment() {
		sg_desc desc{};
		desc.environment=sglue_environment();
		sg_setup(desc);
	}

	void setupSamplers() {
		{
			sg_sampler_desc sampler_desc{};
			smp.linear=sg_make_sampler(sampler_desc);
		}

		{
			sg_sampler_desc sampler_desc{};
			sampler_desc.min_filter=SG_FILTER_NEAREST;
			sampler_desc.mag_filter=SG_FILTER_NEAREST;
			smp.nearest=sg_make_sampler(sampler_desc);
		}
	}

	//"primitive" textures to work with
	void setupTextures() {
		auto& b=tex.blank;
		b=cmn::makeBlankTexture();

		tex.uv=cmn::makeUVTexture(1024, 1024);
	}

	void setupColorview() {
		//2d tristrip pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_colorview_i_pos].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_colorview_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(colorview_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		//with alpha blending
		pip_desc.colors[0].blend.enabled=true;
		pip_desc.colors[0].blend.src_factor_rgb=SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha=SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		colorview.pip=sg_make_pipeline(pip_desc);

		//xyuv
		//flip y
		float vertexes[4][2][2]{
			{{-1, -1}, {0, 1}},
			{{1, -1}, {1, 1}},
			{{-1, 1}, {0, 0}},
			{{1, 1}, {1, 0}}
		};
		sg_buffer_desc vbuf_desc{};
		vbuf_desc.data.ptr=vertexes;
		vbuf_desc.data.size=sizeof(vertexes);
		colorview.vbuf=sg_make_buffer(vbuf_desc);
	}

	void setupFont() {
		font=cmn::Font("assets/img/font/fancy_8x16.png", 8, 16);
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
		{
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
			mat4 rot_x=mat4::makeRotX(rot_trans[i][0].x);
			mat4 rot_y=mat4::makeRotY(rot_trans[i][0].y);
			mat4 rot_z=mat4::makeRotZ(rot_trans[i][0].z);
			mat4 rot=mat4::mul(rot_z, mat4::mul(rot_y, rot_x));
			mat4 trans=mat4::makeTranslation(rot_trans[i][1]);
			skybox.model[i]=mat4::mul(trans, rot);
		}

		//textures
		const std::string filenames[6]{
			"assets/img/skybox/px.png",
			"assets/img/skybox/nx.png",
			"assets/img/skybox/py.png",
			"assets/img/skybox/ny.png",
			"assets/img/skybox/pz.png",
			"assets/img/skybox/nz.png"
		};
		for(int i=0; i<6; i++) {
			sg_view& t=skybox.tex[i];
			if(!cmn::makeTextureFromFile(t, filenames[i])) t=tex.blank;
		}
	}

	void setupTerrain() {
		auto& s=terrain.shp;

		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_terrain_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_terrain_i_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_terrain_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(terrain_shader_desc(sg_query_backend()));
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode=SG_CULLMODE_FRONT;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		terrain.pip=sg_make_pipeline(pip_desc);

		auto status=Mesh::loadFromOBJ(s.mesh, "assets/models/terrain.txt");
		if(status!=Mesh::ReturnCode::Ok) s.mesh=Mesh::makeCube();

		s.scale=100.f*vf3d(1, 1, 1);
		s.updateMatrixes();

		auto& t=terrain.tex;
		if(!cmn::makeTextureFromFile(t, "assets/img/grass_gradient.png")) t=tex.blank;
	}

	void setupSun() {
		sun.yaw=cmn::randFloat(2*cmn::Pi);
	}
#pragma endregion

	void userCreate() override {
		std::srand(std::time(0));

		setupEnvironment();

		setupSamplers();

		setupTextures();

		setupColorview();
		setupFont();

		setupSkybox();

		setupTerrain();

		setupSun();
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
		cam.proj=mat4::makePerspective(60, asp, cam.near_plane, cam.far_plane);

		cam.view_proj=mat4::mul(cam.proj, cam.view);
	}

	void updateSun(float dt) {
		sun.pitch+=dt;
		sun.dir=polarToCartesian(sun.yaw, sun.pitch);
	}
#pragma endregion

	void userUpdate(float dt) override {
		handleUserInput(dt);

		updateCameraMatrixes();

		updateSun(dt);
	}

#pragma region RENDER HELPERS
	void renderTex(float x, float y, float w, float h,
		const sg_view& tex, float l, float t, float r, float b,
		const sg_color& tint
	) {
		sg_apply_pipeline(colorview.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=colorview.vbuf;
		bind.samplers[SMP_b_colorview_smp]=smp.nearest;
		bind.views[VIEW_b_colorview_tex]=tex;
		sg_apply_bindings(bind);

		p_vs_colorview_t p_vs_colorview{};
		p_vs_colorview.u_tl[0]=l;
		p_vs_colorview.u_tl[1]=t;
		p_vs_colorview.u_br[0]=r;
		p_vs_colorview.u_br[1]=b;
		sg_apply_uniforms(UB_p_vs_colorview, SG_RANGE(p_vs_colorview));

		p_fs_colorview_t p_fs_colorview{};
		p_fs_colorview.u_tint[0]=tint.r;
		p_fs_colorview.u_tint[1]=tint.g;
		p_fs_colorview.u_tint[2]=tint.b;
		p_fs_colorview.u_tint[3]=tint.a;
		sg_apply_uniforms(UB_p_fs_colorview, SG_RANGE(p_fs_colorview));

		sg_apply_viewportf(x, y, w, h, true);

		sg_draw(0, 4, 1);
	}

	void renderChar(float x, float y, const cmn::Font& f, char c, float scl=1, const sg_color& tint={1, 1, 1, 1}) {
		float l, t, r, b;
		f.getRegion(c, l, t, r, b);
		renderTex(
			x, y, scl*f.char_w, scl*f.char_h,
			f.tex, l, t, r, b,
			tint
		);
	}

	void renderString(float x, float y, const cmn::Font& f, const std::string& str, float scl=1, const sg_color& tint={1, 1, 1, 1}) {
		int ox=0, oy=0;
		for(const auto& c:str) {
			if(c=='\n') ox=0, oy+=f.char_h;
			//tabsize=2
			else if(c=='\t') ox+=2*f.char_w;
			else if(c>=32&&c<=127) {
				renderChar(x+scl*ox, y+scl*oy, f, c, scl, tint);
				ox+=f.char_w;
			}
		}
	}
#pragma endregion

#pragma region RENDERERS

	void renderSkybox() {
		//view from eye at origin + camera projection
		mat4 look_at=mat4::makeLookAt({0, 0, 0}, cam.dir, {0, 1, 0});
		mat4 view=mat4::inverse(look_at);
		mat4 view_proj=mat4::mul(cam.proj, view);

		for(int i=0; i<6; i++) {
			sg_apply_pipeline(skybox.pip);

			sg_bindings bind{};
			bind.vertex_buffers[0]=skybox.vbuf;
			bind.samplers[SMP_b_skybox_smp]=smp.linear;
			bind.views[VIEW_b_skybox_tex]=skybox.tex[i];
			sg_apply_bindings(bind);

			mat4 mvp=mat4::mul(view_proj, skybox.model[i]);

			p_vs_skybox_t p_vs_skybox{};
			std::memcpy(p_vs_skybox.u_mvp, mvp.m, sizeof(mvp.m));
			sg_apply_uniforms(UB_p_vs_skybox, SG_RANGE(p_vs_skybox));

			sg_draw(0, 4, 1);
		}
	}

	void renderTerrain() {
		const auto& s=terrain.shp;

		sg_apply_pipeline(terrain.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=s.mesh.vbuf;
		bind.index_buffer=s.mesh.ibuf;
		bind.samplers[SMP_b_terrain_smp]=smp.linear;
		bind.views[VIEW_b_terrain_tex]=terrain.tex;
		sg_apply_bindings(bind);

		p_vs_terrain_t p_vs_terrain{};
		std::memcpy(p_vs_terrain.u_model, s.model.m, sizeof(s.model.m));
		mat4 mvp=mat4::mul(cam.view_proj, s.model);
		std::memcpy(p_vs_terrain.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_p_vs_terrain, SG_RANGE(p_vs_terrain));

		p_fs_terrain_t p_fs_terrain{};
		p_fs_terrain.u_eye_pos[0]=cam.pos.x;
		p_fs_terrain.u_eye_pos[1]=cam.pos.y;
		p_fs_terrain.u_eye_pos[2]=cam.pos.z;
		p_fs_terrain.u_light_dir[0]=sun.dir.x;
		p_fs_terrain.u_light_dir[1]=sun.dir.y;
		p_fs_terrain.u_light_dir[2]=sun.dir.z;
		sg_apply_uniforms(UB_p_fs_terrain, SG_RANGE(p_fs_terrain));

		sg_draw(0, 3*s.mesh.tris.size(), 1);
	}
#pragma endregion

	void userRender() override {
		//start display pass
		sg_pass pass{};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		renderSkybox();

		renderTerrain();

		sg_end_pass();

		sg_commit();
	}

	Demo() {
		app_title="Terrain Walking Demo";
	}
};