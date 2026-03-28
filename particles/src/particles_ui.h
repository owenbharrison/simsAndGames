#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/sokol_engine.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "shd.glsl.h"

#include "solver.h"

//for uint32_t
#include <cstdint>

//for time
#include <ctime>

#include "imgui/include/imgui_singleheader.h"
#include "sokol/include/sokol_imgui.h"

using cmn::vf2d;

//https://www.rapidtables.com/convert/color/hsv-to-rgb.html
static void hsv2rgb(
	int hue, float saturation, float value,
	float& r, float& g, float& b
) {
	float c=value*saturation;
	float x=c*(1-std::abs(1-std::fmod(hue/60.f, 2)));
	float m=value-c;
	r=0, g=0, b=0;
	switch(hue/60) {
		case 0: r=c, g=x, b=0; break;
		case 1: r=x, g=c, b=0; break;
		case 2: r=0, g=c, b=x; break;
		case 3: r=0, g=x, b=c; break;
		case 4: r=x, g=0, b=c; break;
		case 5: r=c, g=0, b=x; break;
	}
	r+=m, g+=m, b+=m;
}

vf2d getClosePt(const vf2d& p, const vf2d& a, const vf2d& b) {
	vf2d ap=p-a, ab=b-a;
	float t=cmn::clamp(ap.dot(ab)/ab.dot(ab), 0.f, 1.f);
	return a+t*ab;
}

class ParticlesUI : public cmn::SokolEngine {
	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};
	} line_render;

	struct inst_data_t {
		float pos[2];
		float size[2];
		float col[3];
	};
	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};
		sg_buffer ibuf{};

		sg_sampler smp{};
		sg_view tex{};

		const int max_num=20000;
		inst_data_t* instances=nullptr;
	} particle_render;

	vf2d cam;
	float zoom=1;

	vf2d mouse_scr;
	vf2d prev_mouse_scr;
	vf2d mouse_wld;

	const float time_step=1/120.f;
	const int num_sub_steps=6;
	float update_timer=0;

	Solver solver=Solver(particle_render.max_num, {0, 0}, {720, 400});
	const vf2d gravity{0, 100};

	bool imguiing=false;

	bool adding=false, removing=false;
	const float selection_radius=30;

	vf2d* solid_st=nullptr;
	vf2d* empty_st=nullptr;

	int held_ptc=-1;

	bool paused=false;

	bool render_constraints=false;

