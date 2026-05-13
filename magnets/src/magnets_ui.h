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

#include "cmn/utils.h"

#include <list>

//welcome to precise-town!!
using cmn::vd2d;

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
	vd2d wld_offset;
	double zoom=1;

	vd2d mouse_scr;
	vd2d prev_mouse_scr;
	vd2d mouse_wld;
	vd2d prev_mouse_wld;

public:
	void setupMagnets() {
		double rad=.3*centimeter;
		double spacing=5*centimeter;
		magnets.push_back(Magnet(spacing*vd2d(-.5, -.5), rad));
		magnets.push_back(Magnet(spacing*vd2d(.5, -.5), rad));
		magnets.push_back(Magnet(spacing*vd2d(-.5, .5), rad));
		magnets.push_back(Magnet(spacing*vd2d(.5, .5), rad));
	}

	bool user_create() override {
		app_title="Magnets";

		std::srand(std::time(0));

		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);

		setupMagnets();

		zoomToFit();

		return true;
	}
#pragma region UPDATE_HELPERS
	vd2d wld2scr(const vd2d& w) const {
		vd2d scr_sz(sapp_widthf(), sapp_heightf());
		return scr_sz/2+zoom*(w-wld_offset);
	}

	vd2d scr2wld(const vd2d& s) const {
		vd2d scr_sz(sapp_widthf(), sapp_heightf());
		return wld_offset+(s-scr_sz/2)/zoom;
	}

	double wld2scr(double w) const {
		return zoom*w;
	}

	double scr2wld(double s) const {
		return s/zoom;
	}

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

		//in screen space
		double w_box=wld2scr(max.x-min.x);
		double h_box=wld2scr(max.y-min.y);

		//actual screen box
		const double margin=20;
		double w_scr=sapp_widthf()-2*margin;
		double h_scr=sapp_heightf()-2*margin;

		//find aspect ratio
		double asp_x=w_scr/w_box;
		double asp_y=h_scr/h_box;

		//combine it with current scale
		//based on limiting dimension
		zoom*=asp_x<asp_y?asp_x:asp_y;

		//world space box center
		vd2d box_ctr=(min+max)/2;
		//world space screen center
		vd2d scr_sz(sapp_widthf(), sapp_heightf());
		vd2d scr_ctr=scr2wld(scr_sz/2);

		//make them overlap
		vd2d delta=scr_ctr-box_ctr;
		wld_offset-=delta;
	}

	void handlePanning() {
		if(!GetKey(SAPP_KEYCODE_LEFT_SHIFT).held) return;

		//dont use stored, as this makes them change!
		vd2d delta=scr2wld(mouse_scr)-scr2wld(prev_mouse_scr);
		wld_offset-=delta;
	}

	void handleZooming(double dt) {
		//wld mouse before zoom
		vd2d before=scr2wld(mouse_scr);

		//apply zoom
		if(GetKey(SAPP_KEYCODE_W).held) zoom*=1+dt;
		if(GetKey(SAPP_KEYCODE_Q).held) zoom*=1-dt;

		//wld mouse after zoom
		vd2d after=scr2wld(mouse_scr);

		//pan back so wld mouse stays fixed
		vd2d delta=after-before;
		wld_offset-=delta;
	}

	bool isOverlapping(const Magnet& c) const {
		for(const auto& m:magnets) {
			double rad_t=m.getRad()+c.getRad();
			if((m.pos-c.pos).mag()<rad_t) return true;
		}
		return false;
	}

	void handleAdditionAction() {
		if(!GetKey(SAPP_KEYCODE_A).held) return;

		double rad_cm=cmn::randDouble(.3, .5);
		Magnet cand(mouse_wld, centimeter*rad_cm);
		if(!isOverlapping(cand)) magnets.push_back(cand);
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
		//update screen mouse
		prev_mouse_scr=mouse_scr;
		mouse_scr.x=GetMouseX();
		mouse_scr.y=GetMouseY(); 

		handlePanning();

		handleZooming(dt);

		if(GetKey(SAPP_KEYCODE_H).held) zoomToFit();

		//update world mouse
		prev_mouse_wld=scr2wld(prev_mouse_scr);
		mouse_wld=scr2wld(mouse_scr);

		handleAdditionAction();

		handleRemovalAction();

		if(GetKey(SAPP_KEYCODE_P).pressed) update_phys^=true;
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
		handleUserInput(dt);

		if(update_phys) handlePhysics(dt);
		
		return true;
	}

#pragma region RENDER_HELPERS
	void draw_arc(
		float cx, float cy, float rad,
		float st, float len,
		float r, float g, float b
	) {
		const int num=32;
		sgl_begin_line_strip();
		sgl_c3f(r, g, b);
		for(int i=0; i<num; i++) {
			float angle=st+len*i/(num-1);
			vd2d o=polar(rad, angle);
			sgl_v2f(cx+o.x, cy+o.y);
		}
		sgl_end();
	}

	void renderBounds() {
		vd2d min_scr=wld2scr(bounds_min);
		vd2d max_scr=wld2scr(bounds_max);
		float x=min_scr.x;
		float y=min_scr.y;
		float w=max_scr.x-x;
		float h=max_scr.y-y;
		cmn::draw_rect(x, y, w, h, 1, 0, 0);
	}

	void renderGrid() {
		//askjhdkljh
		//askjhdkljh
		//askjhdkljh
		//askjhdkljh
		//askjhdkljh
		//askjhdkljh
		//askjhdkljh
		//askjhdkljh
		//askjhdkljh
		//askjhdkljh
		//askjhdkljh
		//askjhdkljh
	}

	//grey circle w/ red N arc & blue S arc
	void renderMagnets() {
		for(const auto& m:magnets) {
			vd2d pos_scr=wld2scr(m.pos);
			double rad_scr=wld2scr(m.getRad());
			cmn::fill_circle(pos_scr.x, pos_scr.y, rad_scr, .5f, .5f, .5f);

			double st=m.rot-Pi/2;
			draw_arc(pos_scr.x, pos_scr.y, rad_scr, st, Pi, 1, 0, 0);
			draw_arc(pos_scr.x, pos_scr.y, rad_scr, st, -Pi, 0, 0, 1);
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
		sgl_matrix_mode_projection();
		sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);

		renderBounds();

		renderGrid();

		renderMagnets();

		sgl_draw();

		sg_end_pass();

		sg_commit();

		return true;
	}
};