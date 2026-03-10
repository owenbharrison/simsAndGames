#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include <list>
#include <vector>
//for time
#include <ctime>

#include "cmn/math/v2d.h"

#include "constraints.h"

#include "cmn/utils.h"

#include "render_utils.h"

#include "imgui/include/imgui_singleheader.h"
#define SOKOL_IMGUI_IMPL
#include "sokol/include/sokol_imgui.h"

using cmn::vf2d;

static float degToRad(float deg) {
	return deg/180*cmn::Pi;
}

static float radToDeg(float rad) {
	return rad/cmn::Pi*180;
}

static float fract(float x) {
	return x-std::floor(x);
}

//https://www.shadertoy.com/view/4djSRW
void hash31(float i, float& a, float& b, float& c) {
	float x=fract(.1031*i);
	float y=fract(.1030*i);
	float z=fract(.0973*i);
	float d=x*(33.33+y)+y*(33.33+z)+z*(33.33+x);
	x+=d, y+=d, z+=d;
	a=fract(z*(x+y));
	b=fract(y*(z+x));
	c=fract(x*(y+z));
}

//fisher-yates shuffle
template<typename T>
void shuffle(std::vector<T>& vec) {
	for(int i=vec.size()-1; i>=1; i--) {
		int j=std::rand()%(i+1);
		std::swap(vec[i], vec[j]);
	}
}

class SketcherUI : public cmn::SokolEngine {
	std::list<vf2d> pts;
	vf2d* held_pt=nullptr;
	const float pt_rad=11;

	struct Link {
		vf2d* a, * b, * c;
		float len, angle;
	};
	std::list<Link> links;
	float link_edge_col[3]{0, 0, 0};

	bool imguiing=false;

	float bg_col[3]{.2f, .2f, .2f};
	float grid_col[3]{.349f, .349f, .349f};

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
	
	void setupImGui() {
		simgui_desc_t simgui_desc{};
		simgui_desc.ini_filename="assets/imgui.ini";
		simgui_setup(simgui_desc);
	}

	//generalized hoberman linkage construction
	void makeHoberman(int num) {
		held_pt=nullptr;

		const vf2d ctr=vf2d(sapp_width(), sapp_height())/2;

		//depth=3?
		auto ix=[] (int i, int j) { return 3*i+j; };

		//asymptotically decrease len w/ increasing num 
		//starting from minimum num of 4
		float min_len=60, max_len=125;
		float t=std::exp(-.4f*(num-4));
		float len=min_len+t*(max_len-min_len);

		float inc_angle=2*cmn::Pi/num;

		//allocate and insert into lookup
		pts.clear();
		vf2d** grid=new vf2d*[3*num];
		for(int i=0; i<num; i++) {
			float base_angle=i*inc_angle;
			vf2d pos=ctr;
			for(int j=0; j<3; j++) {
				float angle=base_angle+j*inc_angle;
				pos+=cmn::polar<vf2d>(len, angle);
				pts.push_back(pos);
				grid[ix(i, j)]=&pts.back();
			}
		}

		//bottom then top
		links.clear();
		for(int i=0; i<num; i++) {
			auto& r1=grid[ix(i, 1)], & r2=grid[ix(i, 2)];
			links.push_back({grid[ix(i, 0)], r1, r2, len, inc_angle});
		}
		for(int i=0; i<num; i++) {
			auto& l1=grid[ix((i+num-1)%num, 1)], & l2=grid[ix((i+num-2)%num, 2)];
			links.push_back({grid[ix(i, 0)], l1, l2, len, -inc_angle});
		}

		delete[] grid;
	}
#pragma endregion

	void userCreate() override {
		setupEnvironment();

		//dont choose 6.
		int num;
		do num=cmn::randInt(4, 10);
		while(num==6);
		makeHoberman(num);
		
		setupSGL();

		setupImGui();
	}

#pragma region UPDATE HELPERS
	void handlePointMovement() {
		if(imguiing) return;
		
		const vf2d mouse_pos(mouse_x, mouse_y);
		
		const auto action=getMouse(SAPP_MOUSEBUTTON_LEFT);
		if(action.pressed) {
			held_pt=nullptr;

			float record=-1;
			for(auto& p:pts) {
				float d=(p-mouse_pos).mag();
				if(d<10) {
					if(record<0||d<record) {
						held_pt=&p;
					}
				}
			}
		}
		if(action.held&&held_pt) *held_pt=mouse_pos;
		if(action.released) held_pt=nullptr;
	}

	void handleUserInput() {
		if(getKey(SAPP_KEYCODE_4).pressed) makeHoberman(4);
		if(getKey(SAPP_KEYCODE_5).pressed) makeHoberman(5);
		if(getKey(SAPP_KEYCODE_7).pressed) makeHoberman(7);
		if(getKey(SAPP_KEYCODE_8).pressed) makeHoberman(8);
		if(getKey(SAPP_KEYCODE_9).pressed) makeHoberman(9);
		if(getKey(SAPP_KEYCODE_0).pressed) makeHoberman(10);
	}