public:
#pragma region SETUP_HELPERS
	void setupEnvironment() {
		sg_desc desc{};
		sg_setup(desc);
	}

	void setupLineRender() {
		//instanced tristrip pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.shader=sg_make_shader(line_shader_desc(sg_query_backend()));
		pip_desc.layout.attrs[ATTR_line_i_t].format=SG_VERTEXFORMAT_FLOAT;
		pip_desc.primitive_type=SG_PRIMITIVETYPE_LINES;
		line_render.pip=sg_make_pipeline(pip_desc);

		//line vertex buffer
		float vertexes[2]{0, 1};
		sg_buffer_desc vbuf_desc{};
		vbuf_desc.data=SG_RANGE(vertexes);
		line_render.vbuf=sg_make_buffer(vbuf_desc);
	}

	void setupParticleRender() {
		//instanced tristrip pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.shader=sg_make_shader(quad_shader_desc(sg_query_backend()));
		pip_desc.layout.buffers[1].step_func=SG_VERTEXSTEP_PER_INSTANCE;
		pip_desc.layout.attrs[ATTR_quad_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_quad_i_uv].buffer_index=0;
		pip_desc.layout.attrs[ATTR_quad_inst_pos].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_quad_inst_pos].buffer_index=1;
		pip_desc.layout.attrs[ATTR_quad_inst_size].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_quad_inst_size].buffer_index=1;
		pip_desc.layout.attrs[ATTR_quad_inst_col].format=SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_quad_inst_col].buffer_index=1;
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		//with alpha blending
		pip_desc.colors[0].blend.enabled=true;
		pip_desc.colors[0].blend.src_factor_rgb=SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha=SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		particle_render.pip=sg_make_pipeline(pip_desc);

		//quad vertex buffer
		float vertexes[4][2]{{0, 0}, {1, 0}, {0, 1}, {1, 1}};
		sg_buffer_desc vbuf_desc{};
		vbuf_desc.data=SG_RANGE(vertexes);
		particle_render.vbuf=sg_make_buffer(vbuf_desc);

		//instance data
		particle_render.instances=new inst_data_t[particle_render.max_num];

		//instance buffer
		sg_buffer_desc ibuf_desc{};
		ibuf_desc.size=sizeof(inst_data_t)*particle_render.max_num;
		ibuf_desc.usage.stream_update=true;
		particle_render.ibuf=sg_make_buffer(ibuf_desc);

		sg_sampler_desc sampler_desc{};
		particle_render.smp=sg_make_sampler(sampler_desc);

		//circle texture
		const int sz=1024;
		std::uint32_t* pixels=new std::uint32_t[sz*sz];
		for(int i=0; i<sz; i++) {
			for(int j=0; j<sz; j++) {
				int dx=i-sz/2, dy=j-sz/2;
				bool b=dx*dx+dy*dy<sz*sz/4;
				pixels[i+j*sz]=b?0xFFFFFFFF:0x00000000;
			}
		}
		sg_image_desc image_desc{};
		image_desc.width=sz;
		image_desc.height=sz;
		image_desc.data.mip_levels[0].ptr=pixels;
		image_desc.data.mip_levels[0].size=sizeof(std::uint32_t)*sz*sz;
		sg_image image=sg_make_image(image_desc);
		delete[] pixels;
		sg_view_desc view_desc{};
		view_desc.texture.image=image;
		particle_render.tex=sg_make_view(view_desc);
	}

	void setupImGui() {
		simgui_desc_t simgui_desc{};
		simgui_desc.ini_filename="assets/imgui.ini";
		simgui_setup(simgui_desc);
	}
#pragma endregion

	void userCreate() override {
		setupEnvironment();

		setupLineRender();

		setupParticleRender();

		setupImGui();

		zoomToFit();
	}

	void userDestroy() override {
		simgui_shutdown();
		sg_shutdown();
	}

	void userInput(const sapp_event* e) override {
		simgui_handle_event(e);
	}

