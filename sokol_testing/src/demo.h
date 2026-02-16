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

#include "shape.h"

//for time
#include <ctime>

//list for pointer keeping
//  b/c vector reallocates
#include <list>

#include "sokol/font.h"

#include "gizmo.h"

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

//coordinate system transform
static mat4 makeTransformMatrix(const vf3d& x, const vf3d& y, const vf3d& z, const vf3d& t) {
	mat4 m;
	m(0, 0)=x.x, m(0, 1)=y.x, m(0, 2)=z.x, m(0, 3)=t.x;
	m(1, 0)=x.y, m(1, 1)=y.y, m(1, 2)=z.y, m(1, 3)=t.y;
	m(2, 0)=x.z, m(2, 1)=y.z, m(2, 2)=z.z, m(2, 3)=t.z;
	m(3, 3)=1;
	return m;
}

class Demo : public cmn::SokolEngine {
	struct {
		sg_sampler linear{};
		sg_sampler nearest{};
	} samplers;

	struct {
		sg_view blank{};
		sg_view uv{};
		sg_view checker{};
	} textures;

	sg_pipeline mesh_pip{};

	sg_pipeline line_pip{};

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};
	} colorview;

	struct {
		cmn::Font fancy;
		cmn::Font monogram;
		cmn::Font round;
	} fonts;

	LineMesh arrow_linemesh;

	struct {
		sg_pass_action pass_action{};
		sg_pipeline attach_pip{};
		
		sg_view color_attach[6];
		sg_view depth_attach{};
		sg_view color_view{};

		vf3d pos{0, 2, 0};

		//only works for GL...
		const vf3d face_sys[6][3]{
			{{0, 0, -1}, {0, -1, 0}, {-1, 0, 0}},//px
			{{0, 0, 1}, {0, -1, 0}, {1, 0, 0}},//nx
			{{1, 0, 0}, {0, 0, 1}, {0, -1, 0}},//py
			{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},//ny
			{{1, 0, 0}, {0, -1, 0}, {0, 0, -1}},//px
			{{-1, 0, 0}, {0, -1, 0}, {0, 0, 1}}//nz
		};

		mat4 view[6];

		mat4 face_proj;
	} shadow_map;

	sg_pass_action display_pass_action{};

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};

		mat4 model[6];

		sg_view tex[6];
	} skybox;

	bool render_outlines=false;
	std::list<Shape> shapes;

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};
		
		std::unordered_map<Shape*, Gizmo> map;
	} gizmo;

	struct {
		vf3d pos;
		float yaw=0, pitch=0;
		vf3d dir;

		const float near=.001f, far=1000;
		mat4 proj;
		
		mat4 view;
		
		mat4 view_proj;
	} cam;

	vf3d mouse_dir;
	vf3d prev_mouse_dir;
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
			samplers.linear=sg_make_sampler(sampler_desc);
		}

		{
			sg_sampler_desc sampler_desc{};
			sampler_desc.min_filter=SG_FILTER_NEAREST;
			sampler_desc.mag_filter=SG_FILTER_NEAREST;
			samplers.nearest=sg_make_sampler(sampler_desc);
		}
	}

	//"primitive" textures to work with
	void setupTextures() {
		auto& b=textures.blank;
		b=cmn::makeBlankTexture();

		textures.uv=cmn::makeUVTexture(1024, 1024);

		auto& c=textures.checker;
		if(!cmn::makeTextureFromFile(c, "assets/img/checker.png")) c=b;
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

	void setupFonts() {
		fonts.fancy=cmn::Font("assets/img/font/fancy_8x16.png", 8, 16);
		fonts.monogram=cmn::Font("assets/img/font/monogram_6x9.png", 6, 9);
		fonts.round=cmn::Font("assets/img/font/round_6x6.png", 6, 6);
	}

	void setupArrowLinemesh() {
		sg_color white{1, 1, 1, 1};
		arrow_linemesh.verts={
			{{0, 0, 0}, white},
			{{.8f, 0, 0}, white},
			{{.8f, 0, -.1f}, white},
			{{1, 0, 0}, white},
			{{.8f, 0, .1f}, white}
		};
		arrow_linemesh.updateVertexBuffer();

		arrow_linemesh.lines={
			{0, 1},
			{1, 2},
			{2, 3},
			{3, 4},
			{4, 1}
		};
		arrow_linemesh.updateIndexBuffer();
	}

	//make pipeline, make render targets, & orient view matrixes
	void setupShadowMap() {
		//clear to black
		shadow_map.pass_action.depth.load_action=SG_LOADACTION_CLEAR;
		shadow_map.pass_action.depth.clear_value=1;
		shadow_map.pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
		shadow_map.pass_action.colors[0].clear_value={0, 0, 0, 1};

		{
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_shadow_map_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
			pip_desc.layout.attrs[ATTR_shadow_map_i_norm].format=SG_VERTEXFORMAT_FLOAT3;
			pip_desc.layout.attrs[ATTR_shadow_map_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader=sg_make_shader(shadow_map_shader_desc(sg_query_backend()));
			pip_desc.index_type=SG_INDEXTYPE_UINT32;
			pip_desc.face_winding=SG_FACEWINDING_CCW;
			pip_desc.cull_mode=SG_CULLMODE_BACK;
			pip_desc.depth.write_enabled=true;
			pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
			pip_desc.depth.pixel_format=SG_PIXELFORMAT_DEPTH;
			shadow_map.attach_pip=sg_make_pipeline(pip_desc);
		}

		//cube attach and view
		{
			sg_image_desc image_desc{};
			image_desc.type=SG_IMAGETYPE_CUBE;
			image_desc.usage.color_attachment=true;
			image_desc.width=2048;
			image_desc.height=2048;
			sg_image cube_img=sg_make_image(image_desc);
			
			for(int i=0; i<6; i++) {
				sg_view_desc view_desc{};
				view_desc.color_attachment.image=cube_img;
				view_desc.color_attachment.slice=i;
				shadow_map.color_attach[i]=sg_make_view(view_desc);
			}

			sg_view_desc view_desc{};
			view_desc.texture.image=cube_img;
			shadow_map.color_view=sg_make_view(view_desc);
		}

		{
			//make depth attachment
			sg_image_desc image_desc{};
			image_desc.usage.depth_stencil_attachment=true;
			image_desc.width=2048;
			image_desc.height=2048;
			image_desc.pixel_format=SG_PIXELFORMAT_DEPTH;
			sg_image depth_img=sg_make_image(image_desc);
			sg_view_desc view_desc{};
			view_desc.depth_stencil_attachment.image=depth_img;
			shadow_map.depth_attach=sg_make_view(view_desc);
		}

		//make projection matrix
		shadow_map.face_proj=mat4::makePerspective(90.f, 1, cam.near, cam.far);
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
			if(!cmn::makeTextureFromFile(tex, filenames[i])) tex=textures.blank;
		}
	}

	//scaled cube
	void setupPlatform() {
		Shape shp(Mesh::makeCube(), textures.uv, false);
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
			//default to cube
			if(status!=Mesh::ReturnCode::Ok) msh=Mesh::makeCube();
			shape_circle.push_back(Shape(msh, textures.blank, true));
		}

		//add solids of revolution(i dont like using emplace)
		shape_circle.push_back(Shape(Mesh::makeCube(), textures.uv, true));
		shape_circle.push_back(Shape(Mesh::makeTorus(.7f, 25, .3f, 13), textures.checker, true));
		shape_circle.push_back(Shape(Mesh::makeUVSphere(1, 25, 13), textures.checker, true));
		shape_circle.push_back(Shape(Mesh::makeCylinder(1, 25, 2), textures.checker, true));
		shape_circle.push_back(Shape(Mesh::makeCone(1, 25, 2), textures.checker, true));

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

	void setupGizmo() {
		//basecol pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_basecol_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.shader=sg_make_shader(basecol_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		gizmo.pip=sg_make_pipeline(pip_desc);

		//quad vertex buffer
		float vertexes[4][3]{
			{0, 0, 0},
			{1, 0, 0},
			{0, 0, 1},
			{1, 0, 1}
		};
		sg_buffer_desc buffer_desc{};
		buffer_desc.data=SG_RANGE(vertexes);
		gizmo.vbuf=sg_make_buffer(buffer_desc);
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

		setupSamplers();

		setupTextures();

		setupMeshPipeline();

		setupLinePipeline();

		setupColorview();
		setupFonts();

		setupArrowLinemesh();

		setupShadowMap();
		
		setupDisplayPassAction();

		setupSkybox();

		setupPlatform();
		setupShapes();

		setupGizmo();

		setupCamera();
	}

#pragma region UPDATE HELPERS
	bool isGizmoing() const {
		for(const auto& sg:gizmo.map) {
			if(sg.second.mode!=Gizmo::Mode::None) return true;
		}
		return false;
	}
	
	void handleCameraLooking(float dt) {
		//dont look while gizmoing
		if(isGizmoing()) return;
		
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
		//dont move while gizmoing
		if(isGizmoing()) return;

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

	void handleGizmoSelection(){
		const auto action=getKey(SAPP_KEYCODE_G);
		if(action.pressed) {
			//get closest shape
			float record=-1;
			Shape* shp=nullptr;
			for(auto& s:shapes) {
				if(!s.draggable) continue;

				float d=s.intersectRay(cam.pos, mouse_dir);
				if(d>0) {
					if(record<0||d<record) {
						record=d;
						shp=&s;
					}
				}
			}

			if(shp) {
				//remove if exists
				auto it=gizmo.map.find(shp);
				if(it!=gizmo.map.end()) gizmo.map.erase(it);
				//else add it
				else gizmo.map.emplace(shp, Gizmo());
			}
		}
	}

	void handleGizmoMovement() {
		auto action=getMouse(SAPP_MOUSEBUTTON_LEFT);
		for(auto& sg:gizmo.map) {
			auto& shp=sg.first;
			auto& giz=sg.second;
			if(action.pressed) giz.beginDrag(
				shp->translation,
				cam.pos, cam.pos+cam.far*mouse_dir
			);
			if(action.held) {
				giz.updateDrag(
					shp->translation,
					cam.pos, mouse_dir, prev_mouse_dir
				);
				shp->updateMatrixes();
			}
			if(action.released) giz.endDrag();
		}
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		cam.dir=polarToCartesian(cam.yaw, cam.pitch);

		handleCameraMovement(dt);

		handleGizmoSelection();

		handleGizmoMovement();

		//look at origin
		if(getKey(SAPP_KEYCODE_HOME).held) cartesianToPolar(-cam.pos, cam.yaw, cam.pitch);

		//set light pos
		if(getKey(SAPP_KEYCODE_L).pressed) shadow_map.pos=cam.pos;

		//toggle shape outlines
		if(getKey(SAPP_KEYCODE_O).pressed) render_outlines^=true;
	}

	void updateCameraMatrixes() {
		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		//cam proj can change with window resize
		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(60, asp, cam.near, cam.far);

		cam.view_proj=mat4::mul(cam.proj, cam.view);
	}

	//update faces' view matrix w/ shadow_map pos
	void updateShadowMapMatrixes() {
		for(int i=0; i<6; i++) {
			const auto& x=shadow_map.face_sys[i][0];
			const auto& y=shadow_map.face_sys[i][1];
			const auto& z=shadow_map.face_sys[i][2];
			mat4 sys=makeTransformMatrix(x, y, z, shadow_map.pos);
			shadow_map.view[i]=mat4::inverse(sys);
		}
	}

	void updateMouseRay() {
		prev_mouse_dir=mouse_dir;

		//unprojection matrix
		mat4 m_inv_view_proj=mat4::inverse(cam.view_proj);

		//mouse coords from clip -> world
		float ndc_x=2*mouse_x/sapp_widthf()-1;
		float ndc_y=1-2*mouse_y/sapp_heightf();
		vf3d clip(ndc_x, ndc_y, 1);
		float w=1;
		vf3d world=matMulVec(m_inv_view_proj, clip, w);
		world/=w;

		//normalize direction
		mouse_dir=(world-cam.pos).norm();
	}
#pragma endregion

	void userUpdate(float dt) override {
		handleUserInput(dt);

		updateCameraMatrixes();

		updateMouseRay();

		updateShadowMapMatrixes();
	}

#pragma region RENDER HELPERS
	void renderLinemesh(const LineMesh& l, const mat4& model, const sg_color& col) {
		sg_apply_pipeline(line_pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=l.vbuf;
		bind.index_buffer=l.ibuf;
		sg_apply_bindings(bind);

		vs_line_params_t vs_line_params{};
		mat4 mvp=mat4::mul(cam.view_proj, model);
		std::memcpy(vs_line_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_line_params, SG_RANGE(vs_line_params));

		fs_line_params_t fs_line_params{};
		fs_line_params.u_tint[0]=col.r;
		fs_line_params.u_tint[1]=col.g;
		fs_line_params.u_tint[2]=col.b;
		fs_line_params.u_tint[3]=col.a;
		sg_apply_uniforms(UB_fs_line_params, SG_RANGE(fs_line_params));

		sg_draw(0, 2*l.lines.size(), 1);
	}

	void renderArrow(const vf3d& a, const vf3d& b, const sg_color& col) {
		vf3d sub=b-a;
		float mag=sub.mag();

		//unit axes
		vf3d ux=sub/mag;
		//"point at" camera
		vf3d uz=ux.cross(cam.pos-a).norm();
		vf3d uy=uz.cross(ux);

		//scaled
		vf3d x=mag*ux;
		vf3d y=mag*uy;
		vf3d z=mag*uz;

		mat4 model=makeTransformMatrix(x, y, z, a);
		renderLinemesh(arrow_linemesh, model, col);
	}

	void renderTex(float x, float y, float w, float h,
		const sg_view& tex, float l, float t, float r, float b,
		const sg_color& tint
	) {
		sg_apply_pipeline(colorview.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=colorview.vbuf;
		bind.samplers[SMP_u_colorview_smp]=samplers.nearest;
		bind.views[VIEW_u_colorview_tex]=tex;
		sg_apply_bindings(bind);

		vs_colorview_params_t vs_colorview_params{};
		vs_colorview_params.u_tl[0]=l;
		vs_colorview_params.u_tl[1]=t,
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
	void renderShapesIntoShadowMap() {
		for(int i=0; i<6; i++) {
			sg_pass pass{};
			pass.action=shadow_map.pass_action;
			pass.attachments.colors[0]=shadow_map.color_attach[i];
			pass.attachments.depth_stencil=shadow_map.depth_attach;
			sg_begin_pass(pass);

			mat4 view_proj=mat4::mul(shadow_map.face_proj, shadow_map.view[i]);
			for(const auto& shp:shapes) {
				sg_apply_pipeline(shadow_map.attach_pip);

				sg_bindings bind{};
				bind.vertex_buffers[0]=shp.mesh.vbuf;
				bind.index_buffer=shp.mesh.ibuf;
				sg_apply_bindings(bind);

				mat4 mvp=mat4::mul(view_proj, shp.model);

				vs_shadow_map_params_t vs_shadow_map_params{};
				std::memcpy(vs_shadow_map_params.u_model, shp.model.m, sizeof(shp.model.m));
				std::memcpy(vs_shadow_map_params.u_mvp, mvp.m, sizeof(mvp.m));
				sg_apply_uniforms(UB_vs_shadow_map_params, SG_RANGE(vs_shadow_map_params));

				fs_shadow_map_params_t fs_shadow_map_params{};
				fs_shadow_map_params.u_light_pos[0]=shadow_map.pos.x;
				fs_shadow_map_params.u_light_pos[1]=shadow_map.pos.y;
				fs_shadow_map_params.u_light_pos[2]=shadow_map.pos.z;
				fs_shadow_map_params.u_cam_near=cam.near;
				fs_shadow_map_params.u_cam_far=cam.far;
				sg_apply_uniforms(UB_fs_shadow_map_params, SG_RANGE(fs_shadow_map_params));

				sg_draw(0, 3*shp.mesh.tris.size(), 1);
			}

			sg_end_pass();
		}
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
			bind.samplers[SMP_u_skybox_smp]=samplers.linear;
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
			bind.samplers[SMP_u_mesh_smp]=samplers.linear;
			bind.views[VIEW_u_mesh_tex]=s.tex;
			bind.samplers[SMP_u_mesh_shadow_smp]=samplers.nearest;
			bind.views[VIEW_u_mesh_shadow_tex]=shadow_map.color_view;
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
			fs_mesh_params.u_light_pos[0]=shadow_map.pos.x;
			fs_mesh_params.u_light_pos[1]=shadow_map.pos.y;
			fs_mesh_params.u_light_pos[2]=shadow_map.pos.z;
			fs_mesh_params.u_cam_near=cam.near;
			fs_mesh_params.u_cam_far=cam.far;
			sg_apply_uniforms(UB_fs_mesh_params, SG_RANGE(fs_mesh_params));

			sg_draw(0, 3*s.mesh.tris.size(), 1);
		}
	}

	void renderShapeOutlines() {
		for(const auto& s:shapes) {
			renderLinemesh(s.linemesh, s.model, {1, 1, 1, 1});
		}
	}

	void renderGizmos() {
		auto renderQuad=[&] (const vf3d& pos, const vf3d& ux, const vf3d& uz, float scl, const sg_color& col) {
			sg_apply_pipeline(gizmo.pip);

			sg_bindings bind{};
			bind.vertex_buffers[0]=gizmo.vbuf;
			sg_apply_bindings(bind);

			vs_basecol_params_t vs_basecol_params{};
			vf3d uy=uz.cross(ux).norm();
			mat4 model=makeTransformMatrix(scl*ux, scl*uy, scl*uz, pos);
			mat4 mvp=mat4::mul(cam.view_proj, model);
			std::memcpy(vs_basecol_params.u_mvp, mvp.m, sizeof(mvp.m));
			sg_apply_uniforms(UB_vs_basecol_params, SG_RANGE(vs_basecol_params));

			fs_basecol_params_t fs_basecol_params{};
			fs_basecol_params.u_tint[0]=col.r;
			fs_basecol_params.u_tint[1]=col.g;
			fs_basecol_params.u_tint[2]=col.b;
			fs_basecol_params.u_tint[3]=col.a;
			sg_apply_uniforms(UB_fs_basecol_params, SG_RANGE(fs_basecol_params));

			sg_draw(0, 4, 1);
		};
		
		const vf3d x(1, 0, 0);
		const vf3d y(0, 0, 1);
		const vf3d z(0, 1, 0);

		for(const auto& sg:gizmo.map) {
			const auto& shp=sg.first;
			const auto& giz=sg.second;

			const auto& p=shp->translation;

			//axis handles
			renderArrow(p, p+Gizmo::axis_size*x, {1, 0, 0, 1});
			renderArrow(p, p+Gizmo::axis_size*y, {0, 1, 0, 1});
			renderArrow(p, p+Gizmo::axis_size*z, {0, 0, 1, 1});

			//plane handles
			renderQuad(p+Gizmo::margin*(x+y), x, y, Gizmo::plane_size, {1, 1, 0, 1});
			renderQuad(p+Gizmo::margin*(y+z), y, z, Gizmo::plane_size, {0, 1, 1, 1});
			renderQuad(p+Gizmo::margin*(z+x), z, x, Gizmo::plane_size, {1, 0, 1, 1});
		}
	}

#pragma endregion

	void userRender() override {
		renderShapesIntoShadowMap();
		
		//start display pass 
		sg_pass pass{};
		pass.action=display_pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		renderSkybox();

		if(render_outlines) renderShapeOutlines();
		else renderShapes();

		renderGizmos();

		sg_end_pass();

		sg_commit();
	}

	Demo() {
		app_title="Shadowmap Demo";
	}
};