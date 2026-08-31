/*todo:
win/lose particles?
difficulty modes
sound
fix font
*/
#define SOKOL_IMPL
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "vendor/sokol/sokol_app.h"
#include "vendor/sokol/sokol_gfx.h"
#include "vendor/sokol/sokol_glue.h"

#include "vendor/sokol/sokol_gl.h"

#include "common/sokol/sokol_engine.h"

#include "crt.glsl.h"

//for time
#include <ctime> 

#include "common/math/mat4.h"

#include "minesweeper.h"

#include "common/utils.h"

#include <vector>

#include <algorithm>

#include "common/geom/aabb3.h"

#include "particle.h"

#include "billboard.h"

#include "common/sokol/font.h"

#include "render_target.h"

#include "cursor_mesh.h"

sg_color mixCol(const sg_color& a, const sg_color& b, float t) {
	return {
		a.r+t*(b.r-a.r),
		a.g+t*(b.g-a.g),
		a.b+t*(b.b-a.b),
		a.a+t*(b.a-a.a)
	};
}

cmn::vf3d randDir() {
	return normalize(cmn::vf3d(
		.5f-cmn::randFloat(),
		.5f-cmn::randFloat(),
		.5f-cmn::randFloat()
	));
}

//run f for each char of formatted string
template<typename Func>
void formattedStringDo(const std::string& str, Func f) {
	int ox=0, oy=0;
	for(const auto& c:str) {
		//special formatting
		if(c==' ') ox++;//padding
		else if(c=='\t') ox+=2;//tabsize=2
		else if(c=='\n') ox=0, oy++;//return
		else if(c>='!'&&c<='~') {//printable
			f(c, ox, oy);

			ox++;
		}
	}
}

//2d string sizing based on visible characters
void getStringSize(const std::string& str, int& w, int& h) {
	w=0, h=0;

	formattedStringDo(str, [&] (char c, int ox, int oy) {
		//vacuform to char
		w=std::max(w, ox+1);
		h=std::max(h, oy+1);
	});
};

void explosionGradient(float t, float* r, float* g, float* b) {
	static const float cols[][3]{
		{1, 1, 0},//yellow
		{1, .369f, 0},//orange
		{.271f, .271f, .271f}//grey
	};
	static const int num=sizeof(cols)/sizeof(*cols);
	cmn::colorGradient(
		cols, num, t,
		r, g, b
	);
}

using cmn::vf3d;
using cmn::mat4;

class MinesweeperUI : public cmn::SokolEngine {
	//scene
	Minesweeper game;

	int cursor_i=0;
	int cursor_j=0;
	int cursor_k=0;

	std::vector<Billboard> billboards;

	struct {
		std::vector<Particle> debris;
		std::vector<Particle> explosion;
		const vf3d gravity{0, -9.8f, 0};
	} particles;

	//user input
	float mouse_x=0, mouse_y=0;
	float prev_mouse_x=0, prev_mouse_y=0;

	struct {
		vf3d pos;
		vf3d dir;
		float yaw=0, pitch=0;
		mat4 proj, view;
		//view, then project
		mat4 view_proj;
	} cam;

	//graphics
	sgl_pipeline pip3d{};
	sgl_pipeline pip2d{};

	sg_sampler sampler{};

	struct {
		sg_view blank{};
		sg_view tile{};
		sg_view flag{};
		sg_view bomb{};
		sg_view circle{};
	} textures;

	cmn::Font font;

	struct {
		RenderTarget rt;

		sg_pipeline crt_pip{};

