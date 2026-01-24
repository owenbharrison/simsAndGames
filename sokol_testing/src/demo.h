//todo: make sliders!
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "shd.glsl.h"

#include "mesh.h"

#include "shape.h"

//for time
#include <ctime>

//list for pointer keeping
//  b/c vector reallocates
#include <list>

#include "sokol/font.h"

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

//x y z => y p
//0 0 1 => 0 0
static void cartesianToPolar(const vf3d& pt, float& yaw, float& pitch) {
	//flatten onto xz
	yaw=std::atan2(pt.x, pt.z);
	//vertical triangle
	pitch=std::atan2(pt.y, std::sqrt(pt.x*pt.x+pt.z*pt.z));
}

class Demo : public cmn::SokolEngine {
	sg_sampler sampler{};

	sg_view tex_blank{};
	sg_view tex_uv{};
	sg_view tex_checker{};

	sg_pass_action display_pass_action{};

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};

		mat4 model[6];

		sg_view tex[6];
	} skybox;

	sg_pipeline mesh_pip{};

	sg_pipeline line_pip{};

	sg_buffer quad_vbuf{};
	sg_pipeline colorview_pip{};

	struct {
		cmn::Font fancy;
		cmn::Font monogram;
		cmn::Font round;

		struct {
			bool to_render=true;

			//colored positions
			float kx=0, ky=0;
			float rx=0, ry=0;
			float gx=0, gy=0;
			float bx=0, by=0;
		} test;
	} fonts;

	bool render_outlines=false;
	std::list<Shape> shapes;

	vf3d light_pos{0, 2, 0};

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

	void setupSampler() {
		sg_sampler_desc sampler_desc{};
		sampler=sg_make_sampler(sampler_desc);
	}

	//"primitive" textures to work with
	void setupTextures() {
		tex_blank=cmn::makeBlankTexture();

		tex_uv=cmn::makeUVTexture(1024, 1024);

		if(!cmn::makeTextureFromFile(tex_checker, "assets/img/checker.png")) tex_checker=tex_blank;
	}

	//clear to black
	void setupDisplayPassAction() {
		display_pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
		display_pass_action.colors[0].clear_value={0, 0, 0, 1};
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
			sg_view& tex=skybox.tex[i];
			if(!cmn::makeTextureFromFile(tex, filenames[i])) tex=tex_uv;
		}
	}

	//works with mesh
	void setupMeshPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_mesh_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_mesh_i_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_mesh_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(mesh_shader_desc(sg_query_backend()));
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode=SG_CULLMODE_FRONT;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		mesh_pip=sg_make_pipeline(pip_desc);
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

	void setupQuadVertexBuffer() {
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
		quad_vbuf=sg_make_buffer(vbuf_desc);
	}

	void setupColorviewPipeline() {
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
		colorview_pip=sg_make_pipeline(pip_desc);
	}

	void setupFonts() {
		fonts.fancy=cmn::Font("assets/img/font/fancy_8x16.png", 8, 16);
		fonts.monogram=cmn::Font("assets/img/font/monogram_6x9.png", 6, 9);
		fonts.round=cmn::Font("assets/img/font/round_6x6.png", 6, 6);

		float margin=5;
		fonts.test.kx=margin, fonts.test.ky=margin;
		fonts.test.rx=margin, fonts.test.ry=margin+fonts.fancy.char_h+fonts.test.ky;
		fonts.test.gx=margin, fonts.test.gy=margin+fonts.monogram.char_h+fonts.test.ry;
		fonts.test.bx=margin, fonts.test.by=margin+fonts.monogram.char_h+fonts.test.gy;
	}

	//scaled cube
	void setupPlatform() {
		Shape shp(Mesh::makeCube(), tex_uv);
		shp.scale={4, .5f, 4};
		shp.translation={0, -1.1f, 0};
		shp.updateMatrixes();

		shapes.push_back(shp);
	}

	//meshes around origin
	void setupShapes() {
		//load animal meshes
		const std::vector<std::string> filenames{
			"assets/models/monkey.txt",
			"assets/models/bunny.txt",
			"assets/models/cow.txt",
			"assets/models/horse.txt",
			"assets/models/dragon.txt"
		};
		std::vector<Shape> shape_circle;
		for(const auto& f:filenames) {
			Mesh msh;
			auto status=Mesh::loadFromOBJ(msh, f);
			if(!status.valid) msh=Mesh::makeCube();
			shape_circle.push_back(Shape(msh, tex_blank));
		}

		//add solids of revolution(i dont like using emplace)
		shape_circle.push_back(Shape(Mesh::makeCube(), tex_uv));
		shape_circle.push_back(Shape(Mesh::makeTorus(.7f, 25, .3f, 13), tex_checker));
		shape_circle.push_back(Shape(Mesh::makeUVSphere(1, 25, 13), tex_checker));
		shape_circle.push_back(Shape(Mesh::makeCylinder(1, 25, 2), tex_checker));
		shape_circle.push_back(Shape(Mesh::makeCone(1, 25, 2), tex_checker));

		//fisher-yates shuffle
		for(int i=shape_circle.size()-1; i>=1; i--) {
			int j=std::rand()%(i+1);
			std::swap(shape_circle[i], shape_circle[j]);
		}

		//place shapes in circle pointing toward origin
		const float radius=3;
		for(int i=0; i<shape_circle.size(); i++) {
			auto& shp=shape_circle[i];

			float angle=2*cmn::Pi*i/shape_circle.size();
			shp.translation=radius*polarToCartesian(angle, 0);
			shp.rotation={0, cmn::Pi+angle, 0};
			float scl=cmn::randFloat(.3f, .5f);
			shp.scale=scl*vf3d(1, 1, 1);

			shp.updateMatrixes();
		}

		shapes.insert(shapes.end(),
			shape_circle.begin(), shape_circle.end()
		);
	}

	void setupCamera() {
		//place camera 3-4 units away from origin
		float dx=1-2*cmn::randFloat();
		float dy=cmn::randFloat();
		float dz=1-2*cmn::randFloat();
		float dist=cmn::randFloat(3, 4);
		cam.pos=dist*vf3d(dx, dy, dz).norm();

		//point towards origin
		cartesianToPolar(-cam.pos, cam.yaw, cam.pitch);
	}
