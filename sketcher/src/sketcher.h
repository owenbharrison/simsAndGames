#define SOKOL_IMPL
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/include/sokol_app.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "sokol/sokol_engine.h"

#include "shd.glsl.h"

#include <list>
#include <vector>
#include <string>

//for time
#include <ctime>

#include "cmn/math/v2d.h"

#include "constraints.h"

#include "cmn/utils.h"

#include "sokol/render_utils.h"

#include "imgui/include/imgui_singleheader.h"
#include "sokol/include/sokol_imgui.h"

#include "render_target.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/include/stb_image.h"

using cmn::vf2d;

//hue=color wheel, saturation=whitewash, value=blackwash
//https://www.rapidtables.com/convert/color/hsv-to-rgb.html
static void hsv2rgb(
	int h, float s, float v,
	float& r, float& g, float& b
) {
	float c=v*s;
	float x=c*(1-std::abs(1-std::fmod(h/60.f, 2)));
	float m=v-c;
	switch(h/60) {
		default: r=0, g=0, b=0; break;
		case 0: r=m+c, g=m+x, b=m+0; break;
		case 1: r=m+x, g=m+c, b=m+0; break;
		case 2: r=m+0, g=m+c, b=m+x; break;
		case 3: r=m+0, g=m+x, b=m+c; break;
		case 4: r=m+x, g=m+0, b=m+c; break;
		case 5: r=m+c, g=m+0, b=m+x; break;
	}
}

//fisher-yates shuffle
template<typename T>
void shuffle(std::vector<T>& vec) {
	for(int i=vec.size()-1; i>=1; i--) {
		int j=std::rand()%(i+1);
		std::swap(vec[i], vec[j]);
	}
}

class Sketcher : public cmn::SokolEngine {
	sg_sampler sampler{};

	sgl_pipeline sgl_pip{};

	RenderTarget rt;
	
	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};
	} outline_render;

	//scene stuff
	std::list<vf2d> points;
	vf2d* held_pt=nullptr;
	float point_rad=7.5f;
	bool push_pts_apart=true;

	std::list<DistConstraint> dist_constraints;
	std::list<AngleConstraint> angle_constraints;

	//graphics stuff
	bool render_grid=true;
	float outline_rad=4.5f;
	float point_rgb[3]{.917f, .917f, .917f};
	float outline_rgb[3]{.25f, .25f, .25f};
	float bkgd_rgb[3]{0.992f, .988f, .847f};
	float grid_rgb[3]{.580f, .831f, .847f};

	bool imguiing=false;

