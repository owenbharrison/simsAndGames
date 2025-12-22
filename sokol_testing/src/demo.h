#define SOKOL_GLCORE
#include "sokol_engine.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"

#include "shd.glsl.h"

#include "shape.h"

//for time
#include <ctime>

//list for pointer keeping
//  b/c vector reallocates
#include <list>

#include <set>

//y p => x y z
//0 0 => 0 0 1
static vf3d polar3D(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

#include "texture_utils.h"

class Demo : public SokolEngine {
	sg_sampler sampler{};

	sg_view tex_blank{};
	sg_view tex_uv{};
	sg_view tex_checker{};

	struct {
		sg_pass_action pass_action{};
		sg_pipeline pip{};

		vf3d pos;

		struct {
			sg_view tex_color_attach{};
			sg_view tex_color_view{};

			sg_view tex_depth_attach{};
			sg_view tex_depth_view{};

			vf3d dir, up;

			mat4 view;
		} faces[6];

		mat4 face_proj;
	} shadow_map;

	sg_pass_action display_pass_action{};

	struct {
		vf3d pos{3, 3, 3};
		vf3d dir;
		float yaw=-3*Pi/4;
		float pitch=-Pi/4;
		mat4 proj, view;
		//view, then project
		mat4 view_proj;
	} cam;

	struct {
		sg_pipeline pip{};

		Shape faces[6];
	} skybox;

	sg_pipeline default_pip{};

	sg_pipeline line_pip{};

	struct {
		sg_pipeline pip{};
		sg_bindings bind{};
	} texview;

	bool render_outlines=false;
	std::list<Shape> shapes;

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

	//"primitive" textures to work with
	void setupTextures() {
		tex_blank=makeBlankTexture();

		tex_uv=makeUVTexture(1024, 1024);

		tex_checker=tex_blank;
		makeTextureFromFile(tex_checker, "assets/img/checker.png");
	}

	//make pipeline, make render targets, & orient view matrixes
	void setupShadowMap() {
		//clear to black
		shadow_map.pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
		shadow_map.pass_action.colors[0].clear_value={0, 0, 0, 1};

		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_shadow_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_shadow_v_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_shadow_v_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(shadow_shader_desc(sg_query_backend()));
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode=SG_CULLMODE_FRONT;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.pixel_format=SG_PIXELFORMAT_DEPTH;
		shadow_map.pip=sg_make_pipeline(pip_desc);
		
		const vf3d dir_up[6][2]{
			{{1, 0, 0}, {0, 1, 0}},
			{{0, 1, 0}, {0, 0, -1}},
			{{0, 0, 1}, {0, 1, 0}},
			{{-1, 0, 0}, {0, 1, 0}},
			{{0, -1, 0}, {0, 0, 1}},
			{{0, 0, -1}, {0, 1, 0}}
		};

		//for each of the 6 faces...
		for(int i=0; i<6; i++) {
			auto& face=shadow_map.faces[i];

			//make color texture
			{
				sg_image_desc image_desc{};
				image_desc.usage.color_attachment=true;
				image_desc.width=1024;
				image_desc.height=1024;
				sg_image img_color=sg_make_image(image_desc);

				{
					sg_view_desc view_desc{};
					view_desc.color_attachment.image=img_color;
					face.tex_color_attach=sg_make_view(view_desc);
				}

				{
					sg_view_desc view_desc{};
					view_desc.texture.image=img_color;
					face.tex_color_view=sg_make_view(view_desc);
				}
			}

			//make depth texture
			{
				sg_image_desc image_desc{};
				image_desc.usage.depth_stencil_attachment=true;
				image_desc.width=1024;
				image_desc.height=1024;
				image_desc.pixel_format=SG_PIXELFORMAT_DEPTH;
				sg_image depth_img=sg_make_image(image_desc);
				
				{
					sg_view_desc view_desc{};
					view_desc.depth_stencil_attachment.image=depth_img;
					face.tex_depth_attach=sg_make_view(view_desc);
				}
				
				{
					sg_view_desc view_desc{};
					view_desc.texture.image=depth_img;
					face.tex_depth_view=sg_make_view(view_desc);
				}
			}

			//setup face orientations
			face.dir=dir_up[i][0];
			face.up=dir_up[i][1];
		}

		//make projection matrix
		shadow_map.face_proj=mat4::makePerspective(90.f, 1, .001f, 1000.f);
	}

	//make pipeline, orient meshes, & load textures 
	void setupSkybox() {
		//pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_skybox_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_skybox_v_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_skybox_v_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(skybox_shader_desc(sg_query_backend()));
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		skybox.pip=sg_make_pipeline(pip_desc);

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

	//clear to bluish
	void setupDisplayPassAction() {
		display_pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
		display_pass_action.colors[0].clear_value={.25f, .45f, .65f, 1.f};
	}

	//works with mesh
	void setupDefaultPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_default_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_default_v_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_default_v_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(default_shader_desc(sg_query_backend()));
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode=SG_CULLMODE_FRONT;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		default_pip=sg_make_pipeline(pip_desc);
	}

	//works with linemesh
	void setupLinePipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_line_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_line_v_col].format=SG_VERTEXFORMAT_FLOAT4;
		pip_desc.shader=sg_make_shader(line_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_LINES;
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		line_pip=sg_make_pipeline(pip_desc);
	}

	//2d tristrip
	void setupTexviewPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_texview_v_pos].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_texview_v_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(texview_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		texview.pip=sg_make_pipeline(pip_desc);
	}

	//just a quad
	void setupTexviewBindings() {
		//vertex buffer: xyuv
		float vertexes[4][2][2]{
			{{-1, -1}, {0, 0}},//tl
			{{1, -1}, {1, 0}},//tr
			{{-1, 1}, {0, 1}},//bl
			{{1, 1}, {1, 1}}//br
		};
		sg_buffer_desc vbuf_desc{};
		vbuf_desc.data.ptr=vertexes;
		vbuf_desc.data.size=sizeof(vertexes);
		texview.bind.vertex_buffers[0]=sg_make_buffer(vbuf_desc);

		//sampler
		texview.bind.samplers[SMP_texview_smp]=sampler;
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

		setupSampler();
		setupTextures();

		setupShadowMap();

		setupSkybox();

		setupLinePipeline();

		setupDisplayPassAction();

		setupDefaultPipeline();

		setupTexviewPipeline();
		setupTexviewBindings();


		setupPlatform();
		setupShapes();
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
		if(cam.pitch>Pi/2) cam.pitch=Pi/2-.001f;
		if(cam.pitch<-Pi/2) cam.pitch=.001f-Pi/2;
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

		//polar to cartesian
		cam.dir=polar3D(cam.yaw, cam.pitch);

		handleCameraMovement(dt);

		//toggle outlines
		if(getKey(SAPP_KEYCODE_O).pressed) render_outlines^=true;

		//set shadow map position with L
		if(getKey(SAPP_KEYCODE_L).held) shadow_map.pos=cam.pos;
	}

	void updateCameraMatrixes() {
		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		//cam proj can change with window resize
		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(60, asp, .001f, 1000.f);

		cam.view_proj=mat4::mul(cam.proj, cam.view);
	}

	//update faces' view matrix w/ shadow map pos
	void updateShadowMapMatrixes() {
		for(int i=0; i<6; i++) {
			auto& face=shadow_map.faces[i];
			mat4 look_at=mat4::makeLookAt(shadow_map.pos, shadow_map.pos+face.dir, face.up);
			face.view=mat4::inverse(look_at);
		}
	}
