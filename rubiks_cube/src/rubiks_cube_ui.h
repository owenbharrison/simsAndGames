/*todo:
black cube margins
intuitive controls
orbit camera
selectable sizes
shuffle
beginner solver
*/

#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "shd.glsl.h"

//for srand
#include <ctime>

#include "cmn/utils.h"

#include "sokol/texture_utils.h"

#include "rubiks_cube.h"

#include "cmn/math/v3d.h"
#include "cmn/math/mat4.h"

#include "sokol/font.h"

#include "cmn/easing.h"

#include <deque>

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

struct Turn {
	enum struct Axis {
		X, Y, Z
	} axis;
	int slice;
	bool parity;
};

class RubiksCubeUI : public cmn::SokolEngine {
	sg_sampler smp_linear{};
	sg_sampler smp_nearest{};

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

	struct cube {
		sg_pipeline pip{};

		struct {
			//posnorm
			float vertexes[4][6]{
				{-.5f, -.5f, 0, 0, 0, 1},
				{.5f, -.5f, 0, 0, 0, 1},
				{-.5f, .5f, 0, 0, 0, 1},
				{.5f, .5f, 0, 0, 0, 1}
			};
			sg_buffer vbuf{};

			std::uint32_t indexes[2][3]{
				{0, 2, 1},
				{1, 2, 3}
			};
			sg_buffer ibuf{};
		} facelet;

		const float size=1;
		const float gap=.1f;

		RubiksCube rubiks;

		std::deque<Turn> turn_queue;
		const float turn_time=.33f;
		float turn_timer=0;
		bool render_turn_queue=false;

		mat4 rot[6];

