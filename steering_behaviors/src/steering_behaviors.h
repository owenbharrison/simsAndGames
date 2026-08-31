#define SOKOL_IMPL
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "vendor/sokol/sokol_app.h"
#include "vendor/sokol/sokol_gfx.h"
#include "vendor/sokol/sokol_glue.h"

#include "common/sokol/sokol_engine.h"

#include "vendor/sokol/sokol_gl.h"

#include "vehicle.h"

#include "camera.h"

#include "common/utils.h"

//for time
#include <ctime>

#include "common/geom/aabb2.h"

#include "font.h"

using cmn::vf2d;
using cmn::AABBf2;

class SteeringBehaviors : public cmn::SokolEngine {
	sgl_pipeline pip{};

	sg_view tex{};

	sg_sampler smp{};

	std::vector<Vehicle> vehicles;

	//user input
	vf2d mouse_scr;
	vf2d mouse_wld;
	Camera cam;

public:
	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_desc.max_vertices=500000;
		sgl_setup(sgl_desc);
	}

	void setupVehicles() {
		std::string str="Hello, World!\n\t[abc123]\ntesting...";
		auto pts=Font::stringToDots({0, 0}, str, 1);
		for(const auto& p:pts) {
			Vehicle v;
			v.pos=p;
			v.target=p;
			v.rgb[0]=cmn::randFloat();
			v.rgb[1]=cmn::randFloat();
			v.rgb[2]=cmn::randFloat();
			vehicles.push_back(v);
		}
	}

	void setupPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.colors[0].blend.enabled=true;
		pip_desc.colors[0].blend.src_factor_rgb=SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha=SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip=sgl_make_pipeline(pip_desc);
	}

	//make circle texture:
	//in=white, out=blank
	void setupTexture() {
		const int sz=1024;
		auto pixels=new std::uint32_t[sz*sz];
		for(int i=0; i<sz; i++) {
			for(int j=0; j<sz; j++) {
				int dx=i-sz/2, dy=j-sz/2;
				bool in=dx*dx+dy*dy<sz*sz/4;
				pixels[i+sz*j]=in?0xffffffff:0x00000000;
			}
		}

		sg_image_desc img_desc{};
		img_desc.width=sz;
		img_desc.height=sz;
		img_desc.data.mip_levels[0].ptr=pixels;
		img_desc.data.mip_levels[0].size=sizeof(std::uint32_t)*sz*sz;
		sg_image img=sg_make_image(img_desc);

		delete[] pixels;

		sg_view_desc view_desc{};
		view_desc.texture.image=img;
		tex=sg_make_view(view_desc);
	}

	void setupSampler() {
		sg_sampler_desc smp_desc{};
		smp=sg_make_sampler(smp_desc);
	}

	bool user_create() override {
		app_title="Steering Behaviors";

		setupSGL();

		std::srand(std::time(0));

		if(!Font::setup()) return false;

		setupVehicles();

		zoomToFit();

		setupPipeline();

		setupTexture();

		setupSampler();

		return true;
	}

	void zoomToFit() {
		if(vehicles.empty()) return;

		//vehicles bounding box
		AABBf2 box;
		const cmn::vf2d inf(1e300, 1e300);
		box.min=inf, box.max=-inf;
		for(const auto& v:vehicles) {
			box.fitToEnclose(cam.wld2scr_p(v.pos));
		}

		const float margin=30;
		vf2d scr_min{margin, margin};
		vf2d scr_max{sapp_widthf()-margin, sapp_heightf()-margin};
		cam.zoomToFitScreen(
			box.min, box.max,
			scr_min, scr_max
		);
	}

	void randomizePositions() {
		//vehicles bounding box
		AABBf2 box;
		const cmn::vf2d inf(1e300, 1e300);
		box.min=inf, box.max=-inf;
		for(const auto& v:vehicles) {
			box.fitToEnclose(v.pos);
		}

		for(auto& v:vehicles) {
			vf2d pos01(cmn::randFloat(), cmn::randFloat());
			v.pos=box.min+pos01*(box.max-box.min);
		}
	}

	void randomizeVelocities(float n, float x) {
		for(auto& v:vehicles) {
			float speed=.01f*cmn::randFloat(n, x);
			float angle=cmn::randFloat(2*cmn::Pi);
			v.vel=vf2d::polar({speed, angle});
		}
	}

	bool user_update(float dt) override {
		//update mice
		vf2d mouse_scr_prev=mouse_scr;
		mouse_scr.x=GetMouseX();
		mouse_scr.y=GetMouseY();
		vf2d mouse_scr_delta=mouse_scr-mouse_scr_prev;
		mouse_wld=cam.scr2wld_p(mouse_scr);

		//panning
		const auto lshift=GetKey(SAPP_KEYCODE_LEFT_SHIFT);
		if(lshift.pressed) sapp_set_mouse_cursor(SAPP_MOUSECURSOR_RESIZE_ALL);
		if(lshift.held) cam.updatePan(mouse_scr_delta);
		if(lshift.released) sapp_set_mouse_cursor(SAPP_MOUSECURSOR_DEFAULT);
		
		//zooming
		const auto lctrl=GetKey(SAPP_KEYCODE_LEFT_CONTROL);
		if(lctrl.pressed) {
			cam.beginZoom(mouse_scr);
			sapp_set_mouse_cursor(SAPP_MOUSECURSOR_RESIZE_NS);
		}
		if(lctrl.held) cam.updateZoom(1-mouse_scr_delta.y*dt);
		if(lctrl.released) sapp_set_mouse_cursor(SAPP_MOUSECURSOR_DEFAULT);
		
		//rotating
		const auto lalt=GetKey(SAPP_KEYCODE_LEFT_ALT);
		if(lalt.pressed) {
			cam.beginRotate(mouse_scr);
			sapp_set_mouse_cursor(SAPP_MOUSECURSOR_RESIZE_EW);
		}
		if(lalt.held) cam.updateRotate(mouse_scr_delta.x*dt);
		if(lalt.released) sapp_set_mouse_cursor(SAPP_MOUSECURSOR_DEFAULT);

		//home?
		if(GetKey(SAPP_KEYCODE_Z).pressed) zoomToFit();

		//debug
		if(GetKey(SAPP_KEYCODE_V).pressed) randomizeVelocities(5, 20);
		if(GetKey(SAPP_KEYCODE_P).pressed) randomizePositions();

		//physics
		const auto lmouse=GetMouse(SAPP_MOUSEBUTTON_LEFT);
		for(auto& v:vehicles) {
			const float w_arrive=1;
			const float w_flee=2;

			v.accelerate(w_arrive*v.getArrive(v.target));
			if(lmouse.held&&length(mouse_wld-v.pos)<.2f) {
				v.accelerate(w_flee*v.getFlee(mouse_wld));
			}

			v.update(dt);
		}

		return true;
	}

	//this is just so awesome :D
	void renderGrid() {
		const vf2d res(sapp_widthf(), sapp_heightf());

		//minimum wld box to contain screen
		const vf2d inf(1e300, 1e300);
		AABBf2 box{inf, -inf};
		box.fitToEnclose(cam.scr2wld_p(vf2d(0, 0)*res));
		box.fitToEnclose(cam.scr2wld_p(vf2d(1, 0)*res));
		box.fitToEnclose(cam.scr2wld_p(vf2d(1, 1)*res));
		box.fitToEnclose(cam.scr2wld_p(vf2d(0, 1)*res));

		//world space region
		int si=std::floor(box.min.x);
		int sj=std::floor(box.min.y);
		int ei=std::ceil(box.max.x);
		int ej=std::ceil(box.max.y);

		sgl_begin_lines();
		//"vertical"
		for(int i=si; i<=ei; i++) {
			if(i==0) sgl_c3f(1, 1, 1);
			else if(i%5==0) sgl_c3f(0, 0, 1);
			else sgl_c3f(.3f, .3f, .3f);
			vf2d t=cam.wld2scr_p(vf2d(i, sj));
			vf2d b=cam.wld2scr_p(vf2d(i, ej));
			sgl_v2f(t.x, t.y), sgl_v2f(b.x, b.y);
		}

		//"horizontal"
		for(int j=sj; j<=ej; j++) {
			if(j==0) sgl_c3f(1, 1, 1);
			else if(j%5==0) sgl_c3f(1, 0, 0);
			else sgl_c3f(.3f, .3f, .3f);
			vf2d l=cam.wld2scr_p(vf2d(si, j));
			vf2d r=cam.wld2scr_p(vf2d(ei, j));
			sgl_v2f(l.x, l.y), sgl_v2f(r.x, r.y);
		}
		sgl_end();
	}

	void renderVehicles(float r_wld) {
		const float r_scr=cam.wld2scr_d(r_wld);
		sgl_enable_texture();
		sgl_texture(tex, smp);
		sgl_begin_quads();
		for(const auto& v:vehicles) {
			const auto& pos=cam.wld2scr_p(v.pos);
			sgl_c3f(v.rgb[0], v.rgb[1], v.rgb[2]);
			sgl_v2f_t2f(pos.x-r_scr, pos.y-r_scr, 0, 0);
			sgl_v2f_t2f(pos.x+r_scr, pos.y-r_scr, 1, 0);
			sgl_v2f_t2f(pos.x+r_scr, pos.y+r_scr, 1, 1);
			sgl_v2f_t2f(pos.x-r_scr, pos.y+r_scr, 0, 1);
		}
		sgl_end();
		sgl_disable_texture();
	}

	bool user_render() override {
		const float w_scr=sapp_widthf();
		const float h_scr=sapp_heightf();

		sg_pass pass{};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_projection();
		sgl_ortho(0, w_scr, h_scr, 0, -1, 1);

		sgl_begin_quads();
		sgl_c3f(0, 0, 0);
		sgl_v2f(0, 0);
		sgl_v2f(w_scr, 0);
		sgl_v2f(w_scr, h_scr);
		sgl_v2f(0, h_scr);
		sgl_end();

		renderGrid();

		//1.5cm?
		renderVehicles(.01f*1.5f);

		sgl_draw();

		sg_end_pass();

		sg_commit();

		return true;
	}

	void user_destroy() override {
		sgl_shutdown();
	}
};