#pragma endregion

	void userUpdate(float dt) {
		handleUserInput(dt);

		updateCameraMatrixes();

		updateShadowMapMatrixes();
	}

#pragma region RENDER HELPERS
	void renderShapesIntoShadowMap() {
		const mat4& proj=shadow_map.face_proj;

		for(int i=0; i<6; i++) {
			auto& face=shadow_map.faces[i];

			sg_pass pass{};
			pass.action=shadow_map.pass_action;
			pass.attachments.colors[0]=face.tex_color_attach;
			pass.attachments.depth_stencil=face.tex_depth_attach;
			sg_begin_pass(pass);

			mat4 view_proj=mat4::mul(proj, face.view);
			for(const auto& s:shapes) {
				sg_apply_pipeline(shadow_map.pip);

				sg_bindings bind{};
				bind.vertex_buffers[0]=s.mesh.vbuf;
				bind.index_buffer=s.mesh.ibuf;
				sg_apply_bindings(bind);

				vs_params_t vs_params{};
				mat4 mvp=mat4::mul(view_proj, s.model);
				std::memcpy(vs_params.u_mvp, mvp.m, sizeof(vs_params.u_mvp));
				sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

				sg_draw(0, 3*s.mesh.tris.size(), 1);
			}

			sg_end_pass();
		}
	}

	void renderSkybox() {
		//imagine camera at origin!
		mat4 look_at=mat4::makeLookAt({0, 0, 0}, cam.dir, {0, 1, 0});
		mat4 view=mat4::inverse(look_at);
		mat4 view_proj=mat4::mul(cam.proj, view);

		for(int i=0; i<6; i++) {
			auto& shp=skybox.faces[i];
			
			sg_apply_pipeline(skybox.pip);

			sg_bindings bind{};
			bind.samplers[SMP_skybox_smp]=sampler;
			bind.vertex_buffers[0]=shp.mesh.vbuf;
			bind.index_buffer=shp.mesh.ibuf;
			bind.views[VIEW_skybox_tex]=shp.tex;
			sg_apply_bindings(bind);

			vs_params_t vs_params{};
			mat4 mvp=mat4::mul(view_proj, shp.model);
			std::memcpy(vs_params.u_mvp, mvp.m, sizeof(vs_params.u_mvp));
			sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

			sg_draw(0, 3*shp.mesh.tris.size(), 1);
		}
	}

	void renderShapes() {
		for(const auto& s:shapes) {
			sg_apply_pipeline(default_pip);

			sg_bindings bind{};
			bind.samplers[SMP_default_smp]=sampler;
			bind.vertex_buffers[0]=s.mesh.vbuf;
			bind.index_buffer=s.mesh.ibuf;
			bind.views[VIEW_default_tex]=s.tex;
			sg_apply_bindings(bind);

			vs_params_t vs_params{};
			mat4 mvp=mat4::mul(cam.view_proj, s.model);
			std::memcpy(vs_params.u_mvp, mvp.m, sizeof(vs_params.u_mvp));
			sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

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

			vs_params_t vs_params{};
			mat4 mvp=mat4::mul(cam.view_proj, s.model);
			std::memcpy(vs_params.u_mvp, mvp.m, sizeof(vs_params.u_mvp));
			sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

			sg_draw(0, 2*s.linemesh.lines.size(), 1);
		}
	}

	//show cubemap renders on top left
	void renderCubemap() {
		const int tile_size=64;
		const int idx[6][2]{
			{0, 0}, {1, 0}, {2, 0},
			{0, 1}, {1, 1}, {2, 1}
		};
		for(int i=0; i<6; i++) {
			sg_apply_pipeline(texview.pip);

			texview.bind.views[VIEW_texview_tex]=shadow_map.faces[i].tex_color_view;
			sg_apply_bindings(texview.bind);

			int x=tile_size*idx[i][0];
			int y=tile_size*idx[i][1];
			sg_apply_viewport(x, y, tile_size, tile_size, true);

			//4 verts = 1quad
			sg_draw(0, 4, 1);
		}
	}
#pragma endregion

	void userRender() {
		renderShapesIntoShadowMap();

		//start display pass 
		sg_pass pass{};
		pass.action=display_pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		renderSkybox();

		if(render_outlines) renderShapeOutlines();
		renderShapes();

		renderCubemap();

		sg_end_pass();

		sg_commit();
	}

	Demo() {
		app_title="Cubemap Demo";
	}
};