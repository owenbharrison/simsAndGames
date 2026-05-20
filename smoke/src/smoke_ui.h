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

#include "smoke.h"

#include <list>

#include "cmn/utils.h"

//for time
#include <ctime>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/include/stb_image.h"

using cmn::vf2d;

class SmokeUI : public cmn::SokolEngine {
	//scene
	std::list<Smoke> smokes;
	vf2d buoyancy{0, -120};
	bool update_emitter=false;
	bool update_physics=true;
	
	//ui
	vf2d mouse_pos;
	vf2d prev_mouse_pos;

	vf2d emitter_pos;
	float emitter_r=0;
	float emitter_g=0;
	float emitter_b=0;

	//graphics
	sgl_pipeline pip{};
	
	sg_sampler smp{};

	sg_view tex{};

	bool show_sprites=true;
	bool show_outlines=false;

public:
#pragma region SETUP_HELPERS
	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);
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

	void setupSampler() {
		sg_sampler_desc smp_desc{};
		smp=sg_make_sampler(smp_desc);
	}

	bool setupTexture() {
		int width, height, comp;
		stbi_uc* pixels=stbi_load("assets/smoke.png", &width, &height, &comp, 4);
		if(!pixels) return false;

		sg_image_desc img_desc{};
		img_desc.width=width;
		img_desc.height=height;
		img_desc.data.mip_levels[0].ptr=pixels;
		img_desc.data.mip_levels[0].size=sizeof(stbi_uc)*4*width*height;
		sg_image img=sg_make_image(img_desc);

		sg_view_desc view_desc{};
		view_desc.texture.image=img;
		tex=sg_make_view(view_desc);

		return true;
	}
#pragma endregion

	bool user_create() override {
		app_title="Smoke";
		
		std::srand(std::time(0));

		emitter_pos=vf2d(.2f*sapp_widthf(), .75f*sapp_heightf());

		setupSGL();

		setupPipeline();

		setupSampler();

		if(!setupTexture()) return false;

		return true;
	}

#pragma region UPDATE HELPERS
	//randomize emitter col
	void handleEmitBegin() {
		emitter_r=cmn::randFloat();
		emitter_g=cmn::randFloat();
		emitter_b=cmn::randFloat();
	}

	//add particles at mouse
	void handleEmitUpdate() {
		Smoke smoke;
		smoke.pos=emitter_pos;
		vf2d sub=mouse_pos-emitter_pos;
		float dist=sub.mag();
		vf2d dir;
		if(dist!=0) dir=sub/dist;
		float speed=std::max(0.f, dist+cmn::randFloat(-100, 100));
		smoke.vel=speed*dir;
		smoke.rot=cmn::randFloat(2*cmn::Pi);
		smoke.rot_vel=cmn::randFloat(-1.5f, 1.5f);
		smoke.size=cmn::randFloat(10, 20);
		smoke.size_vel=cmn::randFloat(15, 25);
		smoke.r=emitter_r;
		smoke.g=emitter_g;
		smoke.b=emitter_b;
		smokes.push_back(smoke);
	}

	void handleParticleBomb() {
		//randomize col each click
		float smoke_r=cmn::randFloat();
		float smoke_g=cmn::randFloat();
		float smoke_b=cmn::randFloat();

		//spawn a random number
		int num=cmn::randInt(20, 40);
		for(int i=0; i<num; i++) {
			Smoke smoke;
			smoke.pos=mouse_pos;
			float speed=cmn::randFloat(30, 70);
			smoke.vel=cmn::polar<vf2d>(speed, cmn::randFloat(2*cmn::Pi));
			smoke.rot=cmn::randFloat(2*cmn::Pi);
			smoke.rot_vel=cmn::randFloat(-1.5f, 1.5f);
			smoke.size=cmn::randFloat(10, 20);
			smoke.size_vel=cmn::randFloat(15, 25);
			smoke.r=smoke_r;
			smoke.g=smoke_g;
			smoke.b=smoke_b;
			smokes.push_back(smoke);
		}
	}

	void handleUserInput(float dt) {
		const auto emit_action=GetMouse(SAPP_MOUSEBUTTON_LEFT);
		bool emit_rainbow=GetMouse(SAPP_MOUSEBUTTON_RIGHT).held;
		if(emit_action.pressed||emit_rainbow) handleEmitBegin();
		if(emit_action.held||emit_rainbow) handleEmitUpdate();

		if(GetMouse(SAPP_MOUSEBUTTON_MIDDLE).pressed) {
			handleParticleBomb();
		}

		//set emitter pos
		if(update_emitter) emitter_pos=mouse_pos;

		//gfx toggles
		if(GetKey(SAPP_KEYCODE_E).pressed) update_emitter^=true;
		if(GetKey(SAPP_KEYCODE_S).pressed) show_sprites^=true;
		if(GetKey(SAPP_KEYCODE_O).pressed) show_outlines^=true;
		if(GetKey(SAPP_KEYCODE_P).pressed) update_physics^=true;
	}
