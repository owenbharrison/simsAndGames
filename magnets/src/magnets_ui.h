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

#include "sokol/render_utils.h"

//for time
#include <ctime>

#include "magnet.h"

#include <list>

#include "camera.h"

using cmn::vd2d;
using cmn::vf2d;

class MagnetsUI : public cmn::SokolEngine {
	//physics
	std::list<Magnet> magnets;
	const double meter=1;
	const double decimeter=1e-2;
	const double centimeter=1e-2;
	const double millimeter=1e-3;

	bool update_phys=false;
	const double time_step=1/360.;
	double update_timer=0;

	vd2d bounds_min{-.5, -.5};
	vd2d bounds_max{.5, .5};

	//user input
	vd2d mouse_scr;
	vd2d mouse_wld;
	Camera cam;

	//graphics
	sgl_pipeline pip{};

public:
	void setupMagnets() {
		double rad=.3*centimeter;
		double spacing=5*centimeter;
		magnets.push_back(Magnet(spacing*vd2d(-.5, -.5), rad));
		magnets.push_back(Magnet(spacing*vd2d(.5, -.5), rad));
		magnets.push_back(Magnet(spacing*vd2d(-.5, .5), rad));
		magnets.push_back(Magnet(spacing*vd2d(.5, .5), rad));
	}

	void setupPipeline() {
		//alpha blending
		sg_pipeline_desc pip_desc{};
		pip_desc.colors[0].blend.enabled=true;
		pip_desc.colors[0].blend.src_factor_rgb=SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha=SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip=sgl_make_pipeline(pip_desc);
	}

	bool user_create() override {
		app_title="Magnets";

		std::srand(std::time(0));

		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);

		setupMagnets();

		cam=Camera({sapp_widthf(), sapp_heightf()});

		zoomToFit();

		setupPipeline();

		return true;
	}

	void user_input(const sapp_event* e) override {
		switch(e->type) {
			case SAPP_EVENTTYPE_RESIZED:
				cam.scr_sz.x=sapp_widthf();
				cam.scr_sz.y=sapp_heightf();
				break;
		}
	}

