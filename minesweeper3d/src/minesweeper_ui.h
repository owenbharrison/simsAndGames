/*todo:
win/lose particles?
difficulty modes
sound
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

//for time
#include <ctime>

#include "cmn/math/v3d.h"
#include "cmn/math/mat4.h"

#include "minesweeper.h"

#include "cmn/utils.h"

#include <algorithm>

#include "mesh.h"

#include "cmn/geom/aabb3.h"

#include "particle.h"

#include "billboard.h"

#include "sokol/font.h"

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

sg_color mixCol(const sg_color& a, const sg_color& b, float t) {
	return {
		a.r+t*(b.r-a.r),
		a.g+t*(b.g-a.g),
		a.b+t*(b.b-a.b),
		a.a+t*(b.a-a.a)
	};
}

vf3d randDir() {
	return vf3d(
		.5f-cmn::randFloat(),
		.5f-cmn::randFloat(),
		.5f-cmn::randFloat()
	).norm();
}

//2d string sizing based on visible characters
void getStringSize(const std::string& str, int& w, int& h) {
	w=0, h=0;

	int ox=0, oy=0;
	for(const auto& c:str) {
		if(c==' ') ox++;
		//tabsize=2
		else if(c=='\t') ox+=2;
		else if(c=='\n') ox=0, oy++;
		else if(c>='!'&&c<='~') {
			ox++;
			if(ox>w) w=ox;
			int nh=1+oy;
			if(nh>h) h=nh;
		}
	}
};

class MinesweeperUI : public cmn::SokolEngine {
	sg_sampler sampler{};


	struct {
		sg_view blank{};
		sg_view tile{};
		sg_view flag{};
		sg_view bomb{};
		sg_view circle{};
	} textures;

	struct {
		sg_pass_action pass_action{};

		sg_image color_img{SG_INVALID_ID};
		sg_view color_attach{SG_INVALID_ID};
		sg_view color_tex{SG_INVALID_ID};

		sg_image depth_img{SG_INVALID_ID};
		sg_view depth_attach{SG_INVALID_ID};
	} render;

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};
		sg_buffer ibuf{};
	} line_render;

	sg_pipeline mesh_pip{};

	Mesh face_mesh;

	struct {
		Mesh mesh;

		int i=0, j=0, k=0;
	} cursor;

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};

		std::vector<Billboard> instances;
	} billboard_render;

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};
	} colorview_render;

	cmn::Font font;

	struct {
		sg_pass_action pass_action{};

		sg_pipeline pip{};

		sg_buffer vbuf{};
		sg_buffer ibuf{};
	} process;

	Minesweeper game;

	struct {
		std::vector<Particle> debris;
		std::vector<Particle> explosion;
		const vf3d gravity{0, -9.8f, 0};
	} particles;

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
#pragma region CREATE HELPERS
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
		textures.blank=cmn::makeBlankTexture();

		if(!cmn::makeTextureFromFile(textures.tile, "assets/img/tile.png")) textures.tile=textures.blank;

		{//load flag img
			int width, height, comp;
			unsigned char* pixels8=stbi_load("assets/img/flag.png", &width, &height, &comp, 4);
			if(!pixels8) textures.flag=textures.blank;
			else {
				std::uint32_t* pixels32=new std::uint32_t[width*height];
				std::memcpy(pixels32, pixels8, sizeof(std::uint8_t)*4*width*height);
				stbi_image_free(pixels8);

				//setup flag tex
				textures.flag=cmn::makeTextureFromPixels(pixels32, width, height);

				//set icon to flag tex
				sapp_icon_desc icon_desc{};
				icon_desc.images[0].width=width;
				icon_desc.images[0].height=height;
				icon_desc.images[0].pixels.ptr=pixels32;
				icon_desc.images[0].pixels.size=sizeof(std::uint32_t)*width*height;
				sapp_set_icon(&icon_desc);
				delete[] pixels32;
			}
		}

		if(!cmn::makeTextureFromFile(textures.bomb, "assets/img/bomb.png")) textures.bomb=textures.blank;

		int sz=1024;
		std::uint32_t* pixels=new std::uint32_t[sz*sz];
		for(int x=0; x<sz; x++) {
			for(int y=0; y<sz; y++) {
				int dx=x-sz/2, dy=y-sz/2;
				bool in=dx*dx+dy*dy<sz*sz/4;//r=sz/2
				pixels[x+sz*y]=in?0xFFFFFFFF:0x00000000;
			}
		}
		textures.circle=cmn::makeTextureFromPixels(pixels, sz, sz);
		delete[] pixels;
	}

	//since will be called on resize,
	//  this needs to free & remake resources
	void setupRenderTarget() {
		//make color img
		{
			sg_destroy_image(render.color_img);
			sg_image_desc image_desc{};
			image_desc.usage.color_attachment=true;
			image_desc.width=sapp_width();
			image_desc.height=sapp_height();
			render.color_img=sg_make_image(image_desc);

			//make color attach
			{
				sg_destroy_view(render.color_attach);
				sg_view_desc view_desc{};
				view_desc.color_attachment.image=render.color_img;
				render.color_attach=sg_make_view(view_desc);
			}

			//make color tex
			{
				sg_destroy_view(render.color_tex);
				sg_view_desc view_desc{};
				view_desc.texture.image=render.color_img;
				render.color_tex=sg_make_view(view_desc);
			}
		}

		{
			//make depth img
			sg_destroy_image(render.depth_img);
			sg_image_desc image_desc{};
			image_desc.usage.depth_stencil_attachment=true;
			image_desc.width=sapp_width();
			image_desc.height=sapp_height();
			image_desc.pixel_format=SG_PIXELFORMAT_DEPTH;
			render.depth_img=sg_make_image(image_desc);

			//make depth attach
			sg_destroy_view(render.depth_attach);
			sg_view_desc view_desc{};
			view_desc.depth_stencil_attachment.image=render.depth_img;
			render.depth_attach=sg_make_view(view_desc);
		}
	}

	void setupRender() {
		setupRenderTarget();

		//clear to grey
		render.pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
		render.pass_action.colors[0].clear_value={.5f, .5f, .5f, 1};
	}

	void setupLineRendering() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_line_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.shader=sg_make_shader(line_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_LINES;
		pip_desc.index_type=SG_INDEXTYPE_UINT32;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.pixel_format=SG_PIXELFORMAT_DEPTH;
		line_render.pip=sg_make_pipeline(pip_desc);

		//vertex buffer
		{
			//xyz
			float vertexes[8][3]{
				{0, 0, 0},
				{1, 0, 0},
				{0, 1, 0},
				{1, 1, 0},
				{0, 0, 1},
				{1, 0, 1},
				{0, 1, 1},
				{1, 1, 1}
			};
			sg_buffer_desc buffer_desc{};
			buffer_desc.data=SG_RANGE(vertexes);
			line_render.vbuf=sg_make_buffer(buffer_desc);
		}

		//index buffer
		{
			//ab
			std::uint32_t indexes[12][2]{
				{0, 1}, {0, 2}, {0, 4},
				{1, 3}, {1, 5}, {2, 3},
				{2, 6}, {3, 7}, {4, 5},
				{4, 6}, {5, 7}, {6, 7}
			};
			sg_buffer_desc buffer_desc{};
			buffer_desc.usage.index_buffer=true;
			buffer_desc.data=SG_RANGE(indexes);
			line_render.ibuf=sg_make_buffer(buffer_desc);
		}
	}

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
		pip_desc.depth.pixel_format=SG_PIXELFORMAT_DEPTH;
		mesh_pip=sg_make_pipeline(pip_desc);
	}

	void setupFaceMesh() {
		Mesh& m=face_mesh;
		m.verts={
			{{0, 0, 0}, {0, 1, 0}, {0, 0}},
			{{1, 0, 0}, {0, 1, 0}, {1, 0}},
			{{0, 0, 1}, {0, 1, 0}, {0, 1}},
			{{1, 0, 1}, {0, 1, 0}, {1, 1}}
		};
		m.updateVertexBuffer();
		m.tris={
			{0, 2, 1},
			{1, 2, 3}
		};
		m.updateIndexBuffer();
	}

	void setupCursor() {
		Mesh& m=cursor.mesh;
		if(!Mesh::loadFromOBJ(m, "assets/model/cursor_obj.txt").valid) m=Mesh::makeCube();
	}

	void setupBillboardRendering() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_billboard_i_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_billboard_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(billboard_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		//pip_desc.cull_mode=SG_CULLMODE_FRONT;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.pixel_format=SG_PIXELFORMAT_DEPTH;
		//with alpha blending
		pip_desc.colors[0].blend.enabled=true;
		pip_desc.colors[0].blend.src_factor_rgb=SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha=SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		billboard_render.pip=sg_make_pipeline(pip_desc);

		//xyzuv
		float vertexes[4][5]{
			{0, 0, 0, 0, 0},//tl
			{0, 0, 1, 0, 1},//bl
			{1, 0, 0, 1, 0},//tr
			{1, 0, 1, 1, 1}//br
		};
		sg_buffer_desc buffer_desc{};
		buffer_desc.data=SG_RANGE(vertexes);
		billboard_render.vbuf=sg_make_buffer(buffer_desc);
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
		pip_desc.depth.pixel_format=SG_PIXELFORMAT_DEPTH;
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
		font=cmn::Font("assets/img/intrepid_8x8.png", 8, 8);
	}

	void setupProcess() {
		process.pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
		process.pass_action.colors[0].clear_value={0, 0, 0, 1};

		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_process_i_pos].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_process_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(process_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		process.pip=sg_make_pipeline(pip_desc);

		//xyuv
		float vertexes[4][4]{
			{-1, -1, 0, 0},
			{1, -1, 1, 0}, 
			{-1, 1, 0, 1},
			{1, 1, 1, 1}
		};
		sg_buffer_desc buffer_desc{};
		buffer_desc.data=SG_RANGE(vertexes);
		process.vbuf=sg_make_buffer(buffer_desc);
	}

	void setupGame() {
		game=Minesweeper(7, 5, 8, 20);
		//game.sweep(0, 0, 0);
	}
#pragma endregion

	void userCreate() override {
		std::srand(std::time(0));

		setupEnvironment();

		setupSampler();

		setupTextures();

		setupRender();

		setupLineRendering();

		setupMeshPipeline();

		setupFaceMesh();

		setupCursor();

		setupBillboardRendering();

		setupColorviewRendering();

		setupFont();

		setupProcess();

		setupGame();
	}

	void userInput(const sapp_event* e) override {
		switch(e->type) {
			case SAPP_EVENTTYPE_RESIZED:
				setupRenderTarget();
				break;
		}
	}

#pragma region UPDATE HELPERS
	void handleCameraLooking(float dt) {
		if(getMouse(SAPP_MOUSEBUTTON_LEFT).held) {
			cam.yaw-=mouse_dx*dt;
			cam.pitch-=mouse_dy*dt;
		}

		//left/right/up/down
		if(getKey(SAPP_KEYCODE_LEFT).held) cam.yaw-=dt;
		if(getKey(SAPP_KEYCODE_RIGHT).held) cam.yaw+=dt;
		if(getKey(SAPP_KEYCODE_UP).held) cam.pitch-=dt;
		if(getKey(SAPP_KEYCODE_DOWN).held) cam.pitch+=dt;

		//clamp camera pitch
		if(cam.pitch>cmn::Pi/2) cam.pitch=cmn::Pi/2-.001f;
		if(cam.pitch<-cmn::Pi/2) cam.pitch=.001f-cmn::Pi/2;
	}

	//dynamic camera system :D
	void handleCameraPlacement() {
		const vf3d game_size(game.getWidth(), game.getHeight(), game.getDepth());
		//pt outside box + margin
		float st_dist=1+game_size.mag()/2;
		vf3d ctr=game_size/2;
		vf3d st=ctr-st_dist*cam.dir;
		//snap to box
		cmn::AABB3 box{{0, 0, 0}, game_size};
		float dist=st_dist-box.intersectRay(st, cam.dir);
		//push cam back + margin
		cam.pos=ctr-(4+dist)*cam.dir;
		//light always at cam
		light_pos=cam.pos;
	}

	//make this use dda instead.
	void handleCursor() {
		//unprojection matrix
		mat4 inv_vp=mat4::inverse(cam.view_proj);

		//mouse coords from clip -> world
		float ndc_x=2*mouse_x/sapp_widthf()-1;
		float ndc_y=1-2*mouse_y/sapp_heightf();
		vf3d clip(ndc_x, ndc_y, 1);
		float w=1;
		vf3d world=matMulVec(inv_vp, clip, w);
		world/=w;

		//normalize direction
		vf3d mouse_dir=(world-cam.pos).norm();

		//intersect ray with cells
		float record=-1;
		cmn::AABB3 unit_box{{0, 0, 0}, {1, 1, 1}};
		for(int i=0; i<game.getWidth(); i++) {
			for(int j=0; j<game.getHeight(); j++) {
				for(int k=0; k<game.getDepth(); k++) {
					const auto& cell=game.cells[game.ix(i, j, k)];
					if(cell.swept) continue;

					vf3d rel=cam.pos-vf3d(i, j, k);
					float t=unit_box.intersectRay(rel, mouse_dir);
					if(t>0) {
						if(record<0||t<record) {
							record=t;
							cursor.i=i;
							cursor.j=j;
							cursor.k=k;
						}
					}
				}
			}
		}
	}

	void spawnDebris(const vf3d& ctr) {
		int num=cmn::randInt(4, 8);
		for(int i=0; i<num; i++) {
			//random pos offset
			float pos_rad=cmn::randFloat(.5f);
			vf3d pos=ctr+pos_rad*randDir();

			//random velocity
			float speed=cmn::randFloat(.6f, 1.2f);
			vf3d vel=speed*randDir();

			//random size, lifespan
			float size=cmn::randFloat(.06f, .13f);
			float lifespan=cmn::randFloat(.9f, 1.3f);
			particles.debris.push_back(Particle(pos, vel, size, lifespan));
		}
	}

	void spawnExplosion(const vf3d& ctr) {
		int num=cmn::randInt(24, 32);
		for(int i=0; i<num; i++) {
			//random pos offset
			float pos_rad=cmn::randFloat(.5f);
			vf3d pos=ctr+pos_rad*randDir();

			//random upward velocity
			vf3d dir=randDir();
			if(dir.y<0) dir*=-1;
			float speed=cmn::randFloat(2.5f, 3.7f);
			vf3d vel=speed*dir;

			//random size, lifespan
			float size=cmn::randFloat(.14f, .25f);
			float lifespan=cmn::randFloat(1.3f, 1.9f);
			particles.explosion.push_back(Particle(pos, vel, size, lifespan));
		}
	}

	void handleGame(float dt) {
		if(getKey(SAPP_KEYCODE_R).pressed) game.reset();

		if(getKey(SAPP_KEYCODE_P).pressed) game.pause();

		if(getKey(SAPP_KEYCODE_SPACE).pressed) game.sweep(cursor.i, cursor.j, cursor.k);

		if(getKey(SAPP_KEYCODE_F).pressed) game.flag(cursor.i, cursor.j, cursor.k);

		//find changes from prev->curr
		for(int i=0; i<game.getWidth(); i++) {
			for(int j=0; j<game.getHeight(); j++) {
				for(int k=0; k<game.getDepth(); k++) {
					vf3d ctr=.5f+vf3d(i, j, k);

					int ix=game.ix(i, j, k);
					const auto& prev=game.prev_cells[ix];
					const auto& curr=game.cells[ix];
					if(curr.swept&&!prev.swept) {
						//swept a cell
						spawnDebris(ctr);
						if(curr.bomb) {
							//swept a bomb
							spawnExplosion(ctr);
						}
					}
				}
			}
		}

		game.update(dt);
	}

	//update & sanitize particles
	void updateParticles(float dt) {
		for(int i=0; i<2; i++) {
			auto& ptcs=i==0?particles.debris:particles.explosion;

			for(auto it=ptcs.begin(); it!=ptcs.end();) {
				it->accelerate(particles.gravity);
				it->update(dt);
				if(it->isDead()) it=ptcs.erase(it);
				else it++;
			}
		}
	}

	void updateCameraMatrixes() {
		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		//cam proj can change with window resize
		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(70, asp, .001f, 1000.f);

		cam.view_proj=mat4::mul(cam.proj, cam.view);
	}
#pragma endregion

	void userUpdate(float dt) override {
		handleCameraLooking(dt);
		cam.dir=polarToCartesian(cam.yaw, cam.pitch);
		handleCameraPlacement();

		handleCursor();

		handleGame(dt);

		updateParticles(dt);

		updateCameraMatrixes();
	}

#pragma region RENDER HELPERS
	void renderAABB(const vf3d& a, const vf3d& b, sg_color col) {
		sg_apply_pipeline(line_render.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=line_render.vbuf;
		bind.index_buffer=line_render.ibuf;
		sg_apply_bindings(bind);

		mat4 model=mat4::mul(mat4::makeTranslation(a), mat4::makeScale(b-a));
		mat4 mvp=mat4::mul(cam.view_proj, model);

		vs_line_params_t vs_line_params{};
		std::memcpy(vs_line_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_line_params, SG_RANGE(vs_line_params));

		fs_line_params_t fs_line_params{};
		fs_line_params.u_col[0]=col.r;
		fs_line_params.u_col[1]=col.g;
		fs_line_params.u_col[2]=col.b;
		fs_line_params.u_col[3]=col.a;
		sg_apply_uniforms(UB_fs_line_params, SG_RANGE(fs_line_params));

		sg_draw(0, 2*12, 1);
	}

	void renderFaces() {
		auto renderFace=[&] (const mat4& m, const sg_view& t) {
			sg_apply_pipeline(mesh_pip);

			sg_bindings bind{};
			bind.vertex_buffers[0]=face_mesh.vbuf;
			bind.index_buffer=face_mesh.ibuf;
			bind.samplers[SMP_u_mesh_smp]=sampler;
			bind.views[VIEW_u_mesh_tex]=t;
			sg_apply_bindings(bind);

			mat4 mvp=mat4::mul(cam.view_proj, m);

			vs_mesh_params_t vs_mesh_params{};
			std::memcpy(vs_mesh_params.u_model, m.m, sizeof(m.m));
			std::memcpy(vs_mesh_params.u_mvp, mvp.m, sizeof(mvp.m));
			vs_mesh_params.u_tl[0]=0;
			vs_mesh_params.u_tl[1]=0;
			vs_mesh_params.u_br[0]=1;
			vs_mesh_params.u_br[1]=1;
			sg_apply_uniforms(UB_vs_mesh_params, SG_RANGE(vs_mesh_params));

			fs_mesh_params_t fs_mesh_params{};
			fs_mesh_params.u_eye_pos[0]=cam.pos.x;
			fs_mesh_params.u_eye_pos[1]=cam.pos.y;
			fs_mesh_params.u_eye_pos[2]=cam.pos.z;
			fs_mesh_params.u_light_pos[0]=light_pos.x;
			fs_mesh_params.u_light_pos[1]=light_pos.y;
			fs_mesh_params.u_light_pos[2]=light_pos.z;
			fs_mesh_params.u_tint[0]=1;
			fs_mesh_params.u_tint[1]=1;
			fs_mesh_params.u_tint[2]=1;
			sg_apply_uniforms(UB_fs_mesh_params, SG_RANGE(fs_mesh_params));

			sg_draw(0, 3*face_mesh.tris.size(), 1);
		};

		for(const auto& t:game.tile_faces) renderFace(t, textures.tile);
		for(const auto& f:game.flag_faces) renderFace(f, textures.flag);
	}

	sg_color getCellColor(int num_bombs) {
		switch(num_bombs) {
			case 0: return {1, 1, 1, 1};//white
			case 1: return {0, .392f, 1, 1};//light blue
			case 2: return {0, .784f, 0, 1};//dark green
			case 3: return {1, .137f, .137f, 1};//light red
			case 4: return {.863f, .863f, 0, 1};//dark yellow
			case 5: return {.502f, 0, 0, 1};//dark red
			default:
			{//6-26
				const sg_color purple{.702f, 0, 1, 1};
				const sg_color black{0, 0, 0, 1};
				float t=(num_bombs-6.f)/(26-6);
				return mixCol(purple, black, t);
			}
		}
	}

	void realizeNumberBillboards() {
		auto addNumber=[&] (const vf3d& ctr, float size, float ax, float ay, int n, sg_color col) {
			float l, t, r, b;
			font.getRegion('0'+n, l, t, r, b);
			billboard_render.instances.push_back(Billboard(
				ctr, size, ax, ay,
				font.tex, l, t, r, b,
				col
			));
		};

		//for each cell
		for(int i=0; i<game.getWidth(); i++) {
			for(int j=0; j<game.getHeight(); j++) {
				for(int k=0; k<game.getDepth(); k++) {
					//skip unswept
					const auto& cell=game.cells[game.ix(i, j, k)];
					if(!cell.swept) continue;

					vf3d ctr=.5f+vf3d(i, j, k);

					if(cell.bomb) {
						billboard_render.instances.push_back(Billboard(
							ctr, .6f, .5f, .5f,
							textures.bomb, 0, 0, 1, 1,
							{1, 1, 1, 1}
						));
						continue;
					}

					//skip empties
					if(cell.num_bombs==0) continue;

					//display 2or1 digit number
					int tens=cell.num_bombs/10;
					int ones=cell.num_bombs%10;
					
					//center anchoring if 2 digit
					sg_color col=getCellColor(cell.num_bombs);
					if(tens) {
						addNumber(ctr, .3f, 1, .5f, tens, col);
						addNumber(ctr, .3f, 0, .5f, ones, col);
					} else {
						addNumber(ctr, .4f, .5f, .5f, ones, col);
					}
				}
			}
		}
	}

	void realizeParticleBillboards() {
		//explosion gradient
		auto sampleGradient=[] (float t) {
			const vf3d yellow{1, 1, 0};
			const vf3d orange{1, .369f, 0};
			const vf3d grey{.271f, .271f, .271f};
			if(t<.5f) return yellow+2*t*(orange-yellow);
			else return orange+2*(t-.5f)*(grey-orange);
		};

		for(int i=0; i<2; i++) {
			const auto& ptcs=i==0?particles.debris:particles.explosion;

			for(const auto& p:ptcs) {
				//relative age
				float t=p.age/p.lifespan;
				vf3d col=i==1?sampleGradient(t):vf3d(1, 1, 1);
				billboard_render.instances.push_back(Billboard(
					p.pos, p.size, .5f, .5f,
					textures.circle, 0, 0, 1, 1,
					{col.x, col.y, col.z, 1-t}
				));
			}
		}
	}

	void renderBillboards() {
		//sort billboards w.r.t decreasing dist to camera 
		std::sort(billboard_render.instances.begin(), billboard_render.instances.end(),
			[&] (const Billboard& a, const Billboard& b) {
			return (a.pos-cam.pos).mag_sq()>(b.pos-cam.pos).mag_sq();
		});

		for(const auto& b:billboard_render.instances) {
			sg_apply_pipeline(billboard_render.pip);

			sg_bindings bind{};
			bind.vertex_buffers[0]=billboard_render.vbuf;
			bind.samplers[SMP_u_billboard_smp]=sampler;
			bind.views[VIEW_u_billboard_tex]=b.tex;
			sg_apply_bindings(bind);

			//unit axes to point at camera
			vf3d uy=(cam.pos-b.pos).norm();
			vf3d ux=vf3d(0, 1, 0).cross(uy).norm();
			vf3d uz=ux.cross(uy);
			//scale axes
			vf3d x=b.size*ux, y=b.size*uy, z=b.size*uz;
			//translate with anchoring offset
			vf3d trans=b.pos-b.anchor_x*x-b.anchor_y*z;
			mat4 model;
			model(0, 0)=x.x, model(0, 1)=y.x, model(0, 2)=z.x, model(0, 3)=trans.x;
			model(1, 0)=x.y, model(1, 1)=y.y, model(1, 2)=z.y, model(1, 3)=trans.y;
			model(2, 0)=x.z, model(2, 1)=y.z, model(2, 2)=z.z, model(2, 3)=trans.z;
			model(3, 3)=1;
			mat4 mvp=mat4::mul(cam.view_proj, model);

			vs_billboard_params_t vs_billboard_params{};
			std::memcpy(vs_billboard_params.u_mvp, mvp.m, sizeof(mvp.m));
			vs_billboard_params.u_tl[0]=b.left;
			vs_billboard_params.u_tl[1]=b.top;
			vs_billboard_params.u_br[0]=b.right;
			vs_billboard_params.u_br[1]=b.bottom;
			sg_apply_uniforms(UB_vs_billboard_params, SG_RANGE(vs_billboard_params));

			fs_billboard_params_t fs_billboard_params{};
			fs_billboard_params.u_tint[0]=b.col.r;
			fs_billboard_params.u_tint[1]=b.col.g;
			fs_billboard_params.u_tint[2]=b.col.b;
			fs_billboard_params.u_tint[3]=b.col.a;
			sg_apply_uniforms(UB_fs_billboard_params, SG_RANGE(fs_billboard_params));

			sg_draw(0, 4, 1);
		}

		billboard_render.instances.clear();
	}

	void renderCursor() {
		sg_apply_pipeline(mesh_pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=cursor.mesh.vbuf;
		bind.index_buffer=cursor.mesh.ibuf;
		bind.samplers[SMP_u_mesh_smp]=sampler;
		bind.views[VIEW_u_mesh_tex]=textures.blank;
		sg_apply_bindings(bind);

		mat4 model=mat4::makeTranslation(.5f+vf3d(cursor.i, cursor.j, cursor.k));
		mat4 mvp=mat4::mul(cam.view_proj, model);

		vs_mesh_params_t vs_mesh_params{};
		std::memcpy(vs_mesh_params.u_model, model.m, sizeof(model.m));
		std::memcpy(vs_mesh_params.u_mvp, mvp.m, sizeof(mvp.m));
		vs_mesh_params.u_tl[0]=0;
		vs_mesh_params.u_tl[1]=0;
		vs_mesh_params.u_br[0]=1;
		vs_mesh_params.u_br[1]=1;
		sg_apply_uniforms(UB_vs_mesh_params, SG_RANGE(vs_mesh_params));

		//only show cell color if its swept
		const auto& cell=game.cells[game.ix(cursor.i, cursor.j, cursor.k)];
		sg_color col=cell.swept?getCellColor(cell.num_bombs):sg_color{1, 0, 0, 1};

		fs_mesh_params_t fs_mesh_params{};
		fs_mesh_params.u_eye_pos[0]=cam.pos.x;
		fs_mesh_params.u_eye_pos[1]=cam.pos.y;
		fs_mesh_params.u_eye_pos[2]=cam.pos.z;
		fs_mesh_params.u_light_pos[0]=light_pos.x;
		fs_mesh_params.u_light_pos[1]=light_pos.y;
		fs_mesh_params.u_light_pos[2]=light_pos.z;
		fs_mesh_params.u_tint[0]=col.r;
		fs_mesh_params.u_tint[1]=col.g;
		fs_mesh_params.u_tint[2]=col.b;
		sg_apply_uniforms(UB_fs_mesh_params, SG_RANGE(fs_mesh_params));

		sg_draw(0, 3*cursor.mesh.tris.size(), 1);
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
		bind.samplers[SMP_u_colorview_smp]=sampler;
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

	void renderStats() {
		//take up 1/12 of height
		float scl=sapp_heightf()/font.char_h/12;

		float y=sapp_heightf()-scl*font.char_h;

		//HH:MM:SS display
		{
			//divvy time into hours, then minutes
			int total=game.timer;
			int hours=total/3600;
			int minutes=(total/60)%60;
			int seconds=total%60;

			//format time
			char time_str[9]{
				char('0'+hours/10),
				char('0'+hours%10),
				':',
				char('0'+minutes/10),
				char('0'+minutes%10),
				':',
				char('0'+seconds/10),
				char('0'+seconds%10),
				'\0'
			};

			//skip "HH:" if 0
			std::string str(time_str+(hours?0:3));
			renderString(0, y, str, scl);
		}

		//bomb count
		{
			int remaining=game.getNumBombs();
			for(int i=0; i<game.getNumCells(); i++) {
				if(game.cells[i].flagged) remaining--;
			}
			auto str=std::to_string(remaining);
			float x=sapp_widthf()-scl*font.char_w*str.length();
			renderString(x, y, str, scl);
		}
	}

	void renderStateOverlay() {
		float alpha=0;
		std::string str;
		sg_color col;
		switch(game.state) {
			default: return;
			case Minesweeper::PLAYING:
				renderStats();
				return;
			case Minesweeper::PAUSED:
				alpha=.8f;
				str="PAUSED\nPAUSED\nPAUSED";
				col={.318f, .655f, .909f, 1};
				break;
			case Minesweeper::LOST:
				alpha=.3f;
				str="YOU LOSE!";
				col={.761f, .055f, .055f, 1};
				break;
			case Minesweeper::WON:
				alpha=.5f;
				str="YOU WIN!";
				col={.133f, .839f, .322f, 1};
				break;
		}

		//darken screen
		renderTex(
			0, 0, sapp_widthf(), sapp_heightf(),
			textures.blank, 0, 0, 1, 1,
			{0, 0, 0, alpha}
		);

		//screen box to fit into
		float w_scr=.8f*sapp_widthf();
		float h_scr=.8f*sapp_heightf();

		//determine string sizing
		int w_c, h_c;
		getStringSize(str, w_c, h_c);
		float w_str=font.char_w*w_c;
		float h_str=font.char_h*h_c;

		//which dimension is limiting?
		float nx=w_scr/w_str;
		float ny=h_scr/h_str;
		float scl=nx<ny?nx:ny;

		float cx=.5f*sapp_widthf();
		float cy=.5f*sapp_heightf();
		renderString(cx-.5f*scl*w_str, cy-.5f*scl*h_str, str, scl, col);
	}
#pragma endregion

	void userRender() override {
		//render
		{
			sg_pass pass{};
			pass.action=render.pass_action;
			pass.attachments.colors[0]=render.color_attach;
			pass.attachments.depth_stencil=render.depth_attach;
			sg_begin_pass(pass);

			//black box for game "bounds"
			renderAABB(vf3d(0, 0, 0), vf3d(game.getWidth(), game.getHeight(), game.getDepth()), {0, 0, 0, 1});

			renderFaces();

			renderCursor();

			realizeNumberBillboards();
			realizeParticleBillboards();
			renderBillboards();

			renderStateOverlay();

			sg_end_pass();
		}

		//process
		{
			sg_pass pass{};
			pass.action=process.pass_action;
			pass.swapchain=sglue_swapchain();
			sg_begin_pass(pass);

			sg_apply_pipeline(process.pip);

			sg_bindings bind{};
			bind.vertex_buffers[0]=process.vbuf;
			bind.samplers[SMP_u_process_smp]=sampler;
			bind.views[VIEW_u_process_tex]=render.color_tex;
			sg_apply_bindings(bind);

			sg_draw(0, 4, 1);

			sg_end_pass();
		}

		sg_commit();
	}

	MinesweeperUI() {
		app_title="Minesweeper 3D";
	}
};