#pragma endregion

	bool user_update(float dt) override {
		prev_mouse_pos=mouse_pos;
		mouse_pos.x=GetMouseX();
		mouse_pos.y=GetMouseY();

		handleUserInput(dt);

		//update smokes
		if(update_physics) {
			for(auto& p:smokes) {
				//drag
				p.vel*=1-.85f*dt;
				p.rot_vel*=1-.55f*dt;
				p.size_vel*=1-.25f*dt;

				//kinematics
				p.accelerate(buoyancy);
				p.update(dt);

				//aging?
				p.life-=.25f*dt;
			}
		}

		//remove offscreen smokes
		for(auto it=smokes.begin(); it!=smokes.end();) {
			if(it->isDead()) {
				it=smokes.erase(it);
			} else it++;
		}

		return true;
	}

#pragma region RENDER HELPERS
	void renderEmitter() {
		float rad=4.7f;
		vf2d norm=rad*(mouse_pos-emitter_pos).norm();
		vf2d tang(-norm.y, norm.x);

		sgl_begin_lines();

		sgl_c3f(1, 1, 1);

		vf2d a=emitter_pos-norm, b=emitter_pos+2.f*norm;
		sgl_v2f(a.x, a.y), sgl_v2f(b.x, b.y);
		vf2d c=emitter_pos-tang, d=emitter_pos+tang;
		sgl_v2f(c.x, c.y), sgl_v2f(d.x, d.y);

		sgl_end();
	}

	void renderSmoke(const Smoke& sm) {
		//precompute rotation matrix
		float c=std::cos(sm.rot), s=std::sin(sm.rot);

		//for each corner
		vf2d corners[4]{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
		for(int i=0; i<4; i++) {
			//rotate, scale, & translate
			corners[i]=sm.pos+sm.size/2*vf2d(
				c*corners[i].x-s*corners[i].y,
				s*corners[i].x+c*corners[i].y
			);
		}
		
		if(show_sprites) {
			sgl_enable_texture();
			sgl_texture(tex, smp);
		}
		
		//tint
		sgl_c4f(sm.r, sm.g, sm.b, sm.life);

		sgl_begin_quads();

		sgl_v2f_t2f(corners[0].x, corners[0].y, 0, 0);
		sgl_v2f_t2f(corners[1].x, corners[1].y, 1, 0);
		sgl_v2f_t2f(corners[2].x, corners[2].y, 1, 1);
		sgl_v2f_t2f(corners[3].x, corners[3].y, 0, 1);

		sgl_end();

		sgl_disable_texture();

		if(show_outlines) {
			sgl_begin_lines();

			//alpha based on smoke "age"
			sgl_c4f(1, 1, 1, sm.life);

			//draw box w/ diagonal
			sgl_v2f(corners[0].x, corners[0].y), sgl_v2f(corners[1].x, corners[1].y);
			sgl_v2f(corners[1].x, corners[1].y), sgl_v2f(corners[2].x, corners[2].y);
			sgl_v2f(corners[2].x, corners[2].y), sgl_v2f(corners[3].x, corners[3].y);
			sgl_v2f(corners[3].x, corners[3].y), sgl_v2f(corners[0].x, corners[0].y);
			sgl_v2f(corners[0].x, corners[0].y), sgl_v2f(corners[2].x, corners[2].y);

			sgl_end();
		}
	}
#pragma endregion

	bool user_render() override {
		//display pass
		sg_pass pass{};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_projection();
		sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);

		//grey background
		cmn::fill_rect(0, 0, sapp_widthf(), sapp_heightf(), .5f, .5f, .5f);

		if(!update_emitter) renderEmitter();

		for(const auto& p:smokes) {
			renderSmoke(p);
		}

		sgl_draw();

		sg_end_pass();

		sg_commit();

		return true;
	}
};