#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

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

#include "sokol/texture_utils.h"

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
	sg_sampler sampler{};

	sgl_pipeline sgl_pip{};

	struct {
		sg_image color_img{SG_INVALID_ID};
		sg_view color_attach{SG_INVALID_ID};
		sg_view color_tex{SG_INVALID_ID};
		sg_image depth_img{SG_INVALID_ID};
		sg_view depth_attach{SG_INVALID_ID};
	} render_target;

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};
	} outline_render;

	//scene stuff
	std::list<vf2d> points;
	vf2d* held_pt=nullptr;
	float point_rad=7.5f;
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

	int hoberman_num=0;

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
	void setupEnvironment() {
		sg_desc desc{};
		sg_setup(desc);
	}

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

	void updateRenderTarget() {
		{
			sg_destroy_image(render_target.color_img);
			sg_image_desc image_desc{};
			image_desc.usage.color_attachment=true;
			image_desc.width=sapp_width();
			image_desc.height=sapp_height();
			render_target.color_img=sg_make_image(image_desc);

			//make attach
			{
				sg_destroy_view(render_target.color_attach);
				sg_view_desc view_desc{};
				view_desc.color_attachment.image=render_target.color_img;
				render_target.color_attach=sg_make_view(view_desc);
			}

			//make tex
			{
				sg_destroy_view(render_target.color_tex);
				sg_view_desc view_desc{};
				view_desc.texture.image=render_target.color_img;
				render_target.color_tex=sg_make_view(view_desc);
			}
		}

		{
			sg_destroy_image(render_target.depth_img);
			sg_image_desc image_desc{};
			image_desc.usage.depth_stencil_attachment=true;
			image_desc.width=sapp_width();
			image_desc.height=sapp_height();
			image_desc.pixel_format=SG_PIXELFORMAT_DEPTH_STENCIL;//??
			render_target.depth_img=sg_make_image(image_desc);

			//make attach
			sg_destroy_view(render_target.depth_attach);
			sg_view_desc view_desc{};
			view_desc.depth_stencil_attachment.image=render_target.depth_img;
			render_target.depth_attach=sg_make_view(view_desc);
		}
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
		points.clear();
		vf2d** grid=new vf2d*[3*num];
		for(int i=0; i<num; i++) {
			float base_angle=i*inc_angle;
			vf2d pos=ctr;
			for(int j=0; j<3; j++) {
				float angle=base_angle+j*inc_angle;
				pos+=cmn::polar<vf2d>(len, angle);
				points.push_back(pos);
				grid[ix(i, j)]=&points.back();
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

	void setupImGui() {
		simgui_desc_t simgui_desc{};
		simgui_desc.ini_filename="assets/imgui.ini";
		simgui_setup(simgui_desc);
	}

	void setupIcon() {
		int width, height, comp;
		std::uint8_t* pixels8=stbi_load("assets/icon.png", &width, &height, &comp, 4);
		if(!pixels8) return;

		std::uint32_t* pixels32=new std::uint32_t[width*height];
		std::memcpy(pixels32, pixels8, sizeof(std::uint8_t)*4*width*height);
		stbi_image_free(pixels8);

		sapp_icon_desc icon_desc{};
		icon_desc.images[0].width=width;
		icon_desc.images[0].height=height;
		icon_desc.images[0].pixels.ptr=pixels32;
		icon_desc.images[0].pixels.size=sizeof(std::uint32_t)*width*height;
		sapp_set_icon(&icon_desc);
		delete[] pixels32;
	}
#pragma endregion

	void userCreate() override {
		std::srand(std::time(0));

		setupEnvironment();

		setupSampler();

		setupSGL();

		updateRenderTarget();

		setupOutlineRender();

		setupScene();

		setupImGui();

		setupIcon();
	}

#pragma region UPDATE HELPERS
	void handlePointMovement() {
		if(imguiing) return;

		const vf2d mouse_pos(mouse_x, mouse_y);

		const auto action=getMouse(SAPP_MOUSEBUTTON_LEFT);
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

	void userUpdate(float dt) override {
		for(int i=0; i<25; i++) {
			handlePointMovement();

			if(push_pts_apart) pushPointsApart();
			updateDistConstraints();
			updateAngleConstraints();
		}
	}

	void userInput(const sapp_event* e) override {
		switch(e->type) {
			case SAPP_EVENTTYPE_RESIZED:
				updateRenderTarget();
				break;
		}

		simgui_handle_event(e);
	}

#pragma region RENDERERS
	void renderGrid(const sg_color& col) {
		const float res=25;
		const float w=3;
		const int ratio=5;

		//vertical lines
		int num_x=1+sapp_widthf()/res;
		for(int i=0; i<num_x; i++) {
			float x=res*i;
			vf2d top(x, 0), btm(x, sapp_heightf());
			if(i%ratio==0) cmn::draw_thick_line(top.x, top.y, btm.x, btm.y, w, col);
			else cmn::draw_line(top.x, top.y, btm.x, btm.y, col);
		}

		//horizontal lines
		int num_y=1+sapp_heightf()/res;
		for(int j=0; j<num_y; j++) {
			float y=res*j;
			vf2d lft(0, y), rgt(sapp_widthf(), y);
			if(j%ratio==0) cmn::draw_thick_line(lft.x, lft.y, rgt.x, rgt.y, w, col);
			else cmn::draw_line(lft.x, lft.y, rgt.x, rgt.y, col);
		}
	}
	
	void renderIntoTarget() {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={0, 0, 0, 0};
		pass.attachments.colors[0]=render_target.color_attach;
		pass.attachments.depth_stencil=render_target.depth_attach;
		sg_begin_pass(pass);

		sgl_defaults();

		//pixel space
		sgl_matrix_mode_projection();
		sgl_load_identity();
		sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);
		sgl_matrix_mode_modelview();
		sgl_load_identity();

		sgl_load_pipeline(sgl_pip);

		//alpha=0 = background(no outline)
		cmn::fill_rect(0, 0, sapp_widthf(), sapp_heightf(),
			{bkgd_rgb[0], bkgd_rgb[1], bkgd_rgb[2], 0}
		);

		if(render_grid) renderGrid({grid_rgb[0], grid_rgb[1], grid_rgb[2], 0});

		//alpha=1 = foreground(outlines)

		for(const auto& d:dist_constraints) {
			cmn::draw_thick_line(d.a->x, d.a->y, d.b->x, d.b->y, 2*point_rad, d.col);
		}

		//render points
		{
			const sg_color point_col{point_rgb[0], point_rgb[1], point_rgb[2], 1};
			for(const auto& p:points) {
				cmn::fill_circle(p.x,p.y,point_rad,point_col);
			}
		}

		sgl_draw();

		sg_end_pass();
	}

	void renderOutlines() {
		sg_apply_pipeline(outline_render.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=outline_render.vbuf;
		bind.samplers[SMP_b_outline_smp]=sampler;
		bind.views[VIEW_b_outline_tex]=render_target.color_tex;
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
		imguiing|=ImGui::IsWindowHovered();
		ImGui::Checkbox("Render Grid", &render_grid);
		ImGui::SliderFloat("Outline Radius", &outline_rad, 0, 6);
		ImGui::ColorEdit3("Background", bkgd_rgb);
		ImGui::ColorEdit3("Grid Lines", grid_rgb);
		ImGui::ColorEdit3("Points", point_rgb);
		ImGui::ColorEdit3("Outlines", outline_rgb);
		ImGui::End();

		ImGui::Begin("Physics Options");
		imguiing|=ImGui::IsWindowHovered();
		ImGui::Checkbox("Push Points Apart", &push_pts_apart);
		ImGui::SliderFloat("Point Radius", &point_rad, 5, 15);
		if(ImGui::SliderInt("Make Hoberman Linkage", &hoberman_num, 3, 48)) {
			makeHobermanLinkage(hoberman_num);
		}
		ImGui::End();

		simgui_render();
	}
#pragma endregion

	void userRender() override {
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
	}

	void userDestroy() override {
		simgui_shutdown();
		sgl_shutdown();
		sg_shutdown();
	}

	SketcherUI() {
		app_title="Sketcher UI Demo";
	}
};