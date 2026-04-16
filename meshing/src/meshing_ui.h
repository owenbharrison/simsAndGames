#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

//for time
#include <ctime>

#include "cmn/math/v2d.h"

#include "poisson.h"

#include "delaunay.h"

#include "sokol/render_utils.h"

using cmn::vf2d;

void stressGradient(float t, float& r, float& g, float& b) {
	static const float cols[5][3]{
		{0, 0, 1},//blue
		{0, 1, 1},//cyan
		{0, 1, 0},//green
		{1, 1, 0},//yellow
		{1, 0, 0}//red
	};
	static const int num=sizeof(cols)/sizeof(cols[0]);

	//clamp percent
	if(t<0) t=0;
	if(t>=1) t=.999f;

	//floor index, fract t
	float x=t*(num-1);
	int i=x;
	t=x-i;

	//lerp cols
	const auto& c1=cols[i];
	const auto& c2=cols[i+1];
	r=c1[0]+t*(c2[0]-c1[0]);
	g=c1[1]+t*(c2[1]-c1[1]);
	b=c1[2]+t*(c2[2]-c1[2]);
}

float snapTo(float a, float b) {
	return b*std::round(a/b);
}

class MeshingUI : public cmn::SokolEngine {
	vf2d mouse_pos;

	const float rad=7;
	std::vector<vf2d> shell, verts;
	std::vector<Edge> constraints;
	std::vector<Tri> tris;