		const sg_color cols[6]{
			{1, 0, 0, 1},//+x red
			{1, 1, 0, 1},//+y yellow
			{0, 0, 1, 1},//+z white
			{1, .392f, 0, 1},//-x orange
			{1, 1, 1, 1},//-y white
			{0, 1, 0, 1}//-z yellow
		};
	} cube;

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};
	} colorview_render;

	cmn::Font font;

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

	void setupSamplers() {
		{
			sg_sampler_desc sampler_desc{};
			sampler_desc.min_filter=SG_FILTER_LINEAR;
			sampler_desc.mag_filter=SG_FILTER_LINEAR;
			smp_linear=sg_make_sampler(sampler_desc);
		}

		{
			sg_sampler_desc sampler_desc{};
			smp_nearest=sg_make_sampler(sampler_desc);
		}
	}

	void setupTextures() {
		tex_blank=cmn::makeBlankTexture();
		tex_uv=cmn::makeUVTexture(256, 256);
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
			"assets/skybox/px.png",
			"assets/skybox/nx.png",
			"assets/skybox/py.png",
			"assets/skybox/ny.png",
			"assets/skybox/pz.png",
			"assets/skybox/nz.png"
		};
		for(int i=0; i<6; i++) {
			sg_view& tex=skybox.tex[i];
			if(!cmn::makeTextureFromFile(tex, filenames[i])) tex=tex_uv;
		}
	}

	void setupCube() {
		//pipeline
		{
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_cube_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
			pip_desc.layout.attrs[ATTR_cube_v_norm].format=SG_VERTEXFORMAT_FLOAT3;
			pip_desc.shader=sg_make_shader(cube_shader_desc(sg_query_backend()));
			pip_desc.index_type=SG_INDEXTYPE_UINT32;
			pip_desc.cull_mode=SG_CULLMODE_FRONT;
			pip_desc.depth.write_enabled=true;
			pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
			cube.pip=sg_make_pipeline(pip_desc);
		}

		//vertex buffer
		{
			sg_buffer_desc buffer_desc{};
			buffer_desc.data=SG_RANGE(cube.facelet.vertexes);
			cube.facelet.vbuf=sg_make_buffer(buffer_desc);
		}

		//index buffer
		{
			sg_buffer_desc buffer_desc{};
			buffer_desc.usage.index_buffer=true;
			buffer_desc.data=SG_RANGE(cube.facelet.indexes);
			cube.facelet.ibuf=sg_make_buffer(buffer_desc);
		}

		//actual cube
		cube.rubiks=RubiksCube(3);

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
			cube.rot[i]=m;
		}
	}

	void setupColorviewRendering() {
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
		colorview_render.pip=sg_make_pipeline(pip_desc);

		//xyuv
		//flip y
		float vertexes[4][2][2]{
			{{-1, -1}, {0, 1}},
			{{1, -1}, {1, 1}},
			{{-1, 1}, {0, 0}},
			{{1, 1}, {1, 0}}
		};
		sg_buffer_desc buffer_desc{};
		buffer_desc.data.ptr=vertexes;
		buffer_desc.data.size=sizeof(vertexes);
		colorview_render.vbuf=sg_make_buffer(buffer_desc);
	}

	void setupFont() {
		font=cmn::Font("assets/monogram_6x9.png", 6, 9);
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

		setupSamplers();

		setupTextures();

		setupDisplayPassAction();

		setupSkybox();

		setupCube();

		setupColorviewRendering();

		setupFont();

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

	void handleCubeControls() {
		if(getKey(SAPP_KEYCODE_R).pressed) {
			cube.rubiks.reset();
			cube.turn_queue.clear();
		}

		bool ccw=!getKey(SAPP_KEYCODE_LEFT_ALT).held;
		if(getKey(SAPP_KEYCODE_1).pressed) cube.turn_queue.push_back({Turn::Axis::X, 0, ccw});
		if(getKey(SAPP_KEYCODE_2).pressed) cube.turn_queue.push_back({Turn::Axis::X, 1, ccw});
		if(getKey(SAPP_KEYCODE_3).pressed) cube.turn_queue.push_back({Turn::Axis::X, 2, ccw});
		if(getKey(SAPP_KEYCODE_4).pressed) cube.turn_queue.push_back({Turn::Axis::Y, 0, ccw});
		if(getKey(SAPP_KEYCODE_5).pressed) cube.turn_queue.push_back({Turn::Axis::Y, 1, ccw});
		if(getKey(SAPP_KEYCODE_6).pressed) cube.turn_queue.push_back({Turn::Axis::Y, 2, ccw});
		if(getKey(SAPP_KEYCODE_7).pressed) cube.turn_queue.push_back({Turn::Axis::Z, 0, ccw});
		if(getKey(SAPP_KEYCODE_8).pressed) cube.turn_queue.push_back({Turn::Axis::Z, 1, ccw});
		if(getKey(SAPP_KEYCODE_9).pressed) cube.turn_queue.push_back({Turn::Axis::Z, 2, ccw});

		if(getKey(SAPP_KEYCODE_C).pressed) cube.rubiks.scramble();
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		cam.dir=polarToCartesian(cam.yaw, cam.pitch);

		handleCameraMovement(dt);

		handleCubeControls();

		if(getKey(SAPP_KEYCODE_T).pressed) cube.render_turn_queue^=true;
	}

	void updateCameraMatrixes() {
		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		//cam proj can change with window resize
		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(90, asp, .001f, 1000.f);

		cam.view_proj=mat4::mul(cam.proj, cam.view);
	}

	void handleCube(float dt) {
		if(cube.turn_queue.empty()) return;

		cube.turn_timer+=dt;

		if(cube.turn_timer>cube.turn_time) {
			cube.turn_timer=0;

			auto turn=cube.turn_queue.front();
			switch(turn.axis) {
				case Turn::Axis::X:
					cube.rubiks.turnX(turn.slice, turn.parity);
					break;
				case Turn::Axis::Y:
					cube.rubiks.turnY(turn.slice, turn.parity);
					break;
				case Turn::Axis::Z:
					cube.rubiks.turnZ(turn.slice, turn.parity);
					break;
			}

			cube.turn_queue.pop_front();
		}
	}
#pragma endregion

	void userUpdate(float dt) {
		handleUserInput(dt);

		updateCameraMatrixes();

		handleCube(dt);
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
			bind.samplers[SMP_skybox_smp]=smp_linear;
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
		const auto& num=cube.rubiks.getNum();
		float total_sz=cube.size*num+cube.gap*(num-1);
		float side_st=.5f*total_sz-.5f*cube.size;

		sg_bindings bind{};
		bind.vertex_buffers[0]=cube.facelet.vbuf;
		bind.index_buffer=cube.facelet.ibuf;

		Turn* curr_turn=nullptr;
		if(cube.turn_queue.size()) curr_turn=&cube.turn_queue.front();

		mat4 rot_axes;
		if(curr_turn) {
			float t=cube.turn_timer/cube.turn_time;
			t=cmn::ease::inOutBack(t);
			float angle=cmn::Pi/2*t;
			if(!curr_turn->parity) angle=-angle;
			switch(curr_turn->axis) {
				case Turn::Axis::X: rot_axes=mat4::makeRotX(angle); break;
				case Turn::Axis::Y: rot_axes=mat4::makeRotY(angle); break;
				case Turn::Axis::Z: rot_axes=mat4::makeRotZ(angle); break;
			}
		} else rot_axes=mat4::makeIdentity();

		for(int i=0; i<num; i++) {
			float x=side_st-(cube.size+cube.gap)*i;
			for(int j=0; j<num; j++) {
				float y=side_st-(cube.size+cube.gap)*j;
				mat4 trans=mat4::makeTranslation({x, y, .5f*total_sz});
				for(int f=0; f<6; f++) {
					int x, y, z;
					cube.rubiks.inv_ix(f, i, j, x, y, z);

					mat4 model=mat4::mul(cube.rot[f], trans);
					if(curr_turn) {
						bool use_rot_axes=false;
						switch(curr_turn->axis) {
							case Turn::Axis::X: if(x==curr_turn->slice) use_rot_axes=true; break;
							case Turn::Axis::Y: if(y==curr_turn->slice) use_rot_axes=true; break;
							case Turn::Axis::Z: if(z==curr_turn->slice) use_rot_axes=true; break;
						}
						if(use_rot_axes) model=mat4::mul(rot_axes, model);
					}

					sg_apply_pipeline(cube.pip);

					sg_apply_bindings(bind);

					//send model & camera transforms
					mat4 mvp=mat4::mul(cam.view_proj, model);
					vs_cube_params_t vs_cube_params{};
					std::memcpy(vs_cube_params.u_model, model.m, sizeof(model.m));
					std::memcpy(vs_cube_params.u_mvp, mvp.m, sizeof(mvp.m));
					sg_apply_uniforms(UB_vs_cube_params, SG_RANGE(vs_cube_params));

					//send face col
					int col=cube.rubiks.grid[cube.rubiks.ix(f, i, j)];
					fs_cube_params_t fs_cube_params{};
					fs_cube_params.u_col[0]=cube.cols[col].r;
					fs_cube_params.u_col[1]=cube.cols[col].g;
					fs_cube_params.u_col[2]=cube.cols[col].b;
					//light at cam
					fs_cube_params.u_light_pos[0]=cam.pos.x;
					fs_cube_params.u_light_pos[1]=cam.pos.y;
					fs_cube_params.u_light_pos[2]=cam.pos.z;
					fs_cube_params.u_eye_pos[0]=cam.pos.x;
					fs_cube_params.u_eye_pos[1]=cam.pos.y;
					fs_cube_params.u_eye_pos[2]=cam.pos.z;
					sg_apply_uniforms(UB_fs_cube_params, SG_RANGE(fs_cube_params));

					sg_draw(0, 2*3, 1);
				}
			}
		}
	}

	//rect, texregion, tint
	void renderTex(
		float x, float y, float w, float h,
		const sg_view& tex, float l, float t, float r, float b,
		const sg_color& tint
	) {
		sg_apply_pipeline(colorview_render.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=colorview_render.vbuf;
		bind.samplers[SMP_u_colorview_smp]=smp_nearest;
		bind.views[VIEW_u_colorview_tex]=tex;
		sg_apply_bindings(bind);

		vs_colorview_params_t vs_colorview_params{};
		vs_colorview_params.u_tl[0]=l;
		vs_colorview_params.u_tl[1]=t;
		vs_colorview_params.u_br[0]=r;
		vs_colorview_params.u_br[1]=b;
		sg_apply_uniforms(UB_vs_colorview_params, SG_RANGE(vs_colorview_params));

		fs_colorview_params_t fs_colorview_params{};
		fs_colorview_params.u_tint[0]=tint.r;
		fs_colorview_params.u_tint[1]=tint.g;
		fs_colorview_params.u_tint[2]=tint.b;
		fs_colorview_params.u_tint[3]=tint.a;
		sg_apply_uniforms(UB_fs_colorview_params, SG_RANGE(fs_colorview_params));

		sg_apply_viewportf(x, y, w, h, true);

		sg_draw(0, 4, 1);
	}

	void renderChar(float x, float y, char c, float scl=1, sg_color tint={1, 1, 1, 1}) {
		float l, t, r, b;
		font.getRegion(c, l, t, r, b);

		renderTex(
			x, y, scl*font.char_w, scl*font.char_h,
			font.tex, l, t, r, b,
			tint
		);
	}

	void renderString(float x, float y, const std::string& str, float scl=1, sg_color tint={1, 1, 1, 1}) {
		int ox=0, oy=0;
		for(const auto& c:str) {
			if(c==' ') ox++;
			//tabsize=2
			else if(c=='\t') ox+=2;
			else if(c=='\n') ox=0, oy++;
			else if(c>='!'&&c<='~') {
				renderChar(x+scl*font.char_w*ox, y+scl*font.char_h*oy, c, scl, tint);
				ox++;
			}
		}
	}

	void renderTurnQueue() {
		int m=6;
		int y=m;
		int scl=2;
		for(const auto& t:cube.turn_queue) {
			std::string parity=t.parity?"CW":"CCW";

			std::string axis;
			sg_color col{0, 0, 0, 0};
			switch(t.axis) {
				case Turn::Axis::X: axis="X", col={1, 0, 0, 1}; break;
				case Turn::Axis::Y: axis="Y", col={0, 1, 0, 1}; break;
				case Turn::Axis::Z: axis="Z", col={0, 0, 1, 1}; break;
			}

			auto slice=std::to_string(t.slice);

			auto str=parity+" along "+axis+" at "+slice;
			renderString(m, y, str, scl, col);

			y+=m+scl*font.char_h;
		}
	}
#pragma endregion

	void userRender() {
		sg_pass pass{};
		pass.action=display_pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		renderSkybox();

		renderCube();

		if(cube.render_turn_queue) renderTurnQueue();
		
		sg_end_pass();

		sg_commit();
	}

	RubiksCubeUI() {
		app_title="Rubiks Cube UI";
	}
};