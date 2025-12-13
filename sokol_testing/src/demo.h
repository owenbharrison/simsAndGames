#define SOKOL_GLCORE
#include "sokol_engine.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"

#include "shd.glsl.h"

#include "math/mat4.h"

#include "mesh.h"

#include "texture.h"

//y p => x y z
//0 0 => 0 0 1
static vf3d polar3D(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

static float randFloat(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=std::rand()/rand_max;
	return a+t*(b-a);
}

//https://stackoverflow.com/a/6178290
static float randNormal() {
	float rho=std::sqrt(-2*std::log(randFloat()));
	float theta=2*Pi*randFloat();
	return rho*std::cos(theta);
}

//https://math.stackexchange.com/a/1585996
static vf3d randDir() {
	return vf3d(
		randNormal(),
		randNormal(),
		randNormal()
	).norm();
}

static vf3d projectOntoPlane(const vf3d& pt, const vf3d& norm) {
	return pt-norm.dot(pt)*norm;
}

struct Shape {
	Mesh mesh;
	sg_view tex;

	float radius;

	vf3d fwd;
	float speed;

	vf3d rot_speed;
};

class Demo : public SokolEngine {
	sg_pass offscreen_pass{};
	sg_pass_action display_pass_action{};
	sg_pipeline pipeline{};

	sg_sampler sampler{};

	sg_view tex_blank{};
	sg_view tex_uv{};
	sg_view tex_checker{};

	//cam info
	vf3d cam_pos{3, 3, 3};
	vf3d cam_dir;
	float cam_yaw=-3*Pi/4;
	float cam_pitch=-Pi/4;

	std::list<Shape> shapes;

	struct {
		sg_view tex_color{};
		sg_view tex_depth{};

		Mesh mesh;
		mat4 view;
	} cubemap_faces[6];

	mat4 face_proj;

public:
#pragma region SETUP HELPERS
	void setupEnvironment() {
		sg_desc desc{};
		desc.environment=sglue_environment();
		sg_setup(desc);
	}

	void setupDisplayPass() {
		//clear to bluish
		display_pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
		display_pass_action.colors[0].clear_value={0.25f, 0.45f, 0.65f, 1.0f};
	}

	void setupOffscreenPass() {
		offscreen_pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		offscreen_pass.action.colors[0].clear_value={0.25f, 0.25f, 0.25f, 1.0f};
	}

	//for now, these both use the same pipeline & shader
	void setupPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_shd_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_shd_v_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_shd_v_uv].format=SG_VERTEXFORMAT_SHORT2N;
		pip_desc.shader=sg_make_shader(shd_shader_desc(sg_query_backend()));
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode=SG_CULLMODE_FRONT;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.write_enabled=true;
		pipeline=sg_make_pipeline(pip_desc);
	}

	void setupSampler() {
		sg_sampler_desc sampler_desc{};
		sampler=sg_make_sampler(sampler_desc);
	}

	void setupTextures() {
		tex_blank=makeBlankTexture();

		tex_uv=makeUVTexture(1024, 1024);

		tex_checker=tex_blank;
		makeTextureFromFile(tex_checker, "assets/img/checker.png");
	}

	void setupShapes() {
		shapes.push_back({Mesh::makeCube(), tex_uv});
		shapes.push_back({Mesh::makeTorus(.7f, 24, .3f, 12), tex_checker});
		shapes.push_back({Mesh::makeUVSphere(1, 24, 12), tex_checker});
		shapes.push_back({Mesh::makeCylinder(1, 24, 1), tex_checker});
		shapes.push_back({Mesh::makeCone(1, 24, 1), tex_uv});

		for(auto& s:shapes) {
			//starting pos
			vf3d dir=randDir();
			s.radius=randFloat(2.5f, 3.5f);
			s.mesh.translation=s.radius*dir;
			s.mesh.scale=randFloat(.5f, .75f)*vf3d(1, 1, 1);

			vf3d fwd=randDir();
			s.fwd=projectOntoPlane(fwd, dir).norm();
			s.speed=randFloat(.5f, 1.5f);

			s.rot_speed.x=.5f-randFloat();
			s.rot_speed.y=.5f-randFloat();
			s.rot_speed.z=.5f-randFloat();
		}
	}

	void setupCubemap() {
		//make color textures
		for(int i=0; i<6; i++) {
			auto& f=cubemap_faces[i];

			sg_image_desc image_desc{};
			image_desc.usage.color_attachment=true;
			image_desc.width=1024;
			image_desc.height=1024;
			image_desc.pixel_format=SG_PIXELFORMAT_RGBA8;
			sg_image img_color=sg_make_image(image_desc);

			sg_view_desc view_desc{};
			view_desc.color_attachment.image=img_color;
			view_desc.texture.image=img_color;
			f.tex_color=sg_make_view(view_desc);
		}

		//make depth textures
		for(int i=0; i<6; i++) {
			auto& f=cubemap_faces[i];

			sg_image_desc image_desc{};
			image_desc.usage.depth_stencil_attachment=true;
			image_desc.width=1024;
			image_desc.height=1024;
			image_desc.pixel_format=SG_PIXELFORMAT_DEPTH;
			sg_image depth_img=sg_make_image(image_desc);

			sg_view_desc view_desc{};
			view_desc.depth_stencil_attachment.image=depth_img;
			f.tex_depth=sg_make_view(view_desc);
		}

		//orient meshes
		Mesh face;
		face.verts={
			{{-1, 0, -1}, {0, 1, 0}, {1, 1}},
			{{1, 0, -1}, {0, 1, 0}, {0, 1}},
			{{-1, 0, 1}, {0, 1, 0}, {1, 0}},
			{{1, 0, 1}, {0, 1, 0}, {0, 0}}
		};
		face.tris={
			{0, 2, 1},
			{1, 2, 3}
		};
		face.updateVertexBuffer();
		face.updateIndexBuffer();
		const vf3d rot_trans[6][2]{
			{Pi*vf3d(.5f, .5f, 0), {1, 0, 0}},
			{Pi*vf3d(0, 0, 0), {0, 1, 0}},
			{Pi*vf3d(.5f, 0, 0), {0, 0, 1}},
			{Pi*vf3d(.5f, -.5f, 0), {-1, 0, 0}},
			{Pi*vf3d(1, 0, 0), {0, -1, 0}},
			{Pi*vf3d(.5f, 1, 0), {0, 0, -1}}
		};
		for(int i=0; i<6; i++) {
			Mesh& m=cubemap_faces[i].mesh;
			m=face;
			m.rotation=rot_trans[i][0];
			m.translation=rot_trans[i][1];
			m.updateMatrixes();
		}

		//make view matrixes
		const vf3d dir_up[6][2]{
			{{1, 0, 0}, {0, 1, 0}},
			{{0, 1, 0}, {0, 0, -1}},
			{{0, 0, 1}, {0, 1, 0}},
			{{-1, 0, 0}, {0, 1, 0}},
			{{0, -1, 0}, {0, 0, 1}},
			{{0, 0, -1}, {0, 1, 0}}
		};
		for(int i=0; i<6; i++) {
			const auto& dir=dir_up[i][0];
			const auto& up=dir_up[i][1];
			mat4 look_at=mat4::makeLookAt({0, 0, 0}, dir, up);
			cubemap_faces[i].view=mat4::inverse(look_at);
		}

		//make projection matrix
		face_proj=mat4::makePerspective(90.f, 1, .001f, 1000.f);
	}