#pragma region UPDATE_HELPERS
	void zoomToFit() {
		if(magnets.empty()) return;

		//magnets bounding box
		vd2d min{1e30, 1e30};
		vd2d max{-1e30, -1e30};
		for(const auto& m:magnets) {
			auto r=m.getRad();
			if(m.pos.x-r<min.x) min.x=m.pos.x-r;
			if(m.pos.y-r<min.y) min.y=m.pos.y-r;
			if(m.pos.x+r>max.x) max.x=m.pos.x+r;
			if(m.pos.y+r>max.y) max.y=m.pos.y+r;
		}

		cam.zoomToFit(min, max, 30);
	}

	bool isOverlapping(const Magnet& c) const {
		auto rad=c.getRad();
		if(c.pos.x-rad<bounds_min.x) return true;
		if(c.pos.y-rad<bounds_min.y) return true;
		if(c.pos.x+rad>bounds_max.x) return true;
		if(c.pos.y+rad>bounds_max.y) return true;

		for(const auto& m:magnets) {
			double rad_t=rad+m.getRad();
			if((m.pos-c.pos).mag_sq()<rad_t*rad_t) return true;
		}
		return false;
	}

	void handleAdditionAction() {
		if(!GetKey(SAPP_KEYCODE_A).held) return;

		//candidate
		vd2d pos=mouse_wld;
		double rad=centimeter*cmn::randDouble(.3, .35);

		//inside bounds?
		if(pos.x-rad<bounds_min.x) return;
		if(pos.y-rad<bounds_min.y) return;
		if(pos.x+rad>bounds_max.x) return;
		if(pos.y+rad>bounds_max.y) return;

		//overlapping others?
		for(const auto& m:magnets) {
			double rad_t=rad+m.getRad();
			if((m.pos-pos).mag_sq()<rad_t*rad_t) return;
		}

		//valid
		double rot=cmn::randDouble(2*Pi);
		magnets.push_back(Magnet(pos, rad, rot));
	}

	void handleRemovalAction() {
		if(!GetKey(SAPP_KEYCODE_X).held) return;

		for(auto it=magnets.begin(); it!=magnets.end(); ) {
			if((it->pos-mouse_wld).mag()<it->getRad()) {
				it=magnets.erase(it);
			} else it++;
		}
	}

	void handleUserInput(double dt) {
		handleAdditionAction();

		handleRemovalAction();

		if(GetKey(SAPP_KEYCODE_LEFT_SHIFT).pressed) cam.begin_pan(mouse_scr);
		if(GetKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam.update_pan(mouse_scr);

		if(GetKey(SAPP_KEYCODE_W).held) cam.update_zoom(mouse_scr, 1+dt);
		if(GetKey(SAPP_KEYCODE_Q).held) cam.update_zoom(mouse_scr, 1-dt);

		if(GetKey(SAPP_KEYCODE_Z).held) zoomToFit();

		if(GetKey(SAPP_KEYCODE_SPACE).pressed) update_phys^=true;
	}

	void handlePhysics(double dt) {
		//ensure similar update across multiple framerates?
		update_timer+=dt;
		while(update_timer>time_step) {
			//for each unique pair of magnets
			for(auto ait=magnets.begin(); ait!=magnets.end(); ait++) {
				for(auto bit=std::next(ait); bit!=magnets.end(); bit++) {
					auto& a=*ait;
					auto& b=*bit;

					Magnet::applyMonopoleForces(a, b);
				}
			}

			//integrate magnets
			for(auto& m:magnets) {
				m.update(time_step);

				m.keepInside(bounds_min, bounds_max);
			}

			//check for collisions
			for(auto ait=magnets.begin(); ait!=magnets.end(); ait++) {
				for(auto bit=std::next(ait); bit!=magnets.end(); bit++) {
					auto& a=*ait;
					auto& b=*bit;

					Magnet::checkCollide(a, b);
				}
			}

			update_timer-=time_step;
		}
	}
#pragma endregion

	bool user_update(float dt) override {
		//update mice
		mouse_scr.x=GetMouseX();
		mouse_scr.y=GetMouseY();
		mouse_wld=cam.scr2wld(mouse_scr);

		handleUserInput(dt);

		if(update_phys) handlePhysics(dt);

		return true;
	}

#pragma region RENDER_HELPERS
	void fill_arc(
		float x, float y, float rad,
		float st, float len,
		float r, float g, float b
	) {
		sgl_begin_triangles();

		sgl_c3f(r, g, b);

		const int num=32;
		vf2d ctr(x, y), prev;
		for(int i=0; i<=num; i++) {
			float angle=st+len*i/num;
			vf2d curr=ctr+cmn::polar<vf2d>(rad, angle);
			if(i!=0) {
				sgl_v2f(x, y);
				sgl_v2f(prev.x, prev.y);
				sgl_v2f(curr.x, curr.y);
			}
			prev=curr;
		}

		sgl_end();
	}

	void renderGrid(float r, float g, float b) {
		//screen bounds in world space
		vf2d min=cam.scr2wld({0, 0});
		vf2d max=cam.scr2wld(cam.scr_sz);

		//how much "world fits horiz on scr
		float z=max.x-min.x;
		float l=std::log10(z);
		float e=std::floor(l)-1;
		float size=std::pow(10, e);
		float fade=l-std::floor(l);

		//snap to containment
		int si=std::floor(min.x/size);
		int sj=std::floor(min.y/size);
		int ei=std::ceil(max.x/size);
		int ej=std::ceil(max.y/size);

		//vertical lines
		for(int i=si; i<=ei; i++) {
			float x_wld=size*i;
			float x_scr=cam.wld2scr({x_wld, 0}).x;
			if(i%5==0) cmn::draw_thick_line(
				x_scr, 0, x_scr, sapp_heightf(),
				2,
				r, g, b
			);
			else cmn::draw_line(
				x_scr, 0, x_scr, sapp_heightf(),
				r, g, b, 1-fade
			);
		}

		//horizontal lines
		for(int j=sj; j<=ej; j++) {
			float y_wld=size*j;
			float y_scr=cam.wld2scr({0, y_wld}).y;
			if(j%5==0) cmn::draw_thick_line(
				0, y_scr, sapp_widthf(), y_scr,
				2,
				r, g, b
			);
			else cmn::draw_line(
				0, y_scr, sapp_widthf(), y_scr,
				r, g, b, 1-fade
			);
		}
	}

	void renderBounds(float r, float g, float b) {
		vf2d min_scr=cam.wld2scr(bounds_min);
		vf2d max_scr=cam.wld2scr(bounds_max);
		float x=min_scr.x;
		float y=min_scr.y;
		float w=max_scr.x-x;
		float h=max_scr.y-y;
		cmn::draw_thick_rect(
			x, y, w, h,
			2,
			r, g, b
		);
	}

	//grey circle w/ red N arc & blue S arc
	void renderMagnets(float r, float g, float b) {
		for(const auto& m:magnets) {
			vf2d pos_scr=cam.wld2scr(m.pos);
			float rad_scr=cam.wld2scr(m.getRad());

			float st=m.rot-Pi/2;
			fill_arc(pos_scr.x, pos_scr.y, rad_scr, st, Pi, 1, 0, 0);
			fill_arc(pos_scr.x, pos_scr.y, rad_scr, st, -Pi, 0, 0, 1);

			cmn::fill_torus(
				pos_scr.x, pos_scr.y,
				rad_scr-1, rad_scr+1,
				r, g, b
			);
		}
	}
#pragma endregion

	bool user_render() override {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={0, 0, 0, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_projection();
		sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);

		//light blue
		cmn::fill_rect(
			0, 0, sapp_widthf(), sapp_heightf(),
			112/255.f, 207/255.f, 254/255.f
		);

		//dark grey
		renderGrid(38/255.f, 38/255.f, 38/255.f);

		//red
		renderBounds(1, 0, 0);

		renderMagnets(1, 1, 1);
		
		//blue
		if(!update_phys) cmn::draw_thick_rect(
			0, 0, sapp_widthf(), sapp_heightf(),
			5,
			0, 0, 1
		);

		sgl_draw();

		sg_end_pass();

		sg_commit();

		return true;
	}
};