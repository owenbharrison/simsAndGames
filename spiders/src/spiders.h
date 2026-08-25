#define SOKOL_IMPL
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/include/sokol_app.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"
#include "sokol/include/sokol_gl.h"

#include "sokol/sokol_engine.h"

#include "legged_walker.h"
using Spider=LeggedWalker<8>;

cmn::vf2d clampVec(const cmn::vf2d& v, float max) {
	float mag=length(v);
	return v*(mag>max?max/mag:1);
}

using cmn::vf2d;

class Spiders : public cmn::SokolEngine {
	Spider spider=Spider(60, 30, 20);

public:
	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);
	}

	bool user_create() override {
		app_title="Spiders";

		setupSGL();

		return true;
	}

	bool user_update(float dt) override {
		const vf2d mouse_pos(GetMouseX(), GetMouseY());

		{
			const vf2d fwd=spider.loc2wld({1, 0})-spider.pos;
			const vf2d rgt=spider.loc2wld({0, 1})-spider.pos;

			const bool move_fwd=GetKey(SAPP_KEYCODE_W).held;
			const bool move_bkwd=GetKey(SAPP_KEYCODE_S).held;
			const bool move_left=GetKey(SAPP_KEYCODE_Q).held;
			const bool move_right=GetKey(SAPP_KEYCODE_E).held;
			const bool turn_left=GetKey(SAPP_KEYCODE_A).held;
			const bool turn_right=GetKey(SAPP_KEYCODE_D).held;

			vf2d movement;
			const float move_fb_fac=move_fwd?1:move_bkwd?-.75f:0;
			movement+=200*move_fb_fac*dt*fwd;
			const float move_rl_fac=move_right?1:move_left?-1:0;
			movement+=100*move_rl_fac*dt*rgt;
			spider.pos+=clampVec(movement, 225);
			const int turn_fac=turn_left?-1:turn_right?1:0;
			spider.rot+=2*turn_fac*dt;
		}

		spider.update();

		return true;
	}

#pragma region RENDER_HELPERS
	template<int N>
	void renderLegSegments(const Leg<N>& l) {
		sgl_begin_line_strip();
		for(int i=0; i<l.num; i++) {
			const auto& p=l.pts[i];
			sgl_v2f(p.x, p.y);
		}
		sgl_end();
	}

	template<int N>
	void renderLegJoints(const Leg<N>& l) {
		sgl_begin_points();
		sgl_point_size(1);
		for(int i=0; i<l.num; i++) {
			const auto& p=l.pts[i];
			sgl_v2f(p.x, p.y);
		}
		sgl_end();
	}

	void renderGrid(
		float res,
		float r1, float g1, float b1,
		float r2, float g2, float b2
	) {
		sgl_begin_points();
		sgl_point_size(res);
		int num_x=1+sapp_widthf()/res;
		int num_y=1+sapp_heightf()/res;
		for(int i=0; i<=num_x; i++) {
			for(int j=0; j<=num_y; j++) {
				if((i+j)%2) sgl_c3f(r1, g1, b1);
				else sgl_c3f(r2, g2, b2);
				sgl_v2f((.5f+i)*res, (.5f+j)*res);
			}
		}
		sgl_end();
	}

	void renderSpider() {
		//axes
		{
			const float sz=10;
			const vf2d ctr=spider.pos;
			const vf2d x=spider.loc2wld({sz, 0});
			const vf2d y=spider.loc2wld({0, sz});
			sgl_begin_lines();
			sgl_c3f(1, 0, 0);
			sgl_v2f(ctr.x, ctr.y), sgl_v2f(x.x, x.y);
			sgl_c3f(0, 0, 1);
			sgl_v2f(ctr.x, ctr.y), sgl_v2f(y.x, y.y);
			sgl_end();
		}

		//body
		{
			const auto& l=spider.length;
			const auto& w=spider.width;
			const vf2d fl=spider.loc2wld({.5f*l, -.5f*w});
			const vf2d fr=spider.loc2wld({.5f*l, .5f*w});
			const vf2d bl=spider.loc2wld({-.5f*l, -.5f*w});
			const vf2d br=spider.loc2wld({-.5f*l, .5f*w});
			sgl_begin_line_strip();
			sgl_c3f(0, 0, 0);
			sgl_v2f(fl.x, fl.y);
			sgl_v2f(fr.x, fr.y);
			sgl_v2f(br.x, br.y);
			sgl_v2f(bl.x, bl.y);
			sgl_v2f(fl.x, fl.y);
			sgl_end();
		}

		//legs
		for(int i=0; i<spider.num; i++) {
			const auto& l=spider.legs[i];
			sgl_c3f(0, 0, 0);
			renderLegSegments(l);
			sgl_c3f(1, 1, 1);
			renderLegJoints(l);
		}
	}
#pragma endregion

	bool user_render() override {
		//display pass
		sg_pass pass{};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_matrix_mode_projection();
		sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);
		
		renderGrid(
			20,
			59/255.f, 59/255.f, 59/255.f,
			46/255.f, 46/255.f, 46/255.f
		);

		renderSpider();

		sgl_draw();

		sg_end_pass();

		sg_commit();

		return true;
	}

	void user_destroy() override {
		sgl_shutdown();
	}
};