#pragma endregion

	void userCreate() override {
		std::srand(std::time(0));

		setupEnvironment();

		setupSampler();
		setupTextures();

		setupSkybox();

		setupDisplayPassAction();

		setupMeshPipeline();

		setupLinePipeline();

		setupQuadVertexBuffer();
		setupColorviewPipeline();

		setupFonts();

		setupPlatform();
		setupShapes();

		setupCamera();
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
		handleOrbit(dt);

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

	void handleFonts() {
		//toggle font test
		if(getKey(SAPP_KEYCODE_F).pressed) fonts.test.to_render^=true;
		
		//set colored font positions
		if(getKey(SAPP_KEYCODE_K).held) fonts.test.kx=mouse_x, fonts.test.ky=mouse_y;
		if(getKey(SAPP_KEYCODE_R).held) fonts.test.rx=mouse_x, fonts.test.ry=mouse_y;
		if(getKey(SAPP_KEYCODE_G).held) fonts.test.gx=mouse_x, fonts.test.gy=mouse_y;
		if(getKey(SAPP_KEYCODE_B).held) fonts.test.bx=mouse_x, fonts.test.by=mouse_y;
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		cam.dir=polarToCartesian(cam.yaw, cam.pitch);

		handleCameraMovement(dt);

		handleFonts();

		//look at origin
		if(getKey(SAPP_KEYCODE_HOME).held) cartesianToPolar(-cam.pos, cam.yaw, cam.pitch);

		//set light pos
		if(getKey(SAPP_KEYCODE_L).pressed) light_pos=cam.pos;

		//toggle shape outlines
		if(getKey(SAPP_KEYCODE_O).pressed) render_outlines^=true;
	}

	void updateCameraMatrixes() {
		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		//cam proj can change with window resize
		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(60, asp, .001f, 1000.f);

		cam.view_proj=mat4::mul(cam.proj, cam.view);
	}
#pragma endregion

	void userUpdate(float dt) override {
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

	void renderShapes() {
		for(const auto& s:shapes) {
			sg_apply_pipeline(mesh_pip);

			sg_bindings bind{};
			bind.vertex_buffers[0]=s.mesh.vbuf;
			bind.index_buffer=s.mesh.ibuf;
			bind.samplers[SMP_u_mesh_smp]=sampler;
			bind.views[VIEW_u_mesh_tex]=s.tex;
			sg_apply_bindings(bind);

			vs_mesh_params_t vs_mesh_params{};
			std::memcpy(vs_mesh_params.u_model, s.model.m, sizeof(s.model.m));
			mat4 mvp=mat4::mul(cam.view_proj, s.model);
			std::memcpy(vs_mesh_params.u_mvp, mvp.m, sizeof(mvp.m));
			sg_apply_uniforms(UB_vs_mesh_params, SG_RANGE(vs_mesh_params));

			fs_mesh_params_t fs_mesh_params{};
			fs_mesh_params.u_eye_pos[0]=cam.pos.x;
			fs_mesh_params.u_eye_pos[1]=cam.pos.y;
			fs_mesh_params.u_eye_pos[2]=cam.pos.z;
			fs_mesh_params.u_light_pos[0]=light_pos.x;
			fs_mesh_params.u_light_pos[1]=light_pos.y;
			fs_mesh_params.u_light_pos[2]=light_pos.z;
			sg_apply_uniforms(UB_fs_mesh_params, SG_RANGE(fs_mesh_params));

			sg_draw(0, 3*s.mesh.tris.size(), 1);
		}
	}

	void renderShapeOutlines() {
		for(const auto& s:shapes) {
			sg_apply_pipeline(line_pip);

			sg_bindings bind{};
			bind.vertex_buffers[0]=s.linemesh.vbuf;
			bind.index_buffer=s.linemesh.ibuf;
			sg_apply_bindings(bind);

			vs_line_params_t vs_line_params{};
			mat4 mvp=mat4::mul(cam.view_proj, s.model);
			std::memcpy(vs_line_params.u_mvp, mvp.m, sizeof(mvp.m));
			sg_apply_uniforms(UB_vs_line_params, SG_RANGE(vs_line_params));

			sg_draw(0, 2*s.linemesh.lines.size(), 1);
		}
	}

	void renderChar(const cmn::Font& f, float x, float y, char c, float scl=1, sg_color tint={1, 1, 1, 1}) {
		sg_apply_pipeline(colorview_pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=quad_vbuf;
		bind.samplers[SMP_u_colorview_smp]=sampler;
		bind.views[VIEW_u_colorview_tex]=f.tex;
		sg_apply_bindings(bind);

		vs_colorview_params_t vs_colorview_params{};
		f.getRegion(c,
			vs_colorview_params.u_tl[0],
			vs_colorview_params.u_tl[1],
			vs_colorview_params.u_br[0],
			vs_colorview_params.u_br[1]
		);
		sg_apply_uniforms(UB_vs_colorview_params, SG_RANGE(vs_colorview_params));

		fs_colorview_params_t fs_colorview_params{};
		fs_colorview_params.u_tint[0]=tint.r;
		fs_colorview_params.u_tint[1]=tint.g;
		fs_colorview_params.u_tint[2]=tint.b;
		fs_colorview_params.u_tint[3]=tint.a;
		sg_apply_uniforms(UB_fs_colorview_params, SG_RANGE(fs_colorview_params));

		sg_apply_viewportf(x, y, scl*f.char_w, scl*f.char_h, true);

		sg_draw(0, 4, 1);
	}

	void renderString(const cmn::Font& f, float x, float y, const std::string& str, float scl=1, sg_color tint={1, 1, 1, 1}) {
		int ox=0, oy=0;
		for(const auto& c:str) {
			if(c=='\n') ox=0, oy+=f.char_h;
			//tabsize=2
			else if(c=='\t') ox+=2*f.char_w;
			else if(c>=32&&c<=127) {
				renderChar(f, x+scl*ox, y+scl*oy, c, scl, tint);
				ox+=f.char_w;
			}
		}
	}

	void getStringSize(const cmn::Font& f, const std::string& str, int& w, int& h) {
		w=0, h=0;
		int ox=0, oy=0;
		for(const auto& c:str) {
			if(c==' ') ox++;
			else if(c=='\n') ox=0, oy++;
			else if(c>='!'&&c<='~') {
				ox++;
				if(ox>w) w=ox;
				int nh=1+ox;
				if(nh>h) h=nh;
			}
		}
		w*=f.char_w;
		h*=f.char_w;
	};

	void renderFontTest() {
		renderString(fonts.fancy, fonts.test.kx, fonts.test.ky, "hello, world!", 1, {0, 0, 0, 1});
		renderString(fonts.monogram, fonts.test.rx, fonts.test.ry, "red", 1, {1, 0, 0, 1});
		renderString(fonts.monogram, fonts.test.gx, fonts.test.gy, "green", 1, {0, 1, 0, 1});
		renderString(fonts.monogram, fonts.test.bx, fonts.test.by, "blue", 1, {0, 0, 1, 1});
	}
#pragma endregion

	void userRender() override {
		//start display pass 
		sg_pass pass{};
		pass.action=display_pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		renderSkybox();

		if(render_outlines) renderShapeOutlines();
		else renderShapes();

		if(fonts.test.to_render) renderFontTest();

		sg_end_pass();

		sg_commit();
	}

	Demo() {
		app_title="Font Demo";
	}
};