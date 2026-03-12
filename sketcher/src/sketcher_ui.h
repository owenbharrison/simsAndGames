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
#include <string>

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

//hue=color wheel, saturation=whitewash, value=blackwash
//https://www.rapidtables.com/convert/color/hsv-to-rgb.html
static sg_color hsv2rgb(int h, float s, float v) {
	float c=v*s;
	float x=c*(1-std::abs(1-std::fmod(h/60.f, 2)));
	float m=v-c;
	float r=0, g=0, b=0;
	switch(h/60) {
		case 0: r=c, g=x, b=0; break;
		case 1: r=x, g=c, b=0; break;
		case 2: r=0, g=c, b=x; break;
		case 3: r=0, g=x, b=c; break;
		case 4: r=x, g=0, b=c; break;
		case 5: r=c, g=0, b=x; break;
	}
	return {m+r, m+g, m+b, 1};
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
	float pt_rad=7.5f;
	bool push_pts_apart=true;

	struct DistConstraint {
		vf2d* a, * b;
		float len;
		sg_color col;
	};
	std::list<DistConstraint> dist_constraints;
	struct AngleConstraint {
		vf2d* a, * b, * c, * d;
		float angle;
	};
	std::list<AngleConstraint> angle_constraints;

	bool imguiing=true;

	bool render_grid=true;
	bool render_outlines=true;
	int hoberman_num=0;
	float outline_rad=2;
	float outline_rgb[3]{0, 0, 0};
	float background_rgb[3]{0.992f, .988f, .847f};
	float grid_rgb[3]{.580f, .831f, .847f};

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
	void makeHobermanLinkage(int num) {
		held_pt=nullptr;

		if(num<3) return;

		const vf2d ctr=vf2d(sapp_width(), sapp_height())/2;

		//depth=3?
		auto ix=[] (int i, int j) { return 3*i+j; };

		//asymptotically decrease len w/ increasing num 
		//starting from minimum num of 3
		float min_len=40, max_len=150;
		float t=std::exp(-.1f*(num-3));
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

		dist_constraints.clear();
		std::list<DistConstraint> dist_other;
		angle_constraints.clear();

		auto randCol=[&] () {
			int h=std::rand()%360;
			float s=cmn::randFloat(.5f, 1);
			float v=1;
			return hsv2rgb(h, s, v);
		};

		//branch out
		for(int i=0; i<num; i++) {
			auto& c=grid[ix(i, 0)];
			//next 2
			auto& n1=grid[ix(i, 1)];
			auto& n2=grid[ix(i, 2)];
			sg_color col1=randCol();
			dist_constraints.push_back({c, n1, len, col1});
			dist_constraints.push_back({n1, n2, len, col1});
			angle_constraints.push_back({c, n1, n1, n2, inc_angle});

			//previous 2
			auto& p1=grid[ix((i+num-1)%num, 1)];
			auto& p2=grid[ix((i+num-2)%num, 2)];
			sg_color col2=randCol();
			dist_other.push_back({c, p1, len, col2});
			dist_other.push_back({p1, p2, len, col2});
			angle_constraints.push_back({c, p1, p1, p2, -inc_angle});
		}

		//render as "two layers"
		dist_constraints.insert(dist_constraints.end(),
			dist_other.begin(), dist_other.end()
		);

		delete[] grid;
	}

	void setupScene() {
		//dont choose 6.
		do hoberman_num=cmn::randInt(4, 10);
		while(hoberman_num==6);
		makeHobermanLinkage(hoberman_num);
	}
#pragma endregion

	void userCreate() override {
		std::srand(std::time(0));

		setupEnvironment();

		setupScene();

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

	//keep from overlapping
	void pushPointsApart() {
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

	void updateDistConstraints() {
		//randomize order
		std::vector<DistConstraint*> dist_refs;
		for(auto& d:dist_constraints) dist_refs.push_back(&d);
		shuffle(dist_refs);

		for(const auto& d:dist_refs) {
			constrain::dist(*d->a, *d->b, d->len);
		}
	}

	void updateAngleConstraints() {
		//randomize order
		std::vector<AngleConstraint*> angle_refs;
		for(auto& a:angle_constraints) angle_refs.push_back(&a);
		shuffle(angle_refs);

		for(const auto& a:angle_refs) {
			constrain::angle(*a->a, *a->b, *a->c, *a->d, a->angle);
		}
	}
#pragma endregion

	void userUpdate(float dt) override {
		for(int i=0; i<25; i++) {
			handlePointMovement();

			if(push_pts_apart) pushPointsApart();
			updateDistConstraints();
			updateAngleConstraints();
		}
	}

	void userInput(const sapp_event* e) override {
		simgui_handle_event(e);
	}

#pragma region RENDER HELPERS
	void renderGrid(const sg_color& col) {
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

	void renderDistConstraints() {
		auto renderSlot=[](
			const cmn::vf2d& a, const cmn::vf2d& b,
			float r, const sg_color& col
			) {
			sgl_fill_circle(a.x, a.y, r, col);
			sgl_fill_line(a.x, a.y, b.x, b.y, 2*r, col);
			sgl_fill_circle(b.x, b.y, r, col);
		};

		//outlines
		if(render_outlines) {
			const sg_color col{
				outline_rgb[0],
				outline_rgb[1],
				outline_rgb[2],
				1
			};
			for(const auto& d:dist_constraints) {
				renderSlot(*d.a, *d.b, pt_rad+outline_rad, col);
			}
		}

		//insides
		for(const auto& d:dist_constraints) {
			renderSlot(*d.a, *d.b, pt_rad, d.col);
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

		ImGui::Begin("Physics Options");
		imguiing|=ImGui::IsWindowHovered();
		ImGui::Checkbox("Push Points Apart", &push_pts_apart);
		ImGui::SliderFloat("Point Radius", &pt_rad, 5, 10);
		ImGui::End();

		ImGui::Begin("Scene Options");
		imguiing|=ImGui::IsWindowHovered();
		if(ImGui::SliderInt("Make Hoberman", &hoberman_num, 3, 36)) {
			makeHobermanLinkage(hoberman_num);
		}
		ImGui::End();

		ImGui::Begin("Display Options");
		imguiing|=ImGui::IsWindowHovered();
		ImGui::Checkbox("Render Grid", &render_grid);
		ImGui::Checkbox("Render Outlines", &render_outlines);
		ImGui::SliderFloat("Outline Radius", &outline_rad, 1, 5);
		ImGui::ColorEdit3("Background", background_rgb);
		if(render_grid) ImGui::ColorEdit3("Grid", grid_rgb);
		if(render_outlines) ImGui::ColorEdit3("Outlines", outline_rgb);
		ImGui::End();

		simgui_render();
	}
#pragma endregion

	void userRender() override {
		sg_pass pass{};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		//begin sgl rendering
		{
			sgl_defaults();

			//pixel space
			sgl_matrix_mode_projection();
			sgl_load_identity();
			sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);
			sgl_matrix_mode_modelview();
			sgl_load_identity();

			//background
			sgl_fill_rect(0, 0, sapp_width(), sapp_height(), {
				background_rgb[0],
				background_rgb[1],
				background_rgb[2],
				1
				});

			if(render_grid) renderGrid({
				grid_rgb[0],
				grid_rgb[1],
				grid_rgb[2],
				1
				});

			renderDistConstraints();

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