		sg_buffer vbuf{};
	} post_process;

public:
#pragma region CREATE HELPERS
	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);
	}

	void setupGame() {
		game=Minesweeper(7, 5, 8, 20);
	}

	void setupPipeline3D() {
		sg_pipeline_desc pip_desc{};
		pip_desc.face_winding=SG_FACEWINDING_CCW;
		pip_desc.cull_mode=SG_CULLMODE_BACK;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		//with alpha blending
		pip_desc.colors[0].blend.enabled=true;
		pip_desc.colors[0].blend.src_factor_rgb=SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha=SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip3d=sgl_make_pipeline(pip_desc);
	}

	void setupPipeline2D() {
		sg_pipeline_desc pip_desc{};
		//with alpha blending
		pip_desc.colors[0].blend.enabled=true;
		pip_desc.colors[0].blend.src_factor_rgb=SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha=SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip2d=sgl_make_pipeline(pip_desc);
	}

	void setupSampler() {
		sg_sampler_desc sampler_desc{};
		sampler=sg_make_sampler(sampler_desc);
	}

	//"primitive" textures to work with
	bool setupTextures() {
		textures.blank=cmn::makeBlankTexture();

		if(!cmn::makeTextureFromFile(textures.tile, "assets/tile.png")) return false;

		{//load flag img
			int width, height, comp;
			auto pixels=stbi_load("assets/flag.png", &width, &height, &comp, 4);
			if(!pixels) return false;

			//setup flag tex
			textures.flag=cmn::makeTextureFromPixels(pixels, width, height);

			//set icon to flag tex
			sapp_icon_desc icon_desc{};
			icon_desc.images[0].width=width;
			icon_desc.images[0].height=height;
			icon_desc.images[0].pixels.ptr=pixels;
			icon_desc.images[0].pixels.size=4*width*height;
			sapp_set_icon(&icon_desc);

			stbi_image_free(pixels);
		}

		if(!cmn::makeTextureFromFile(textures.bomb, "assets/bomb.png")) return false;

		int sz=1024;
		std::uint8_t* pixels=new std::uint8_t[4*sz*sz];
		for(int x=0; x<sz; x++) {
			for(int y=0; y<sz; y++) {
				int dx=x-sz/2, dy=y-sz/2;
				bool in=dx*dx+dy*dy<sz*sz/4;//r=sz/2
				int z=x+sz*y;
				auto& r=pixels[0+4*z];
				auto& g=pixels[1+4*z];
				auto& b=pixels[2+4*z];
				auto& a=pixels[3+4*z];
				if(in) r=255, g=255, b=255, a=255;
				else r=0, g=0, b=0, a=0;
			}
		}
		textures.circle=cmn::makeTextureFromPixels(pixels, sz, sz);
		delete[] pixels;

		return true;
	}

	void setupFont() {
		font=cmn::Font("assets/intrepid_8x8.png", 8, 8);
	}

	void setupPostProcess() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_crt_i_pos].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(crt_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		post_process.crt_pip=sg_make_pipeline(pip_desc);

		//xy
		float vertexes[4][2]{
			{-1, -1},
			{1, -1},
			{-1, 1},
			{1, 1}
		};
		sg_buffer_desc buffer_desc{};
		buffer_desc.data=SG_RANGE(vertexes);
		post_process.vbuf=sg_make_buffer(buffer_desc);
	}
#pragma endregion

	bool user_create() override {
		app_title="Minesweeper 3D";

		std::srand(std::time(0));

		setupSGL();

		setupGame();

		setupPipeline3D();

		setupPipeline2D();

		setupSampler();

		if(!setupTextures()) return false;

		setupFont();

		setupPostProcess();

		return true;
	}

#pragma region UPDATE HELPERS
	void handleCameraLooking(float dt) {
		prev_mouse_x=mouse_x;
		prev_mouse_y=mouse_y;
		mouse_x=GetMouseX();
		mouse_y=GetMouseY();

		//mouse orbit
		if(GetMouse(SAPP_MOUSEBUTTON_LEFT).held) {
			cam.yaw+=(mouse_x-prev_mouse_x)*dt;
			cam.pitch-=(mouse_y-prev_mouse_y)*dt;
		}

		//left/right/up/down
		if(GetKey(SAPP_KEYCODE_LEFT).held) cam.yaw+=dt;
		if(GetKey(SAPP_KEYCODE_RIGHT).held) cam.yaw-=dt;
		if(GetKey(SAPP_KEYCODE_UP).held) cam.pitch-=dt;
		if(GetKey(SAPP_KEYCODE_DOWN).held) cam.pitch+=dt;

		//clamp camera pitch
		if(cam.pitch>cmn::Pi/2) cam.pitch=cmn::Pi/2-.001f;
		if(cam.pitch<-cmn::Pi/2) cam.pitch=.001f-cmn::Pi/2;
	}

	//dynamic camera system :D
	void handleCameraPlacement() {
		cam.dir=vf3d::polar({1, cam.yaw, cam.pitch});

		const vf3d game_size(game.getWidth(), game.getHeight(), game.getDepth());
		//pt outside box + margin
		float st_dist=1+game_size.mag()/2;
		vf3d ctr=game_size/2;
		vf3d st=ctr-st_dist*cam.dir;
		//snap to box
		cmn::AABBf3 box{{0, 0, 0}, game_size};
		float dist=st_dist-box.intersectRay(st, cam.dir);
		//push cam back + margin
		cam.pos=ctr-(4+dist)*cam.dir;
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
		vf3d mouse_dir=normalize(world-cam.pos);

		//intersect ray with cells
		float record=-1;
		cmn::AABBf3 unit_box{{0, 0, 0}, {1, 1, 1}};
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
							cursor_i=i;
							cursor_j=j;
							cursor_k=k;
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
		if(GetKey(SAPP_KEYCODE_R).pressed) game.reset();

		if(GetKey(SAPP_KEYCODE_P).pressed) game.pause();

		if(GetKey(SAPP_KEYCODE_SPACE).pressed) game.sweep(cursor_i, cursor_j, cursor_k);

		if(GetKey(SAPP_KEYCODE_F).pressed) game.flag(cursor_i, cursor_j, cursor_k);

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
		cam.proj=mat4::makePerspective(80, asp, .001f, 1000.f);

		cam.view_proj=mat4::mul(cam.proj, cam.view);
	}
