#define SOKOL_GLCORE
#include "sokol_engine.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"

#include "shd.glsl.h"

#include "shape.h"

//for time
#include <ctime>

#include <list>

//y p => x y z
//0 0 => 0 0 1
static vf3d polar3D(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

class Demo : public SokolEngine {
	sg_pass offscreen_pass{};
	sg_pass_action display_pass_action{};
	sg_pipeline default_pip{};

	sg_sampler sampler{};

	sg_view tex_blank{};
	sg_view tex_uv{};
	sg_view tex_checker{};

	//cam info
	vf3d cam_pos{3, 3, 3};
	vf3d cam_dir;
	float cam_yaw=-3*Pi/4;
	float cam_pitch=-Pi/4;
	mat4 cam_proj, cam_view;
	//view, then project
	mat4 cam_view_proj;

	struct {
		sg_pipeline pip{};

		Shape faces[6];
	} skybox;

	std::list<Shape> shapes;

	struct {
		struct {
			Shape shape;

			sg_view tex_depth{};

			mat4 view;
		} faces[6];

		mat4 face_proj;
	} cubemap;

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

	void setupDefaultPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_default_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_default_v_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_default_v_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(default_shader_desc(sg_query_backend()));
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode=SG_CULLMODE_FRONT;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.write_enabled=true;
		default_pip=sg_make_pipeline(pip_desc);
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

	void setupSkybox() {
		//pipeline
		{
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_skybox_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
			pip_desc.layout.attrs[ATTR_skybox_v_norm].format=SG_VERTEXFORMAT_FLOAT3;
			pip_desc.layout.attrs[ATTR_skybox_v_uv].format=SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader=sg_make_shader(skybox_shader_desc(sg_query_backend()));
			pip_desc.index_type=SG_INDEXTYPE_UINT32;
			skybox.pip=sg_make_pipeline(pip_desc);
		}
		
		const vf3d rot_trans[6][2]{
			{Pi*vf3d(.5f, -.5f, 0), {.5f, 0, 0}},
			{Pi*vf3d(.5f, .5f, 0), {-.5f, 0, 0}},
			{Pi*vf3d(0, 0, 1), {0, .5f, 0}},
			{Pi*vf3d(1, 0, 1), {0, -.5f, 0}},
			{Pi*vf3d(.5f, 1, 0), {0, 0, .5f}},
			{Pi*vf3d(.5f, 0, 0), {0, 0, -.5f}}
		};

		Mesh face_mesh;
		face_mesh.verts={
			{{-1, 0, -1}, {0, 1, 0}, {0, 0}},
			{{1, 0, -1}, {0, 1, 0}, {1, 0}},
			{{-1, 0, 1}, {0, 1, 0}, {0, 1}},
			{{1, 0, 1}, {0, 1, 0}, {1, 1}}
		};
		face_mesh.tris={
			{0, 2, 1},
			{1, 2, 3}
		};
		face_mesh.updateVertexBuffer();
		face_mesh.updateIndexBuffer();

		const std::string filenames[6]{
			"assets/img/skybox/px.png",
			"assets/img/skybox/nx.png",
			"assets/img/skybox/py.png",
			"assets/img/skybox/ny.png",
			"assets/img/skybox/pz.png",
			"assets/img/skybox/nz.png"
		};

		for(int i=0; i<6; i++) {
			auto& shape=skybox.faces[i];

			//orient meshes
			shape.mesh=face_mesh;
			shape.scale=.5f*vf3d(1, 1, 1);
			shape.rotation=rot_trans[i][0];
			shape.translation=rot_trans[i][1];
			shape.updateMatrixes();

			sg_view& tex=shape.tex;
			if(!makeTextureFromFile(tex, filenames[i]).valid) tex=tex_uv;
		}
	}

	void setupCubemap() {
		const vf3d rot_trans[6][2]{
			{Pi*vf3d(.5f, .5f, 0), {.5f, 0, 0}},
			{Pi*vf3d(0, 0, 0), {0, .5f, 0}},
			{Pi*vf3d(.5f, 0, 0), {0, 0, .5f}},
			{Pi*vf3d(.5f, -.5f, 0), {-.5f, 0, 0}},
			{Pi*vf3d(1, 0, 0), {0, -.5f, 0}},
			{Pi*vf3d(.5f, 1, 0), {0, 0, -.5f}}
		};

		const vf3d dir_up[6][2]{
			{{1, 0, 0}, {0, 1, 0}},
			{{0, 1, 0}, {0, 0, -1}},
			{{0, 0, 1}, {0, 1, 0}},
			{{-1, 0, 0}, {0, 1, 0}},
			{{0, -1, 0}, {0, 0, 1}},
			{{0, 0, -1}, {0, 1, 0}}
		};

		Mesh face_mesh;
		face_mesh.verts={
			{{-1, 0, -1}, {0, 1, 0}, {1, 1}},
			{{1, 0, -1}, {0, 1, 0}, {0, 1}},
			{{-1, 0, 1}, {0, 1, 0}, {1, 0}},
			{{1, 0, 1}, {0, 1, 0}, {0, 0}}
		};
		face_mesh.tris={
			{0, 2, 1},
			{1, 2, 3}
		};
		face_mesh.updateVertexBuffer();
		face_mesh.updateIndexBuffer();

		for(int i=0; i<6; i++) {
			auto& face=cubemap.faces[i];
			auto& shape=face.shape;

			//orient meshes
			shape.mesh=face_mesh;
			shape.scale=.5f*vf3d(1, 1, 1);
			shape.rotation=rot_trans[i][0];
			shape.translation=rot_trans[i][1];
			shape.updateMatrixes();

			//make color texture
			{
				sg_image_desc image_desc{};
				image_desc.usage.color_attachment=true;
				image_desc.width=1024;
				image_desc.height=1024;
				image_desc.pixel_format=SG_PIXELFORMAT_RGBA8;
				sg_image img_color=sg_make_image(image_desc);

				sg_view_desc view_desc{};
				view_desc.color_attachment.image=img_color;
				view_desc.texture.image=img_color;
				shape.tex=sg_make_view(view_desc);
			}

			//make depth texture
			{
				sg_image_desc image_desc{};
				image_desc.usage.depth_stencil_attachment=true;
				image_desc.width=1024;
				image_desc.height=1024;
				image_desc.pixel_format=SG_PIXELFORMAT_DEPTH;
				sg_image depth_img=sg_make_image(image_desc);

				sg_view_desc view_desc{};
				view_desc.depth_stencil_attachment.image=depth_img;
				face.tex_depth=sg_make_view(view_desc);
			}

			//make view matrix
			{
				const auto& dir=dir_up[i][0];
				const auto& up=dir_up[i][1];
				mat4 look_at=mat4::makeLookAt({0, 0, 0}, dir, up);
				cubemap.faces[i].view=mat4::inverse(look_at);
			}
		}

		//make projection matrix
		cubemap.face_proj=mat4::makePerspective(90.f, 1, .001f, 1000.f);
	}

	void setupPlatform() {
		Shape shp;
		shp.mesh=Mesh::makeCube();

		shp.scale={4, .5f, 4};
		shp.translation={0, -1.1f, 0};
		shp.updateMatrixes();

		shp.tex=tex_uv;

		shapes.push_back(shp);
	}

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
			Mesh m;
			auto status=Mesh::loadFromOBJ(m, f);
			if(!status.valid) m=Mesh::makeCube();
			shape_circle.push_back(Shape(m, tex_blank, true));
		}
		//i dont like using emplace_back
		shape_circle.push_back(Shape(Mesh::makeCube(), tex_uv, true));
		shape_circle.push_back(Shape(Mesh::makeTorus(.7f, 24, .3f, 12), tex_checker, true));
		shape_circle.push_back(Shape(Mesh::makeUVSphere(1, 24, 12), tex_checker, true));
		shape_circle.push_back(Shape(Mesh::makeCylinder(1, 24, 2), tex_checker, true));
		shape_circle.push_back(Shape(Mesh::makeCone(1, 24, 2), tex_checker, true));

		//fisher-yates shuffle
		for(int i=shape_circle.size()-1; i>=1; i--) {
			int j=std::rand()%(i+1);
			std::swap(shape_circle[i], shape_circle[j]);
		}

		//place shapes in circle pointing toward origin
		const float radius=3;
		for(int i=0; i<shape_circle.size(); i++) {
			auto& shp=shape_circle[i];
			float angle=2*Pi*i/shape_circle.size();
			shp.translation=radius*polar3D(angle, 0);
			shp.rotation={0, Pi+angle, 0};
			float scl=randFloat(.3f, .5f);
			shp.scale=scl*vf3d(1, 1, 1);
			shp.updateMatrixes();
		}

		shapes.insert(shapes.end(),
			shape_circle.begin(), shape_circle.end()
		);
	}