public:
#pragma region SETUP_HELPERS
	void setupSampler() {
		sg_sampler_desc sampler_desc{};
		sampler_desc.wrap_u=SG_WRAP_CLAMP_TO_EDGE;
		sampler_desc.wrap_v=SG_WRAP_CLAMP_TO_EDGE;
		sampler=sg_make_sampler(sampler_desc);
	}

	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_setup(&sgl_desc);

		sg_pipeline_desc pip_desc{};
		pip_desc.colors[0].write_mask=SG_COLORMASK_RGBA;
		sgl_pip=sgl_make_pipeline(pip_desc);
	}

	void setupOutlineRender() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_outline_i_pos].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(outline_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		outline_render.pip=sg_make_pipeline(pip_desc);

		float vertexes[4][2]{{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
		sg_buffer_desc buffer_desc{};
		buffer_desc.data=SG_RANGE(vertexes);
		outline_render.vbuf=sg_make_buffer(buffer_desc);
	}

	//generalized hoberman linkage construction
	void makeHobermanLinkage(int num, float len) {
		held_pt=nullptr;

		if(num<3) return;

		const vf2d ctr=vf2d(sapp_width(), sapp_height())/2;

		//geometric angles
		const float alpha=cmn::Pi*(1-2.f/num);
		const float beta=2*cmn::Pi/num;
		const float gamma=alpha/4+beta/2;
		const float delta=cmn::Pi-alpha;

		//starting dist from ctr
		const float rad=len*std::sin(alpha/4)/std::sin(beta/2);

		//allocate and insert into lookup
		points.clear();
		vf2d** grid=new vf2d*[3*num];
		auto ix=[] (int i, int j) { return 3*i+j; };
		for(int i=0; i<num; i++) {
			float angle1=2*cmn::Pi*i/num;
			float angle2=angle1+gamma;
			float angle3=angle2+delta;
			vf2d p[3];
			p[0]=ctr+vf2d::polar({rad, angle1});
			p[1]=p[0]+vf2d::polar({len, angle2});
			p[2]=p[1]+vf2d::polar({len, angle3});
			for(int j=0; j<3; j++) {
				points.push_back(p[j]);
				grid[ix(i, j)]=&points.back();
			}
		}

		auto randCol=[&] (float& r, float& g, float& b) {
			int h=std::rand()%360;
			float s=cmn::randFloat(.5f, 1);
			float v=1;
			hsv2rgb(h, s, v, r, g, b);
		};

		//branch out
		std::list<DistConstraint> dist_top, dist_btm;
		angle_constraints.clear();
		float r, g, b;
		for(int i=0; i<num; i++) {
			auto& c=grid[ix(i, 0)];
			//next 2
			auto& n1=grid[ix(i, 1)];
			auto& n2=grid[ix(i, 2)];
			randCol(r, g, b);
			dist_top.push_back({c, n1, len, {r, g, b}});
			dist_top.push_back({n1, n2, len, {r, g, b}});
			angle_constraints.push_back({c, n1, n1, n2, delta});

			//previous 2
			auto& p1=grid[ix((i+num-1)%num, 1)];
			auto& p2=grid[ix((i+num-2)%num, 2)];
			randCol(r, g, b);
			dist_btm.push_back({c, p1, len, {r, g, b}});
			dist_btm.push_back({p1, p2, len, {r, g, b}});
			angle_constraints.push_back({c, p1, p1, p2, -delta});
		}

		//render as "two layers"
		dist_constraints.clear();
		for(const auto& t:dist_top) dist_constraints.push_back(t);
		for(const auto& b:dist_btm) dist_constraints.push_back(b);

		delete[] grid;
	}

	void setupScene() {
		makeHobermanLinkage(7, 100.f);
	}

	void setupImGui() {
		simgui_desc_t simgui_desc{};
		simgui_desc.ini_filename="assets/imgui.ini";
		simgui_setup(simgui_desc);
	}

	bool setupIcon() {
		int width, height, comp;
		stbi_uc* pixels=stbi_load("assets/icon.png", &width, &height, &comp, 4);
		if(!pixels) return false;

		sapp_icon_desc icon_desc{};
		icon_desc.images[0].width=width;
		icon_desc.images[0].height=height;
		icon_desc.images[0].pixels.ptr=pixels;
		icon_desc.images[0].pixels.size=sizeof(stbi_uc)*4*width*height;
		sapp_set_icon(&icon_desc);

		stbi_image_free(pixels);

		return true;
	}
#pragma endregion

	bool user_create() override {
		app_title="Sketcher";

		std::srand(std::time(0));

		setupSampler();

		setupSGL();

		rt.resize(sapp_width(), sapp_height());

		setupOutlineRender();

		setupScene();

		setupImGui();

		if(!setupIcon()) return false;

		return true;
	}

#pragma region UPDATE HELPERS
	void handlePointMovement() {
		if(imguiing) return;

		const vf2d mouse_pos(GetMouseX(), GetMouseY());

		const auto action=GetMouse(SAPP_MOUSEBUTTON_LEFT);
		if(action.pressed) {
			held_pt=nullptr;

			float record=-1;
			for(auto& p:points) {
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
		for(auto& p:points) pts_ref.push_back(&p);

		shuffle(pts_ref);

		//check against every other
		float min_dist=2*point_rad;
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

	bool user_update(float dt) override {
		for(int i=0; i<25; i++) {
			handlePointMovement();

			if(push_pts_apart) pushPointsApart();
			updateDistConstraints();
			updateAngleConstraints();
		}

		return true;
	}

	void user_input(const sapp_event* e) override {
		switch(e->type) {
			case SAPP_EVENTTYPE_RESIZED:
				rt.resize(sapp_width(), sapp_height());
				break;
		}

		simgui_handle_event(e);
	}

#pragma region RENDERERS
	void renderGrid(float r, float g, float b, float a) {
		const float res=25;
		const float width=3;
		const int ratio=5;

		//vertical lines
		int num_x=1+sapp_widthf()/res;
		for(int i=0; i<num_x; i++) {
			float x=res*i;
			vf2d top(x, 0), btm(x, sapp_heightf());
			if(i%ratio==0) cmn::draw_thick_line(
				top.x, top.y, btm.x, btm.y,
				width,
				r, g, b, a
			);
			else cmn::draw_line(top.x, top.y, btm.x, btm.y, r, g, b, a);
		}

		//horizontal lines
		int num_y=1+sapp_heightf()/res;
		for(int j=0; j<num_y; j++) {
			float y=res*j;
			vf2d lft(0, y), rgt(sapp_widthf(), y);
			if(j%ratio==0) cmn::draw_thick_line(
				lft.x, lft.y, rgt.x, rgt.y,
				width,
				r, g, b, a
			);
			else cmn::draw_line(lft.x, lft.y, rgt.x, rgt.y, r, g, b, a);
		}
	}

	void renderIntoTarget() {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={0, 0, 0, 0};
		pass.attachments.colors[0]=rt.color_attach;
		pass.attachments.depth_stencil=rt.depth_attach;
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_matrix_mode_projection();
		sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);

		sgl_load_pipeline(sgl_pip);

		//alpha=0 = background(no outline)
		cmn::fill_rect(
			0, 0, sapp_widthf(), sapp_heightf(),
			bkgd_rgb[0], bkgd_rgb[1], bkgd_rgb[2], 0
		);

		if(render_grid) renderGrid(grid_rgb[0], grid_rgb[1], grid_rgb[2], 0);

		//alpha=1 = foreground(outlines)

		for(const auto& d:dist_constraints) {
			cmn::draw_thick_line(
				d.a->x, d.a->y, d.b->x, d.b->y,
				2*point_rad,
				d.rgb[0], d.rgb[1], d.rgb[2], 1
			);
		}

		//render points
		for(const auto& p:points) {
			cmn::fill_circle(
				p.x, p.y, point_rad,
				point_rgb[0], point_rgb[1], point_rgb[2]
			);
		}

		sgl_draw();

		sg_end_pass();
	}

	void renderOutlines() {
		sg_apply_pipeline(outline_render.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=outline_render.vbuf;
		bind.samplers[SMP_b_outline_smp]=sampler;
		bind.views[VIEW_b_outline_tex]=rt.color_tex;
		sg_apply_bindings(bind);

		p_fs_outline_t p_fs_outline{};
		p_fs_outline.u_resolution[0]=sapp_widthf();
		p_fs_outline.u_resolution[1]=sapp_heightf();
		p_fs_outline.u_rad=outline_rad;
		p_fs_outline.u_col[0]=outline_rgb[0];
		p_fs_outline.u_col[1]=outline_rgb[1];
		p_fs_outline.u_col[2]=outline_rgb[2];
		sg_apply_uniforms(UB_p_fs_outline, SG_RANGE(p_fs_outline));

		sg_draw(0, 4, 1);
	}

	void renderImGui() {
		simgui_frame_desc_t simgui_frame_desc{};
		simgui_frame_desc.width=sapp_width();
		simgui_frame_desc.height=sapp_height();
		simgui_frame_desc.delta_time=sapp_frame_duration();
		simgui_frame_desc.dpi_scale=sapp_dpi_scale();
		simgui_new_frame(simgui_frame_desc);

		imguiing=false;

		ImGui::Begin("Display Options");
		{
			ImGui::Checkbox("Render Grid", &render_grid);
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("Outline Radius", &outline_rad, 0, 6);
			ImGui::ColorEdit3("Background", bkgd_rgb);
			ImGui::ColorEdit3("Grid Lines", grid_rgb);
			ImGui::ColorEdit3("Points", point_rgb);
			ImGui::ColorEdit3("Outlines", outline_rgb);
			imguiing|=ImGui::GetIO().WantCaptureMouse;
		}
		ImGui::End();

		ImGui::Begin("Physics Options");
		{
			const int num_min=3;
			const int num_max=32;
			static int num=5;
			
			ImGui::Checkbox("Push Points Apart", &push_pts_apart);
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("Point Radius", &point_rad, 5, 15);
			ImGui::SetNextItemWidth(100);
			if(ImGui::SliderInt("Make Hoberman Linkage", &num, num_min, num_max)) {
				//asymptotically decrease len w/ increasing num 
				//starting from minimum num of 3
				float min_len=40, max_len=150;
				float t=std::exp(-.1f*(num-num_min));
				float len=min_len+t*(max_len-min_len);
				makeHobermanLinkage(num, len);
			}
			imguiing|=ImGui::GetIO().WantCaptureMouse;
		}
		ImGui::End();

		simgui_render();
	}
#pragma endregion

	bool user_render() override {
		renderIntoTarget();

		//display pass
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={0, 0, 0, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		renderOutlines();

		renderImGui();

		sg_end_pass();

		sg_commit();

		return true;
	}

	void user_destroy() override {
		simgui_shutdown();
		sgl_shutdown();
	}
};