#pragma endregion

	void userCreate() override {
		std::srand(std::time(0));
		
		setupEnvironment();

		setupDisplayPass();

		setupOffscreenPass();

		setupPipeline();

		setupSampler();

		setupTextures();

		setupShapes();

		setupCubemap();
	}

#pragma region UPDATE HELPERS
	void handleCameraLooking(float dt) {
		//left/right
		if(getKey(SAPP_KEYCODE_LEFT).held) cam_yaw+=dt;
		if(getKey(SAPP_KEYCODE_RIGHT).held) cam_yaw-=dt;

		//up/down
		if(getKey(SAPP_KEYCODE_UP).held) cam_pitch+=dt;
		if(getKey(SAPP_KEYCODE_DOWN).held) cam_pitch-=dt;

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

		//polar to cartesian
		cam_dir=polar3D(cam_yaw, cam_pitch);

		handleCameraMovement(dt);
	}
#pragma endregion

	void userUpdate(float dt) {
		handleUserInput(dt);

		for(auto& s:shapes) {
			Mesh& m=s.mesh;

			//move by vel
			vf3d delta=s.speed*s.fwd*dt;
			//reproject pos & fwd onto sphere
			vf3d dir=(m.translation+delta).norm();
			m.translation=s.radius*dir;
			s.fwd=projectOntoPlane(s.fwd, dir).norm();

			m.rotation+=s.rot_speed*dt;

			m.updateMatrixes();
		}
	}