#pragma region UPDATE HELPERS
	vf2d wld2scr(const vf2d& w) const {
		vf2d scr_sz(sapp_widthf(), sapp_heightf());
		return scr_sz/2+zoom*(w-cam);
	}

	vf2d scr2wld(const vf2d& s) const {
		vf2d scr_sz(sapp_widthf(), sapp_heightf());
		return cam+(s-scr_sz/2)/zoom;
	}

	void zoomToFit() {
		vf2d scr_sz(sapp_widthf(), sapp_heightf());

		//scale to fit
		int margin=10;
		vf2d sz=solver.max-solver.min;
		vf2d scl=(scr_sz-2*margin)/sz;
		zoom=scl.x<scl.y?scl.x:scl.y;

		//shift world center to scr center
		cam+=(solver.min+solver.max)/2-scr2wld(scr_sz/2);
	}

	void handleZooming(float dt) {
		//wld mouse before zoom
		vf2d before=scr2wld(mouse_scr);

		//apply zoom
		float factor=1+dt;
		if(getKey(SAPP_KEYCODE_W).held) zoom*=factor;
		if(getKey(SAPP_KEYCODE_Q).held) zoom/=factor;

		//wld mouse after zoom
		vf2d after=scr2wld(mouse_scr);

		//pan so wld mouse stays fixed
		cam+=before-after;
	}

	void handleParticleGrabbing() {
		const auto drag_action=getMouse(SAPP_MOUSEBUTTON_LEFT);
		if(drag_action.pressed) {
			for(int i=0; i<solver.getNumParticles(); i++) {
				const auto& p=solver.particles[i];
				if((p.pos-mouse_wld).mag()<p.getRadius()) {
					held_ptc=i;
					break;
				}
			}
		}
		if(drag_action.released) held_ptc=-1;
	}

	//add particles in circle
	void handleParticleAddition() {
		adding=getKey(SAPP_KEYCODE_A).held;
		if(adding) {
			for(int i=0; i<100; i++) {
				float dist=cmn::randFloat(selection_radius);
				float angle=cmn::randFloat(2*cmn::Pi);
				vf2d offset=cmn::polar<vf2d>(dist, angle);
				float rad=cmn::randFloat(2, 4);
				Particle p(scr2wld(mouse_scr+offset), rad);
				p.r=cmn::randFloat();
				p.g=cmn::randFloat();
				p.b=cmn::randFloat();
				solver.addParticle(p);
			}
		}
	}

	//drag solid rect
	void handleSolidAddition() {
		const auto solid_action=getKey(SAPP_KEYCODE_S);
		if(solid_action.pressed) solid_st=new vf2d(mouse_wld);
		if(solid_action.released) {
			//get bounds
			vf2d min=*solid_st, max=mouse_wld;
			if(min.x>max.x) std::swap(min.x, max.x);
			if(min.y>max.y) std::swap(min.y, max.y);

			//random sizing
			float rad=cmn::randFloat(6, 7);
			float m=cmn::randFloat(.5f, 1);

			//determine spacing
			float sz=2*rad+m;
			int w=1+(max.x-min.x)/sz;
			int h=1+(max.y-min.y)/sz;
			solver.addSolidRect(min, rad, m, w, h);

			//free flag
			delete solid_st;
			solid_st=nullptr;
		}
	}

	//drag empty rect
	void handleEmptyAddition() {
		const auto empty_action=getKey(SAPP_KEYCODE_E);
		if(empty_action.pressed) empty_st=new vf2d(mouse_wld);
		if(empty_action.released) {
			//get bounds
			vf2d min=*empty_st, max=mouse_wld;
			if(min.x>max.x) std::swap(min.x, max.x);
			if(min.y>max.y) std::swap(min.y, max.y);

			//random sizing
			float rad=cmn::randFloat(6, 7);
			float m=cmn::randFloat(.5f, 1);

			//determine spacing
			float sz=2*rad+m;
			int w=1+(max.x-min.x)/sz;
			int h=1+(max.y-min.y)/sz;
			solver.addEmptyRect(min, rad, m, w, h);

			//free flag
			delete empty_st;
			empty_st=nullptr;
		}
	}

	//remove anything touching circle
	void handleRemoval() {
		removing=getKey(SAPP_KEYCODE_X).held;
		if(removing) {
			held_ptc=-1;

			//check if particles touch circle
			for(int i=0; i<solver.getNumParticles(); i++) {
				const auto& p=solver.particles[i];
				float rad_wld=p.getRadius()+selection_radius/zoom;
				if((p.pos-mouse_wld).mag()<rad_wld) {
					solver.removeParticle(i);
					i=-1;
				}
			}

			//check if constraint close pt on inside circle
			for(auto it=solver.constraints.begin(); it!=solver.constraints.end();) {
				const auto& a=wld2scr(solver.particles[it->a].pos);
				const auto& b=wld2scr(solver.particles[it->b].pos);
				vf2d close_pt=getClosePt(mouse_scr, a, b);
				if((close_pt-mouse_scr).mag()<selection_radius) {
					it=solver.constraints.erase(it);
				} else it++;
			}
		}
	}

	void handleUserInput(float dt) {
		if(imguiing) return;

		//panning
		if(getKey(SAPP_KEYCODE_LEFT_SHIFT).held) {
			cam-=(mouse_scr-prev_mouse_scr)/zoom;
		}

		handleZooming(dt);

		if(getKey(SAPP_KEYCODE_Z).pressed) zoomToFit();

		handleParticleGrabbing();

		handleParticleAddition();

		handleSolidAddition();

		handleEmptyAddition();

		handleRemoval();

		//play/pause toggle
		if(getKey(SAPP_KEYCODE_SPACE).pressed) paused^=true;

		//constraint render toggle
		if(getKey(SAPP_KEYCODE_C).pressed) render_constraints^=true;
	}

	void handlePhysics(float dt) {
		//ensure similar update across multiple framerates
		update_timer+=dt;
		while(update_timer>time_step) {
			solver.updateSizing();

			solver.accelerate(gravity);

			solver.integrateParticles(time_step);

			for(int i=0; i<num_sub_steps; i++) {
				if(held_ptc!=-1) {
					solver.particles[held_ptc].pos=mouse_wld;
				}
				solver.updateConsraints();
				solver.solveCollisions();
			}

			update_timer-=time_step;
		}
	}
