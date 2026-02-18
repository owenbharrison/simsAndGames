/*todo:
beginner3x3 solver
varying turn speeds:
	fast scramble
	slow solve
mouse interaction
selectable sizes
	NxN solver
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
			//pos norm tex
			const float vertexes[4][8]{
				{-.5f, -.5f, 0, 0, 0, 1, 0, 0},
				{.5f, -.5f, 0, 0, 0, 1, 1, 0},
				{-.5f, .5f, 0, 0, 0, 1, 0, 1},
				{.5f, .5f, 0, 0, 0, 1, 1, 1}
			};
			sg_buffer vbuf{};

			const std::uint32_t indexes[2][3]{
				{0, 2, 1},
				{1, 2, 3}
			};
			sg_buffer ibuf{};

			sg_view tex{};
		} facelet;

		RubiksCube rubiks;

		std::deque<Turn> turn_queue;
		const float turn_time=.25f;
		float turn_timer=0;
		bool render_turn_queue=false;

		mat4 rot[6];

		const sg_color cols[6]{
			{1, 0, 0, 1},//+x red
			{1, 1, 0, 1},//+y yellow
			{0, 0, 1, 1},//+z white
			{1, .5f, 0, 1},//-x orange
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
			pip_desc.layout.attrs[ATTR_cube_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
			pip_desc.layout.attrs[ATTR_cube_i_norm].format=SG_VERTEXFORMAT_FLOAT3;
			pip_desc.layout.attrs[ATTR_cube_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
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

		//texture
		{
			auto& t=cube.facelet.tex;
			if(!cmn::makeTextureFromFile(t, "assets/facelet.png")) t=tex_blank;
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

	void setupIcon() {
		int width, height, comp;
		std::uint8_t* pixels8=stbi_load("assets/icon.png", &width, &height, &comp, 4);
		if(!pixels8) return;

		std::uint32_t* pixels32=new std::uint32_t[width*height];
		std::memcpy(pixels32, pixels8, sizeof(std::uint8_t)*4*width*height);
		stbi_image_free(pixels8);
		
		sapp_icon_desc icon_desc{};
		icon_desc.images[0].width=width;
		icon_desc.images[0].height=height;
		icon_desc.images[0].pixels.ptr=pixels32;
		icon_desc.images[0].pixels.size=sizeof(std::uint32_t)*width*height;
		sapp_set_icon(&icon_desc);
		delete[] pixels32;
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

		setupIcon();

		setupCamera();
	}

#pragma region UPDATE HELPERS
	void handleCameraLooking(float dt) {
		//orbit
		if(getMouse(SAPP_MOUSEBUTTON_LEFT).held) {
			cam.yaw-=mouse_dx*dt;
			cam.pitch-=mouse_dy*dt;
		}
		
		const float turn_speed=1.5f*dt;

		//left/right
		if(getKey(SAPP_KEYCODE_LEFT).held) cam.yaw-=turn_speed;
		if(getKey(SAPP_KEYCODE_RIGHT).held) cam.yaw+=turn_speed;

		//up/down
		if(getKey(SAPP_KEYCODE_UP).held) cam.pitch-=turn_speed;
		if(getKey(SAPP_KEYCODE_DOWN).held) cam.pitch+=turn_speed;

		//clamp camera pitch
		if(cam.pitch>cmn::Pi/2) cam.pitch=cmn::Pi/2-.001f;
		if(cam.pitch<-cmn::Pi/2) cam.pitch=.001f-cmn::Pi/2;
	}

	void handleCameraPlacement() {
		float rad=std::sqrt(3)*cube.rubiks.getNum()/2;
		cam.pos=-(3+rad)*cam.dir;
	}

	void handleCubeControls() {
		if(getKey(SAPP_KEYCODE_HOME).pressed) {
			cube.rubiks.reset();
			cube.turn_queue.clear();
		}

		bool ccw=getKey(SAPP_KEYCODE_LEFT_SHIFT).held;
		if(getKey(SAPP_KEYCODE_F).pressed) cube.turn_queue.push_back(ccw?Turn::f:Turn::F);
		if(getKey(SAPP_KEYCODE_U).pressed) cube.turn_queue.push_back(ccw?Turn::u:Turn::U);
		if(getKey(SAPP_KEYCODE_L).pressed) cube.turn_queue.push_back(ccw?Turn::l:Turn::L);
		if(getKey(SAPP_KEYCODE_B).pressed) cube.turn_queue.push_back(ccw?Turn::b:Turn::B);
		if(getKey(SAPP_KEYCODE_D).pressed) cube.turn_queue.push_back(ccw?Turn::d:Turn::D);
		if(getKey(SAPP_KEYCODE_R).pressed) cube.turn_queue.push_back(ccw?Turn::r:Turn::R);
		if(getKey(SAPP_KEYCODE_X).pressed) cube.turn_queue.push_back(ccw?Turn::x:Turn::X);
		if(getKey(SAPP_KEYCODE_Y).pressed) cube.turn_queue.push_back(ccw?Turn::y:Turn::Y);
		if(getKey(SAPP_KEYCODE_Z).pressed) cube.turn_queue.push_back(ccw?Turn::z:Turn::Z);

		if(getKey(SAPP_KEYCODE_S).pressed) {
			cube.turn_queue.clear();
			auto scramble=cube.rubiks.getScramble();
			cube.turn_queue.insert(cube.turn_queue.end(),
				scramble.begin(), scramble.end()
			);
		}
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		cam.dir=polarToCartesian(cam.yaw, cam.pitch);

		handleCameraPlacement();

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
			cube.rubiks.turn(turn);

			cube.turn_queue.pop_front();
		}
	}
#pragma endregion

	void userUpdate(float dt) override {
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
			bind.samplers[SMP_b_skybox_smp]=smp_linear;
			bind.views[VIEW_b_skybox_tex]=skybox.tex[i];
			sg_apply_bindings(bind);

			mat4 mvp=mat4::mul(view_proj, skybox.model[i]);

			p_vs_skybox_t p_vs_skybox{};
			std::memcpy(p_vs_skybox.u_mvp, mvp.m, sizeof(mvp.m));
			sg_apply_uniforms(UB_p_vs_skybox, SG_RANGE(p_vs_skybox));

			sg_draw(0, 4, 1);
		}
	}

	void renderCube() {
		const auto& num=cube.rubiks.getNum();
		float side_st=.5f*(num-1);

		sg_bindings bind{};
		bind.vertex_buffers[0]=cube.facelet.vbuf;
		bind.index_buffer=cube.facelet.ibuf;
		bind.samplers[SMP_b_cube_smp]=smp_linear;
		bind.views[VIEW_b_cube_tex]=cube.facelet.tex;

		Turn* turn=nullptr;
		if(cube.turn_queue.size()) turn=&cube.turn_queue.front();

		mat4 rot_axes;
		if(turn) {
			float t=cube.turn_timer/cube.turn_time;
			t=cmn::ease::inOutBack(t);
			float angle=cmn::Pi/2*t;
			if(!turn->ccw) angle=-angle;
			switch(turn->axis) {
				case Turn::XAxis: rot_axes=mat4::makeRotX(angle); break;
				case Turn::YAxis: rot_axes=mat4::makeRotY(angle); break;
				case Turn::ZAxis: rot_axes=mat4::makeRotZ(angle); break;
			}
		} else rot_axes=mat4::makeIdentity();

		for(int i=0; i<num; i++) {
			float x=side_st-i;
			for(int j=0; j<num; j++) {
				float y=side_st-j;
				mat4 trans=mat4::makeTranslation({x, y, .5f*num});
				for(int f=0; f<6; f++) {
					int x, y, z;
					cube.rubiks.inv_ix(f, i, j, x, y, z);

					mat4 model=mat4::mul(cube.rot[f], trans);
					if(turn) {
						bool use_rot_axes=false;
						if(turn->slice==0) use_rot_axes=true;
						else {
							int slice=turn->slice<0?num+turn->slice:turn->slice-1;
							switch(turn->axis) {
								case Turn::XAxis: if(x==slice) use_rot_axes=true; break;
								case Turn::YAxis: if(y==slice) use_rot_axes=true; break;
								case Turn::ZAxis: if(z==slice) use_rot_axes=true; break;
							}
						}
						if(use_rot_axes) model=mat4::mul(rot_axes, model);
					}

					sg_apply_pipeline(cube.pip);

					sg_apply_bindings(bind);

					//send model & camera transforms
					mat4 mvp=mat4::mul(cam.view_proj, model);
					p_vs_cube_t p_vs_cube{};
					std::memcpy(p_vs_cube.u_model, model.m, sizeof(model.m));
					std::memcpy(p_vs_cube.u_mvp, mvp.m, sizeof(mvp.m));
					sg_apply_uniforms(UB_p_vs_cube, SG_RANGE(p_vs_cube));

					//send face col
					int col=cube.rubiks.grid[cube.rubiks.ix(f, i, j)];
					p_fs_cube_t p_fs_cube{};
					//light at cam
					p_fs_cube.u_light_pos[0]=cam.pos.x;
					p_fs_cube.u_light_pos[1]=cam.pos.y;
					p_fs_cube.u_light_pos[2]=cam.pos.z;
					p_fs_cube.u_eye_pos[0]=cam.pos.x;
					p_fs_cube.u_eye_pos[1]=cam.pos.y;
					p_fs_cube.u_eye_pos[2]=cam.pos.z;
					p_fs_cube.u_tint[0]=cube.cols[col].r;
					p_fs_cube.u_tint[1]=cube.cols[col].g;
					p_fs_cube.u_tint[2]=cube.cols[col].b;
					sg_apply_uniforms(UB_p_fs_cube, SG_RANGE(p_fs_cube));

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
		bind.samplers[SMP_b_colorview_smp]=smp_nearest;
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
			std::string ccw=t.ccw?"CW":"CCW";

			std::string axis;
			sg_color col{0, 0, 0, 0};
			switch(t.axis) {
				case Turn::XAxis: axis="X", col={1, 0, 0, 1}; break;
				case Turn::YAxis: axis="Y", col={0, 1, 0, 1}; break;
				case Turn::ZAxis: axis="Z", col={0, 0, 1, 1}; break;
			}

			auto slice=std::to_string(t.slice);

			auto str=ccw+" along "+axis+" at "+slice;
			renderString(m, y, str, scl, col);

			y+=m+scl*font.char_h;
		}
	}
#pragma endregion

	void userRender() override {
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