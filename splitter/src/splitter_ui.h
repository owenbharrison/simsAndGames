#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "mesh.h"

#include "sokol/render_utils.h"

#include <list>

#include <ctime>

using cmn::vf2d;

class SplitterUI : public cmn::SokolEngine {
	std::list<Mesh> meshes;

	bool render_wireframes=true;

	vf2d* split_st=nullptr;

public:
#pragma region SETUP_HELPERS
	void setupEnvironment() {
		sg_desc desc{};
		sg_setup(desc);
	}

	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_setup(&sgl_desc);
	}
#pragma endregion

	void userCreate() override {
		setupEnvironment();

		setupSGL();

		meshes.push_back(Mesh::makeTorus(100, 60, 24, {140, 180}, 1, 0, 0));
		meshes.push_back(Mesh::makeRect(120, 200, {340, 180}, 0, 1, 0));
		//squiggly mesh
		Mesh m;
		m.verts={
			{-100, -100}, {20, -100}, {20, 60},
			{60, 60}, {60, -100}, {100, -100},
			{100, 100}, {-20, 100}, {-20, -60},
			{-60, -60}, {-60, 100}, {-100, 100},
		};
		m.tris={
			{0, 1, 8}, {1, 2, 8}, {2, 7, 8}, {2, 3, 7}, {3, 6, 7},
			{3, 5, 6}, {3, 4, 5}, {0, 8, 9}, {0, 9, 10}, {0, 10, 11}
		};
		m.pos={540, 180};
		m.r=0, m.g=0, m.b=1;
		meshes.push_back(m);
	}

	void userUpdate(float dt) override {
		const vf2d mouse_pos(mouse_x, mouse_y);
		
		std::srand(std::time(0));

		const auto split_action=getMouse(SAPP_MOUSEBUTTON_LEFT);
		if(split_action.pressed) split_st=new vf2d(.1f+mouse_pos);
		if(split_action.released) {
			delete split_st;
			split_st=nullptr;
		}

		if(getKey(SAPP_KEYCODE_W).pressed) render_wireframes^=true;
	}

	void fillMesh(const Mesh& m) {
		//first transform points
		std::vector<vf2d> verts;
		for(const auto& v:m.verts) {
			verts.push_back(m.loc2wld(v));
		}

		//connect em up
		sg_color col{m.r, m.g, m.b, 1};
		for(const auto& t:m.tris) {
			const auto& a=verts[t.ix[0]], & b=verts[t.ix[1]], & c=verts[t.ix[2]];
			cmn::fill_triangle(a.x, a.y, b.x, b.y, c.x, c.y, col);
		}
	}

	void drawMesh(const Mesh& m, sg_color col) {
		//first transform points
		std::vector<vf2d> verts;
		for(const auto& v:m.verts) {
			verts.push_back(m.loc2wld(v));
		}

		//connect em up
		for(const auto& t:m.tris) {
			const auto& a=verts[t.ix[0]], & b=verts[t.ix[1]], & c=verts[t.ix[2]];
			cmn::draw_triangle(a.x, a.y, b.x, b.y, c.x, c.y, col);
		}
	}

	void userRender() override {
		//display pass
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={1, 1, 1, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();

		//pixel space
		sgl_matrix_mode_projection();
		sgl_load_identity();
		sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);
		sgl_matrix_mode_modelview();
		sgl_load_identity();
		
		const sg_color black{0, 0, 0, 1};

		for(const auto& m:meshes) {
			fillMesh(m);
			if(render_wireframes) drawMesh(m, black);
		}

		const vf2d mouse_pos(mouse_x, mouse_y);
		if(split_st) {
			vf2d tang=(*split_st-mouse_pos).norm();
			vf2d norm(-tang.y, tang.x);

			for(const auto& m:meshes) {
				auto split=m.split(*split_st, norm);
				for(const auto& s:split) {
					fillMesh(s);
					if(render_wireframes) drawMesh(s, black);
				}
			}

			//draw plane
			const sg_color red{1, 0, 0, 1};
			float rad=10;
			vf2d top=*split_st+rad*norm, btm=*split_st-rad*norm;
			cmn::draw_line(top.x, top.y, btm.x, btm.y, red);
			float len=50;
			vf2d lft=*split_st-len*tang, rgt=*split_st+len*tang;
			cmn::draw_line(lft.x, lft.y, rgt.x, rgt.y, red);
		}

		sgl_draw();

		sg_end_pass();

		sg_commit();
	}

	void userDestroy() override {
		sgl_shutdown();
		sg_shutdown();
	}

	SplitterUI() {
		app_title="Splitter UI Demo";
	}
};