	bool show_quality=true;
	bool show_constraints=true;
	bool show_grid=true;
	const float grid_sz=20;

public:
#pragma region SETUP_HELPERS
	void setupEnvironment() {
		sg_desc desc{};
		sg_setup(desc);
	}

	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_desc.max_commands=200000;
		sgl_desc.max_vertices=500000;
		sgl_setup(&sgl_desc);
	}

	void userCreate() override {
		std::srand(std::time(0));

		setupEnvironment();

		setupSGL();
	}

	void handleAddAction() {
		const auto action=getMouse(SAPP_MOUSEBUTTON_LEFT);
		if(action.pressed) {
			shell.clear(), verts.clear();
			constraints.clear();
			tris.clear();
		}

		if(action.held) shell.push_back(mouse_pos);

		if(action.released) {
			//ensure verts are spaced apart
			auto isUnique=[&] (const vf2d& p) {
				for(const auto& v:verts) {
					if((v-p).mag_sq()<rad*rad) {
						return false;
					}
				}
				return true;
			};

			//make shell
			for(int i=0; i<shell.size(); i++) {
				int j=(i+1)%shell.size();
				const auto& a=shell[i], ab=shell[j]-a;

				//ensure verts placed along jumps
				const int num=100;
				for(int k=0; k<num; k++) {
					float t=k/(num-1.f);
					vf2d p=a+t*ab;
					if(isUnique(p)) verts.push_back(p);
				}
			}
			shell.clear();

			//wind shell
			for(int i=0; i<verts.size(); i++) {
				int j=(i+1)%verts.size();
				constraints.push_back({i, j});
			}

			//ray-polygon test
			auto insideShell=[&] (const vf2d& p) {
				//random direction
				float angle=cmn::randFloat(2*cmn::Pi);
				vf2d pr=p+cmn::polar<vf2d>(1, angle);

				int num=0;
				for(const auto& c:constraints) {
					const auto& a=verts[c[0]], & b=verts[c[1]];
					//treat as ray
					vf2d tu=cmn::lineLineIntersection(p, pr, a, b);
					if(tu.x>0&&tu.y>0&&tu.y<1) num++;
				}

				//odd? inside!
				return num%2;
			};

			//add verts inside shell
			cmn::vf2d min(1e30f, 1e30f);
			cmn::vf2d max(-1e30f, -1e30f);
			for(const auto& p:verts) {
				if(p.x<min.x) min.x=p.x;
				if(p.y<min.y) min.y=p.y;
				if(p.x>max.x) max.x=p.x;
				if(p.y>max.y) max.y=p.y;
			}
			auto poisson=poissonDiscSample(min, max, rad);
			for(const auto& p:poisson) {
				if(insideShell(p)&&isUnique(p)) verts.push_back(p);
			}

			//triangulate w.r.t. shell
			auto cdt=constrainedDelaunayTriangulate(verts, constraints);

			//remove tris outside shell
			for(const auto& t:cdt) {
				vf2d c=(verts[t[0]]+verts[t[1]]+verts[t[2]])/3;
				if(insideShell(c)) tris.push_back(t);
			}
		}
	}

	void userUpdate(float dt) override {
		mouse_pos=vf2d(mouse_x, mouse_y);

		//snap mouse pos to grid
		if(getKey(SAPP_KEYCODE_LEFT_SHIFT).held) {
			mouse_pos.x=snapTo(mouse_pos.x, grid_sz);
			mouse_pos.y=snapTo(mouse_pos.y, grid_sz);
		}

		handleAddAction();

		//graphics toggles
		if(getKey(SAPP_KEYCODE_Q).pressed) show_quality^=true;
		if(getKey(SAPP_KEYCODE_C).pressed) show_constraints^=true;
		if(getKey(SAPP_KEYCODE_G).pressed) show_grid^=true;
	}

	void renderGrid(const sg_color& col) {
		const float width=sapp_widthf();
		const float height=sapp_heightf();
		
		//vertical lines
		int num_x=1+width/grid_sz;
		for(int i=0; i<=num_x; i++) {
			float x=grid_sz*i;
			cmn::draw_line(x, 0, x, height, col);
		}

		//horizontal lines
		float num_y=1+height/grid_sz;
		for(int j=0; j<=num_y; j++) {
			float y=grid_sz*j;
			cmn::draw_line(0, y, width, y, col);
		}
	}

	void renderShell() {
		for(int i=0; i<shell.size(); i++) {
			int j=(i+1)%shell.size();
			const auto& a=shell[i], b=shell[j];
			cmn::draw_line(a.x, a.y, b.x, b.y, {1, 0, 0, 1});
		}
	}

	void renderTris() {
		for(const auto& t:tris) {
			const auto& v1=verts[t[0]];
			const auto& v2=verts[t[1]];
			const auto& v3=verts[t[2]];

			//default to white
			float r=1, g=1, b=1;
			if(show_quality) {
				//find aspect ratio
				float la=(v1-v2).mag();
				float lb=(v2-v3).mag();
				float lc=(v3-v1).mag();
				float s=(la+lb+lc)/2;
				float asp=la*lb*lc/8/(s-la)/(s-lb)/(s-lc);

				//color based on how close to 1
				float t=1-std::exp(1.3f*(1-asp));
				stressGradient(t, r, g, b);
			}

			cmn::fill_triangle(
				v1.x, v1.y,
				v2.x, v2.y,
				v3.x, v3.y,
				{r, g, b, 1}
			);
		}

		for(const auto& t:tris) {
			const auto& a=verts[t[0]];
			const auto& b=verts[t[1]];
			const auto& c=verts[t[2]];
			cmn::draw_triangle(
				a.x, a.y,
				b.x, b.y,
				c.x, c.y,
				{0, 0, 0, 1}
			);
		}
	}

	void renderConstraints() {
		for(const auto& c:constraints) {
			const auto& a=verts[c[0]];
			const auto& b=verts[c[1]];
			cmn::draw_line(a.x, a.y, b.x, b.y, {1, 1, 1, 1});
		}
	}

	void userRender() override {
		//display pass
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={0, 0, 0, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();

		//pixel space
		sgl_matrix_mode_projection();
		sgl_load_identity();
		sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);
		sgl_matrix_mode_modelview();
		sgl_load_identity();

		cmn::fill_rect(0, 0, sapp_widthf(), sapp_heightf(), {.34f, .34f, .34f, 1});

		if(show_grid) renderGrid({.61f, .61f, .61f, 1});

		renderShell();

		renderTris();

		if(show_constraints) renderConstraints();

		sgl_draw();

		sg_end_pass();

		sg_commit();
	}

	void userDestroy() override {
		sgl_shutdown();
		sg_shutdown();
	}

	MeshingUI() {
		app_title="Meshing UI Demo";
	}
};