#pragma endregion

	void userCreate() override {
		std::srand(std::time(0));

		setupEnvironment();

		setupDisplayPass();

		setupOffscreenPass();

		setupDefaultPipeline();

		setupSampler();

		setupTextures();

		setupSkybox();

		setupCubemap();

		setupPlatform();

		setupShapes();
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

	void updateCameraMatrixes() {
		{
			mat4 look_at=mat4::makeLookAt(cam_pos, cam_pos+cam_dir, {0, 1, 0});
			cam_view=mat4::inverse(look_at);
		}

		//cam proj can change with window resize
		{
			float asp=sapp_widthf()/sapp_heightf();
			cam_proj=mat4::makePerspective(60, asp, .001f, 1000.f);
		}

		cam_view_proj=mat4::mul(cam_proj, cam_view);
	}
#pragma endregion

	void userUpdate(float dt) {
		handleUserInput(dt);

		updateCameraMatrixes();
	}

#pragma region RENDER HELPERS
	void renderShape(const Shape& s, const mat4& view_proj) {
		sg_bindings bind{};
		bind.samplers[SMP_default_smp]=sampler;
		bind.vertex_buffers[0]=s.mesh.vbuf;
		bind.index_buffer=s.mesh.ibuf;
		bind.views[VIEW_default_tex]=s.tex;
		sg_apply_bindings(bind);

		vs_params_t vs_params{};
		mat4 mvp=mat4::mul(view_proj, s.model);
		std::memcpy(vs_params.u_mvp, mvp.m, sizeof(vs_params.u_mvp));
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

		sg_draw(0, 3*s.mesh.tris.size(), 1);
	}
	
	void renderToCubemap() {
		const mat4& proj=cubemap.face_proj;

		for(int i=0; i<6; i++) {
			auto& face=cubemap.faces[i];

			offscreen_pass.attachments.colors[0]=face.shape.tex;
			offscreen_pass.attachments.depth_stencil=face.tex_depth;
			sg_begin_pass(offscreen_pass);

			sg_apply_pipeline(default_pip);

			const mat4& view=face.view;

			mat4 view_proj=mat4::mul(proj, view);

			for(const auto& s:shapes) {
				renderShape(s, view_proj);
			}

			sg_end_pass();
		}
	}

	void renderToDisplay() {
		sg_pass pass{};
		pass.action=display_pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		//render skybox
		{
			sg_apply_pipeline(skybox.pip);
			
			//imagine camera at origin!
			mat4 look_at=mat4::makeLookAt({0, 0, 0}, cam_dir, {0, 1, 0});
			mat4 view=mat4::inverse(look_at);
			mat4 view_proj=mat4::mul(cam_proj, view);

			for(int i=0; i<6; i++) {
				renderShape(skybox.faces[i], view_proj);
			}
		}

		//render shapes
		{
			sg_apply_pipeline(default_pip);

			for(const auto& s:shapes) {
				renderShape(s, cam_view_proj);
			}

			//render cubemap
			for(int i=0; i<6; i++) {
				renderShape(cubemap.faces[i].shape, cam_view_proj);
			}
		}

		sg_end_pass();
	}

#pragma endregion
	
	void userRender() {
		renderToCubemap();

		renderToDisplay();

		sg_commit();
	}

	Demo() {
		app_title="Cubemap Demo";
	}
};