#pragma endregion

	void userUpdate(float dt) override {
		prev_mouse_scr=mouse_scr;
		mouse_scr.x=mouse_x;
		mouse_scr.y=mouse_y;
		mouse_wld=scr2wld(mouse_scr);

		handleUserInput(dt);

		if(!paused) handlePhysics(dt);
	}

#pragma region RENDER HELPERS
	//screen space
	void drawLine(const vf2d& a, const vf2d& b, const sg_color& col) {
		sg_apply_pipeline(line_render.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=line_render.vbuf;
		sg_apply_bindings(bind);

		p_vs_line_t p_vs_line{};
		p_vs_line.u_a[0]=a.x/sapp_widthf();
		p_vs_line.u_a[1]=a.y/sapp_heightf();
		p_vs_line.u_b[0]=b.x/sapp_widthf();
		p_vs_line.u_b[1]=b.y/sapp_heightf();
		p_vs_line.u_col[0]=col.r;
		p_vs_line.u_col[1]=col.g;
		p_vs_line.u_col[2]=col.b;
		sg_apply_uniforms(UB_p_vs_line, SG_RANGE(p_vs_line));

		sg_draw(0, 2, 1);
	}

	//between two screen space pts
	void drawRect(const vf2d& v0, const vf2d& v3, const sg_color& col) {
		vf2d v1(v3.x, v0.y), v2(v0.x, v3.y);
		drawLine(v0, v1, col);
		drawLine(v0, v2, col);
		drawLine(v1, v3, col);
		drawLine(v2, v3, col);
	}

	//screen space
	void drawCircle(const vf2d& ctr, float rad, const sg_color& col) {
		const int num=32;
		vf2d first, prev;
		for(int i=0; i<num; i++) {
			float angle=2*cmn::Pi*i/num;
			vf2d curr=ctr+cmn::polar<vf2d>(rad, angle);
			if(i==0) first=curr;
			else drawLine(prev, curr, col);
			prev=curr;
		}
		drawLine(prev, first, col);
	}

	void renderParticles() {
		int num_ptc=solver.getNumParticles();
		if(!num_ptc) return;

		//update instance data
		for(int i=0; i<num_ptc; i++) {
			const auto& p=solver.particles[i];
			const auto& r=p.getRadius();
			auto& inst_data=particle_render.instances[i];
			inst_data.pos[0]=p.pos.x-r;
			inst_data.pos[1]=p.pos.y-r;
			inst_data.size[0]=2*r;
			inst_data.size[1]=2*r;
			inst_data.col[0]=p.r;
			inst_data.col[1]=p.g;
			inst_data.col[2]=p.b;
		}
		sg_range range{};
		range.ptr=particle_render.instances;
		range.size=sizeof(inst_data_t)*num_ptc;
		sg_update_buffer(particle_render.ibuf, range);

		sg_apply_pipeline(particle_render.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=particle_render.vbuf;
		bind.vertex_buffers[1]=particle_render.ibuf;
		bind.samplers[SMP_b_quad_smp]=particle_render.smp;
		bind.views[VIEW_b_quad_tex]=particle_render.tex;
		sg_apply_bindings(bind);

		p_vs_quad_t p_vs_quad{};
		p_vs_quad.u_resolution[0]=sapp_widthf();
		p_vs_quad.u_resolution[1]=sapp_heightf();
		p_vs_quad.u_cam[0]=cam.x;
		p_vs_quad.u_cam[1]=cam.y;
		p_vs_quad.u_zoom=zoom;
		sg_apply_uniforms(UB_p_vs_quad, SG_RANGE(p_vs_quad));

		sg_draw(0, 4, num_ptc);
	}

	void renderConstraints() {
		for(const auto& c:solver.constraints) {
			const auto& a=solver.particles[c.a];
			const auto& b=solver.particles[c.b];
			float ir=1-(a.r+b.r)/2;
			float ig=1-(a.g+b.g)/2;
			float ib=1-(a.b+b.b)/2;
			drawLine(wld2scr(a.pos), wld2scr(b.pos), {ir, ig, ib, 1});
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

		ImGui::Begin("Scene");
		imguiing|=ImGui::IsWindowHovered();
		ImGui::Text("%d Particles", solver.getNumParticles());
		ImGui::SameLine();
		if(ImGui::Button("Clear")) solver.clear();
		ImGui::SeparatorText("Keybinds");
		ImGui::Text("Hold A to add particles");
		ImGui::Text("Hold X to remove items");
		ImGui::Text("Drag S to add a solid rect");
		ImGui::Text("Drag E to add an empty rect");
		ImGui::Text("Drag LMB to grab particles");
		ImGui::Text("Use SPACE to play/pause");
		ImGui::End();

		ImGui::Begin("Graphics");
		imguiing|=ImGui::IsWindowHovered();
		if(ImGui::Button("Rainbow Recolor")) {
			for(int i=0; i<solver.getNumParticles(); i++) {
				auto& p=solver.particles[i];
				int hue=360*(p.pos.x-solver.min.x)/(solver.max.x-solver.min.x);
				float value=(p.pos.y-solver.min.y)/(solver.max.y-solver.min.y);
				hsv2rgb(hue, 1, value, p.r, p.g, p.b);
			}
		}
		ImGui::SeparatorText("Keybinds");
		ImGui::Text("Use W/Q to zoom in/out");
		ImGui::Text("Hold Shift to pan around");
		ImGui::Text("Use Z to reset zoom & pan");
		ImGui::Text("Use C to show/hide constraints");
		ImGui::End();

		simgui_render();
	}
#pragma endregion

	void userRender() override {
		//display pass
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={0, 0, 0, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		//draw solver bounds
		{
			auto col=paused?sg_color{1, 0, 0, 1}:sg_color{.5f, .5f, .5f, 1};
			drawRect(wld2scr(solver.min), wld2scr(solver.max), col);
		}

		//draw green addition circle
		if(adding) drawCircle(mouse_scr, selection_radius, {0, 1, 0, 1});

		//draw red removal circle
		if(removing) drawCircle(mouse_scr, selection_radius, {1, 0, 0, 1});

		//draw yellow solid rect
		if(solid_st) {
			drawRect(wld2scr(*solid_st), mouse_scr, {1, 1, 0, 1});
		}

		//draw purple empty rect
		if(empty_st) {
			drawRect(wld2scr(*empty_st), mouse_scr, {.565f, 0, 1, 1});
		}

		renderParticles();

		if(render_constraints) renderConstraints();

		renderImGui();

		sg_end_pass();

		sg_commit();
	}

	ParticlesUI() {
		app_title="Particles UI Demo";
	}
};