#pragma region RENDER HELPERS
	void renderIntoCubemap() {
		for(int i=0; i<6; i++) {
			auto& face=cubemap_faces[i];

			offscreen_pass.attachments.colors[0]=face.tex_color;
			offscreen_pass.attachments.depth_stencil=face.tex_depth;
			sg_begin_pass(offscreen_pass);

			sg_apply_pipeline(pipeline);

			const mat4& view=face.view;
			const mat4& proj=face_proj;

			mat4 view_proj=mat4::mul(proj, view);

			sg_bindings bind{};
			bind.samplers[SMP_u_smp]=sampler;
			for(const auto& s:shapes) {
				bind.vertex_buffers[0]=s.mesh.vbuf;
				bind.index_buffer=s.mesh.ibuf;
				bind.views[VIEW_u_tex]=s.tex;
				sg_apply_bindings(bind);

				vs_params_t vs_params{};
				mat4 mvp=mat4::mul(view_proj, s.mesh.model);
				std::memcpy(vs_params.u_mvp, mvp.m, sizeof(vs_params.u_mvp));
				sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

				sg_draw(0, 3*s.mesh.tris.size(), 1);
			}

			sg_end_pass();
		}
	}

	void renderToDisplay() {
		sg_pass pass{};
		pass.action=display_pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sg_apply_pipeline(pipeline);

		mat4 look_at=mat4::makeLookAt(cam_pos, cam_pos+cam_dir, {0, 1, 0});
		mat4 view=mat4::inverse(look_at);
		float asp=sapp_widthf()/sapp_heightf();
		mat4 proj=mat4::makePerspective(60, asp, .01f, 10.f);
		mat4 view_proj=mat4::mul(proj, view);

		sg_bindings bind{};
		bind.samplers[SMP_u_smp]=sampler;
		for(const auto& s:shapes) {
			bind.vertex_buffers[0]=s.mesh.vbuf;
			bind.index_buffer=s.mesh.ibuf;
			bind.views[VIEW_u_tex]=s.tex;
			sg_apply_bindings(bind);

			mat4 mvp=mat4::mul(view_proj, s.mesh.model);
			vs_params_t vs_params{};
			std::memcpy(vs_params.u_mvp, mvp.m, sizeof(vs_params.u_mvp));
			sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

			sg_draw(0, 3*s.mesh.tris.size(), 1);
		}

		for(int i=0; i<6; i++) {
			const auto& face=cubemap_faces[i];

			bind.vertex_buffers[0]=face.mesh.vbuf;
			bind.index_buffer=face.mesh.ibuf;
			bind.views[VIEW_u_tex]=face.tex_color;
			sg_apply_bindings(bind);

			mat4 mvp=mat4::mul(view_proj, face.mesh.model);
			vs_params_t vs_params{};
			std::memcpy(vs_params.u_mvp, mvp.m, sizeof(vs_params.u_mvp));
			sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

			sg_draw(0, 3*face.mesh.tris.size(), 1);
		}

		sg_end_pass();
	}
#pragma endregion

	void userRender() {
		renderIntoCubemap();

		renderToDisplay();

		sg_commit();
	}

	Demo() {
		app_title="Cubemap Demo";
	}
};