	//keep from overlapping
	void updatePoints() {
		//accumulate references
		std::vector<vf2d*> pts_ref;
		for(auto& p:pts) pts_ref.push_back(&p);

		shuffle(pts_ref);

		//check against every other
		float min_dist=2*pt_rad;
		for(int i=0; i<pts_ref.size(); i++) {
			auto& a=*pts_ref[i];
			for(int j=1+i; j<pts_ref.size(); j++) {
				auto& b=*pts_ref[j];

				//push apart if too close
				float mag_sq=(b-a).mag_sq();
				if(mag_sq<min_dist*min_dist) {
					constrain::dist(a, b, min_dist);
				}
			}
		}
	}

	void updateLinks() {
		//accumulate references
		std::vector<Link*> links_ref;
		for(auto& l:links) links_ref.push_back(&l);

		shuffle(links_ref);

		//constrain
		for(auto& l:links_ref) {
			auto& a=*l->a, & b=*l->b;
			auto& c=*l->b, & d=*l->c;
			constrain::dist(a, b, l->len);
			constrain::dist(c, d, l->len);
			constrain::angle(a, b, c, d, l->angle);
		}
	}
#pragma endregion

	void userUpdate(float dt) override {
		handleUserInput();
		
		for(int i=0; i<25; i++) {
			handlePointMovement();

			updatePoints();
			updateLinks();
		}
	}

	void userInput(const sapp_event* e) override {
		simgui_handle_event(e);
	}

#pragma region RENDER HELPERS
	void renderGrid(sg_color col) {
		const float res=25;
		const float w=3;
		const int ratio=5;

		//vertical lines
		int num_x=1+sapp_widthf()/res;
		for(int i=0; i<num_x; i++) {
			float x=res*i;
			vf2d top(x, 0), btm(x, sapp_heightf());
			if(i%ratio==0) sgl_fill_line(top.x, top.y, btm.x, btm.y, w, col);
			else sgl_draw_line(top.x, top.y, btm.x, btm.y, col);
		}

		//horizontal lines
		int num_y=1+sapp_heightf()/res;
		for(int j=0; j<num_y; j++) {
			float y=res*j;
			vf2d lft(0, y), rgt(sapp_widthf(), y);
			if(j%ratio==0) sgl_fill_line(lft.x, lft.y, rgt.x, rgt.y, w, col);
			else sgl_draw_line(lft.x, lft.y, rgt.x, rgt.y, col);
		}
	}

	void renderLinks() {
		auto renderLink=[&] (const Link& l, float w, sg_color col) {
			const auto& ax=l.a->x, ay=l.a->y;
			const auto& bx=l.b->x, by=l.b->y;
			const auto& cx=l.c->x, cy=l.c->y;
			sgl_fill_circle(ax, ay, w/2, col);
			sgl_fill_line(ax, ay, bx, by, w, col);
			sgl_fill_circle(bx, by, w/2, col);
			sgl_fill_line(bx, by, cx, cy, w, col);
			sgl_fill_circle(cx, cy, w/2, col);
		};

		int i=0;
		const float w=2*pt_rad;
		for(const auto& l:links) {
			renderLink(l, w, {link_edge_col[0], link_edge_col[1], link_edge_col[2], 1});

			sg_color col{0, 0, 0, 1};
			hash31(1000+7*(i++), col.r, col.g, col.b);
			renderLink(l, .8f*w, col);
		}
	}
	
	void renderImGui() {
		simgui_frame_desc_t simgui_frame_desc{};
		simgui_frame_desc.width=sapp_width();
		simgui_frame_desc.height=sapp_height();
		simgui_frame_desc.delta_time=sapp_frame_duration();
		simgui_frame_desc.dpi_scale=sapp_dpi_scale();
		simgui_new_frame(simgui_frame_desc);

		imguiing=false;

		ImGui::Begin("Grid Options");
		imguiing|=ImGui::IsWindowHovered();
		ImGui::ColorEdit3("bg", bg_col);
		ImGui::ColorEdit3("grid", grid_col);
		ImGui::End();

		ImGui::Begin("Link Options");
		imguiing|=ImGui::IsWindowHovered();
		ImGui::ColorEdit3("edge", link_edge_col);
		ImGui::End();

		simgui_render();
	}
#pragma endregion

	void userRender() override {
		sg_pass pass{};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);
		
		{
			sgl_defaults();

			sgl_matrix_mode_projection();
			sgl_load_identity();
			sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);

			sgl_matrix_mode_modelview();
			sgl_load_identity();

			//background
			sgl_fill_rect(0, 0, sapp_width(), sapp_height(), {bg_col[0], bg_col[1], bg_col[2], 1});

			renderGrid({grid_col[0], grid_col[1], grid_col[2], 1});

			renderLinks();

			sgl_draw();
		}

		renderImGui();

		sg_end_pass();

		sg_commit();
	}

	void userDestroy() override {
		simgui_shutdown();
		sg_shutdown();
	}

	SketcherUI() {
		app_title="Sketcher UI Demo";
	}
};