#pragma endregion

	float total_dt=0;

	bool user_update(float dt) override {
		handleCameraLooking(dt);

		handleCameraPlacement();

		handleCursor();

		handleGame(dt);

		updateParticles(dt);

		updateCameraMatrixes();

		total_dt+=dt;

		return true;
	}

#pragma region RENDER HELPERS
	void renderBox(const vf3d& a, const vf3d& b) {
		static const int edges[][2]{
			{0, 1}, {2, 3}, {4, 5}, {6, 7},//thru x
			{0, 2}, {1, 3}, {4, 6}, {5, 7},//thru y
			{0, 4}, {1, 5}, {2, 6}, {3, 7}//thru z
		};

		vf3d v[8];//interpolators: 000, 001, 010...
		for(int i=0; i<8; i++) {
			v[i]=a+vf3d(1&(i>>2), 1&(i>>1), 1&i)*(b-a);
		}

		sgl_begin_lines();
		for(const auto& e:edges) {
			sgl_v3f(v[e[0]].x, v[e[0]].y, v[e[0]].z);
			sgl_v3f(v[e[1]].x, v[e[1]].y, v[e[1]].z);
		}
		sgl_end();
	}

	//show quads on boundaries
	void renderFaces() {
		//"binary"
		static const vf3d vertexes[8]{
			{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
			{0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}
		};
		static const int faces[][4]{
			{2, 0, 4, 6},//-x
			{7, 5, 1, 3},//+x
			{1, 5, 4, 0},//-y
			{2, 6, 7, 3},//+y
			{3, 1, 0, 2},//-z
			{6, 4, 5, 7}//+z
		};
		static const int di[6]{-1, 1, 0, 0, 0, 0};
		static const int dj[6]{0, 0, -1, 1, 0, 0};
		static const int dk[6]{0, 0, 0, 0, -1, 1};
		sgl_enable_texture();
		for(int i=0; i<game.getWidth(); i++) {
			for(int j=0; j<game.getHeight(); j++) {
				for(int k=0; k<game.getDepth(); k++) {
					int ix=game.ix(i, j, k);
					//skip if swept
					const auto& c=game.cells[ix];
					if(c.swept) continue;

					const vf3d ijk(i, j, k);

					sgl_texture(c.flagged?textures.flag:textures.tile, sampler);

					//check neighbors
					sgl_begin_quads();
					sgl_c3f(1, 1, 1);
					for(int d=0; d<6; d++) {
						//skip if in range and not swept
						int ni=i+di[d];
						int nj=j+dj[d];
						int nk=k+dk[d];
						if(game.inRange(ni, nj, nk)) {
							int nix=game.ix(ni, nj, nk);
							if(!game.cells[nix].swept) continue;
						}

						const auto& v0=ijk+vertexes[faces[d][0]];
						const auto& v1=ijk+vertexes[faces[d][1]];
						const auto& v2=ijk+vertexes[faces[d][2]];
						const auto& v3=ijk+vertexes[faces[d][3]];
						vf3d norm=normalize(cross(v1-v0, v2-v0));
						vf3d ctr=(v0+v1+v2+v3)/4;
						vf3d cam_dir=normalize(cam.pos-ctr);
						float s=std::max(.4f, dot(cam_dir, norm));
						sgl_v3f_t2f_c3f(v0.x, v0.y, v0.z, 0, 0, s, s, s);
						sgl_v3f_t2f_c3f(v1.x, v1.y, v1.z, 0, 1, s, s, s);
						sgl_v3f_t2f_c3f(v2.x, v2.y, v2.z, 1, 1, s, s, s);
						sgl_v3f_t2f_c3f(v3.x, v3.y, v3.z, 1, 0, s, s, s);
					}
					sgl_end();
				}
			}
		}
		sgl_disable_texture();
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
			billboards.push_back(Billboard(
				ctr, size, ax, ay,
				font.tex, l, t, r, b,
				col.r, col.g, col.b, col.a
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
						billboards.push_back(Billboard(
							ctr, .6f, .5f, .5f,
							textures.bomb, 0, 0, 1, 1,
							1, 1, 1, 1
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
		for(int i=0; i<2; i++) {
			const auto& ptcs=i==0?particles.debris:particles.explosion;

			for(const auto& p:ptcs) {
				//relative age
				float t=p.age/p.lifespan;
				float r=1, g=1, b=1;
				if(i==1) explosionGradient(t, &r, &g, &b);
				billboards.push_back(Billboard(
					p.pos, p.size, .5f, .5f,
					textures.circle, 0, 0, 1, 1,
					r, g, b, 1-t
				));
			}
		}
	}

	void renderBillboards() {
		//sort billboards w.r.t decreasing dist to camera 
		std::sort(billboards.begin(), billboards.end(),
			[&] (const Billboard& a, const Billboard& b) {
			return (a.pos-cam.pos).mag_sq()>(b.pos-cam.pos).mag_sq();
		});

		sgl_enable_texture();
		for(const auto& b:billboards) {
			//unit axes to point at camera
			vf3d f=normalize(cam.pos-b.pos);
			vf3d r=normalize(cross(f, vf3d(0, 1, 0)));
			vf3d u=cross(r, f);
			//step vectors
			vf3d rgt=b.size*r, up=b.size*u;
			//translate with anchoring offset
			vf3d bl=b.pos-b.anchor_x*rgt-b.anchor_y*up;
			vf3d br=bl+rgt, tl=bl+up, tr=br+up;

			sgl_texture(b.tex, sampler);
			sgl_begin_quads();
			sgl_c4f(b.rgba[0], b.rgba[1], b.rgba[2], b.rgba[3]);
			sgl_v3f_t2f(tl.x, tl.y, tl.z, b.ltrb[2], b.ltrb[1]);
			sgl_v3f_t2f(tr.x, tr.y, tr.z, b.ltrb[0], b.ltrb[1]);
			sgl_v3f_t2f(br.x, br.y, br.z, b.ltrb[0], b.ltrb[3]);
			sgl_v3f_t2f(bl.x, bl.y, bl.z, b.ltrb[2], b.ltrb[3]);
			sgl_end();
		}
		sgl_disable_texture();

		billboards.clear();
	}

	void renderCursor() {
		vf3d xyz(
			.5f+cursor_i,
			.5f+cursor_j,
			.5f+cursor_k
		);
		sgl_begin_triangles();
		for(const auto& t:cursor_mesh::triangles) {
			const auto& va=cursor_mesh::vertexes[t[0]-1];
			const auto& vb=cursor_mesh::vertexes[t[1]-1];
			const auto& vc=cursor_mesh::vertexes[t[2]-1];
			vf3d pa=xyz+vf3d(va[0], va[1], va[2]);
			vf3d pb=xyz+vf3d(vb[0], vb[1], vb[2]);
			vf3d pc=xyz+vf3d(vc[0], vc[1], vc[2]);
			vf3d ctr=(pa+pb+pc)/3;
			vf3d cam_dir=normalize(cam.pos-ctr);
			vf3d norm=normalize(cross(pb-pa, pc-pa));
			float s=std::max(.4f, dot(cam_dir, norm));
			sgl_c3f(s, 0, 0);
			sgl_v3f(pa.x, pa.y, pa.z);
			sgl_v3f(pb.x, pb.y, pb.z);
			sgl_v3f(pc.x, pc.y, pc.z);
		}
		sgl_end();
	}

	void renderString(
		float x, float y,
		const std::string& str, float scl=1,
		const sg_color& col={1, 1, 1, 1}
	) {
		const float w=scl*font.char_w;
		const float h=scl*font.char_w;
		int ox=0, oy=0;
		sgl_enable_texture();
		sgl_texture(font.tex, sampler);
		sgl_begin_quads();
		sgl_c4f(col.r, col.g, col.b, col.a);
		formattedStringDo(str, [&] (char c, int ox, int oy) {
			//texture coords
			float tl, tt, tr, tb;
			font.getRegion(c, tl, tt, tr, tb);

			//vertexes
			float vl=x+w*ox, vt=y+h*oy;
			float vr=vl+w, vb=vt+h;
			sgl_v2f_t2f(vl, vt, tl, tt);
			sgl_v2f_t2f(vr, vt, tr, tt);
			sgl_v2f_t2f(vr, vb, tr, tb);
			sgl_v2f_t2f(vl, vb, tl, tb);
		});
		sgl_end();
		sgl_disable_texture();
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
		float r, g, b;
		switch(game.state) {
			default: return;
			case Minesweeper::PLAYING:
				renderStats();
				return;
			case Minesweeper::PAUSED:
				alpha=.8f;
				str="PAUSED\nPAUSED\nPAUSED";
				r=.318f, g=.655f, b=.909f;
				break;
			case Minesweeper::LOST:
				alpha=.3f;
				str="YOU LOSE!";
				r=.761f, g=.055f, b=.055f;
				break;
			case Minesweeper::WON:
				alpha=.5f;
				str="YOU WIN!";
				r=.133f, g=.839f, b=.322f;
				break;
		}

		const float w_scr=sapp_widthf();
		const float h_scr=sapp_heightf();

		//darken screen
		sgl_begin_quads();
		sgl_c4f(0, 0, 0, alpha);
		sgl_v2f(0, 0);
		sgl_v2f(w_scr, 0);
		sgl_v2f(w_scr, h_scr);
		sgl_v2f(0, h_scr);
		sgl_end();

		//determine string sizing
		int w_c, h_c;
		getStringSize(str, w_c, h_c);
		float w_str=font.char_w*w_c;
		float h_str=font.char_h*h_c;

		//fit into box 80% of screen
		//which dimension is limiting?
		float nx=.8f*w_scr/w_str;
		float ny=.8f*h_scr/h_str;
		float scl=nx<ny?nx:ny;

		float cx=.5f*sapp_widthf();
		float cy=.5f*sapp_heightf();
		renderString(
			cx-.5f*scl*w_str, cy-.5f*scl*h_str,
			str, scl,
			{r, g, b, 1}
		);
	}
#pragma endregion

	bool user_render() override {
		post_process.rt.resize(sapp_widthf(), sapp_heightf());

		//render
		{
			sg_pass pass{};
			pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
			pass.action.colors[0].clear_value={.5f, .5f, .5f, 1};
			pass.attachments.colors[0]=post_process.rt.color_attach;
			pass.attachments.depth_stencil=post_process.rt.depth_attach;
			sg_begin_pass(pass);

			//3d rendering
			sgl_defaults();
			sgl_load_pipeline(pip3d);
			sgl_matrix_mode_projection();
			sgl_load_matrix(cam.proj.m);
			sgl_matrix_mode_modelview();
			sgl_load_matrix(cam.view.m);

			//black box for game "bounds"
			sgl_c3f(0, 0, 0);
			renderBox(vf3d(0, 0, 0), vf3d(game.getWidth(), game.getHeight(), game.getDepth()));

			renderFaces();

			renderCursor();

			realizeNumberBillboards();
			realizeParticleBillboards();
			renderBillboards();

			//2d rendering
			sgl_defaults();
			sgl_load_pipeline(pip2d);
			sgl_matrix_mode_projection();
			sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);
			sgl_matrix_mode_modelview();
			sgl_load_identity();

			renderStateOverlay();

			sgl_draw();

			sg_end_pass();
		}

		//post process
		{
			sg_pass pass{};
			pass.swapchain=sglue_swapchain();
			sg_begin_pass(pass);

			sg_apply_pipeline(post_process.crt_pip);

			sg_bindings bind{};
			bind.vertex_buffers[0]=post_process.vbuf;
			bind.samplers[SMP_u_crt_smp]=sampler;
			bind.views[VIEW_u_crt_tex]=post_process.rt.color_tex;
			sg_apply_bindings(bind);

			fs_crt_params_t fs_crt_params{};
			fs_crt_params.u_resolution[0]=sapp_widthf();
			fs_crt_params.u_resolution[1]=sapp_heightf();
			sg_apply_uniforms(UB_fs_crt_params, SG_RANGE(fs_crt_params));

			sg_draw(0, 4, 1);

			sg_end_pass();
		}

		sg_commit();

		return true;
	}
};