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

#include "cmn/math/v3d.h"
#include "cmn/math/mat4.h"

#include "phys/particle.h"
#include "phys/spring.h"

#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/include/stb_image.h"

#include <list>

#include <vector>

//for pi
#include "cmn/utils.h"

#include "perlin_noise.h"

//for time
#include <ctime>

struct IndexTriangle {
	Particle* a=0, * b=0, * c=0;
};

//y p => x y z
//0 0 => 0 0 1
static cmn::vf3d polarToCartesian(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

//x y z => y p
//0 0 1 => 0 0
static void cartesianToPolar(const cmn::vf3d& pt, float& yaw, float& pitch) {
	//flatten onto xz
	yaw=std::atan2(pt.x, pt.z);
	//vertical triangle
	pitch=std::atan2(pt.y, std::sqrt(pt.x*pt.x+pt.z*pt.z));
}

void compressiveGradient(float t, float* r, float* g, float* b) {
	static const float cols[][3]{
		{1, 1, 1},//white
		{1, 1, 0},//yellow
		{1, 0, 0}//red
	};
	return cmn::colorGradient(
		cols, sizeof(cols)/sizeof(cols[0]),
		t, r, g, b
	);
}

void tensileGradient(float t, float* r, float* g, float* b) {
	static const float cols[][3]{
		{1, 1, 1},//white
		{0, 1, 1},//cyan
		{0, 0, 1}//blue
	};
	return cmn::colorGradient(
		cols, sizeof(cols)/sizeof(cols[0]),
		t, r, g, b
	);
}

cmn::vf3d rayIntersectPlane(
	const cmn::vf3d& orig, const cmn::vf3d& dir,
	const cmn::vf3d& ctr, const cmn::vf3d& norm
) {
	float dist=norm.dot(ctr-orig)/norm.dot(dir);
	return orig+dist*dir;
}

using cmn::vf3d;
using cmn::mat4;

class Cloth : public cmn::SokolEngine {
	//physics stuff
	std::list<Particle> particles;
	std::vector<Spring> springs;
	std::vector<IndexTriangle> triangles;

	const vf3d gravity{0, -9.8f, 0};

	bool update_phys=true;
	const float time_step=1/240.f;
	float update_timer=0;

	PerlinNoise noise_gen;

	float total_dt=0;

	//user input
	struct {
		vf3d pos{.75f, 0, 4};
		float yaw=0, pitch=0;
		vf3d dir{0, 0, -1};

		float fov_deg=75;

		const float near_plane=.001f, far_plane=1000;
		mat4 proj;

		mat4 view;

		mat4 view_proj;
	} cam;

	vf3d mouse_dir;

	const float select_rad=5;

	Particle* grab_ptc=nullptr;
	vf3d grab_st;
	vf3d grab_norm;
	vf3d grab_pt;

	//graphics
	sgl_pipeline depth_pip{};

	sg_sampler smp{};

	std::vector<sg_view> flags;
	int flag_ix=0;

	bool render_outlines=false;
	bool render_bounds=false;

public:
#pragma CREATE_HELPERS
	void setupCloth() {
		//sizing
		float width=cmn::randFloat(2.75f, 3.25f);
		float height=cmn::randFloat(1.75f, 2.25f);
		float area_sqm=width*height;

		//allocate grid
		float cell_sz=cmn::randFloat(.075f, .1f);
		int num_x=1+width/cell_sz;
		int num_y=1+height/cell_sz;
		int num=num_x*num_y;
		Particle** grid=new Particle*[num];
		auto ix=[&] (int i, int j) { return i+num_x*j; };

		//mass properties given common flag size
		float dens_lb_sqft=.75f/(3*5);
		float dens_kg_sqft=dens_lb_sqft/2.205f;
		float dens_kg_sqm=dens_kg_sqft*10.76f;
		float mass_kg=dens_kg_sqm*area_sqm;
		float mass_per=mass_kg/num;

		//start cloth on on xy plane
		for(int i=0; i<num_x; i++) {
			float x01=i/(num_x-1.f);
			float x=width*(x01-.5f);
			for(int j=0; j<num_y; j++) {
				float y01=j/(num_y-1.f);
				float y=height*(y01-.5f);
				Particle p({x, y, 0}, mass_per);
				if(i==0) p.locked=true;
				p.tex_u=x01;
				p.tex_v=1-y01;
				particles.push_back(p);
				grid[ix(i, j)]=&particles.back();
			}
		}

		//connect springs adjacently
		const float k=18.27f;
		const float d=.7653f;
		for(int i=0; i<num_x; i++) {
			for(int j=0; j<num_y; j++) {
				if(i>0) springs.push_back(Spring(grid[ix(i, j)], grid[ix(i-1, j)], k, d));
				if(j>0) springs.push_back(Spring(grid[ix(i, j)], grid[ix(i, j-1)], k, d));
				if(i>0&&j>0) {
					springs.push_back(Spring(grid[ix(i-1, j-1)], grid[ix(i, j)], k, d));
					springs.push_back(Spring(grid[ix(i-1, j)], grid[ix(i, j-1)], k, d));
				}
			}
		}

		//tesselate surface
		for(int i=1; i<num_x; i++) {
			for(int j=1; j<num_y; j++) {
				const auto& k00=grid[ix(i-1, j-1)];
				const auto& k01=grid[ix(i-1, j)];
				const auto& k10=grid[ix(i, j-1)];
				const auto& k11=grid[ix(i, j)];
				triangles.push_back({k00, k01, k10});
				triangles.push_back({k01, k11, k10});
			}
		}

		delete[] grid;
	}

	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);
	}

	void setupPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		depth_pip=sgl_make_pipeline(pip_desc);
	}

	void setupSampler() {
		sg_sampler_desc smp_desc{};
		smp=sg_make_sampler(smp_desc);
	}

	bool setupFlags() {
		const std::vector<std::string> filenames{
			"assets/barbados.png",
			"assets/bhutan.png",
			"assets/brazil.png",
			"assets/colorado.png",
			"assets/djibouti.png",
			"assets/portugal.png",
			"assets/seychelles.png",
			"assets/south_korea.png",
			"assets/wales.png",
			"assets/usa.png"
		};
		for(const auto& f:filenames) {
			int width, height, comp;
			stbi_uc* pixels8=stbi_load(f.c_str(), &width, &height, &comp, 4);
			if(!pixels8) return false;

			sg_image_desc img_desc{};
			img_desc.width=width;
			img_desc.height=height;
			img_desc.data.mip_levels[0].ptr=pixels8;
			img_desc.data.mip_levels[0].size=sizeof(stbi_uc)*4*width*height;
			sg_image img=sg_make_image(img_desc);

			stbi_image_free(pixels8);

			sg_view_desc view_desc{};
			view_desc.texture.image=img;
			flags.push_back(sg_make_view(view_desc));
		}

		flag_ix=std::rand()%flags.size();

		return true;
	}
#pragma endregion

	bool user_create() override {
		app_title="Cloth Simulation";

		auto now=std::time(0);
		std::srand(now);

		setupCloth();

		noise_gen=PerlinNoise(now);

		cartesianToPolar(cam.dir, cam.yaw, cam.pitch);

		setupSGL();

		setupPipeline();

		setupSampler();

		if(!setupFlags()) return false;

		return true;
	}

#pragma region UPDATE_HELPERS
	void handleCameraLooking(float dt) {
		//don't look while grabbing
		if(grab_ptc) return;

		//left/right
		if(GetKey(SAPP_KEYCODE_LEFT).held) cam.yaw+=dt;
		if(GetKey(SAPP_KEYCODE_RIGHT).held) cam.yaw-=dt;

		//up/down
		if(GetKey(SAPP_KEYCODE_UP).held) cam.pitch+=dt;
		if(GetKey(SAPP_KEYCODE_DOWN).held) cam.pitch-=dt;

		//clamp camera pitch
		if(cam.pitch>cmn::Pi/2) cam.pitch=cmn::Pi/2-.001f;
		if(cam.pitch<-cmn::Pi/2) cam.pitch=.001f-cmn::Pi/2;
	}

	void handleCameraMovement(float dt) {
		//don't move while grabbing
		if(grab_ptc) return;

		//move up, down
		if(GetKey(SAPP_KEYCODE_SPACE).held) cam.pos.y+=4.f*dt;
		if(GetKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam.pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::sin(cam.yaw), 0, std::cos(cam.yaw));
		if(GetKey(SAPP_KEYCODE_W).held) cam.pos+=5.f*dt*fb_dir;
		if(GetKey(SAPP_KEYCODE_S).held) cam.pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(SAPP_KEYCODE_A).held) cam.pos+=4.f*dt*lr_dir;
		if(GetKey(SAPP_KEYCODE_D).held) cam.pos-=4.f*dt*lr_dir;
	}

	void handleGrabAction() {
		const auto grab_action=GetMouse(SAPP_MOUSEBUTTON_LEFT);
		if(grab_action.pressed) {
			grab_ptc=nullptr;

			//angular size of radius pixels
			float fov_rad=cam.fov_deg/180*cmn::Pi;
			float h_view=2*std::tan(.5f*fov_rad);
			float pix2wld=select_rad*h_view/sapp_heightf();

			float record;
			for(auto& p:particles) {
				//parallel & perp dist
				vf3d cp=p.pos-cam.pos;
				float l=std::abs(mouse_dir.dot(cp));
				float r=mouse_dir.cross(cp).mag();

				float max_r=l*pix2wld;
				if(r<max_r) {
					if(!grab_ptc||r<record) {
						record=r;
						grab_ptc=&p;
					}
				}
			}
			if(grab_ptc) {
				grab_st=grab_ptc->pos;
				grab_norm=cam.dir;
			}
		}
		if(grab_action.held) {
			if(grab_ptc) {
				grab_pt=rayIntersectPlane(
					cam.pos, mouse_dir,
					grab_st, grab_norm
				);
			}
		}
		if(grab_action.released) grab_ptc=nullptr;
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		handleCameraMovement(dt);

		handleGrabAction();

		//flag choice!
		for(int i=0; i<flags.size(); i++) {
			auto key=sapp_keycode(SAPP_KEYCODE_0+i);
			if(GetKey(key).pressed) flag_ix=i;
		}

		//debug toggles
		if(GetKey(SAPP_KEYCODE_P).pressed) update_phys^=true;
		if(GetKey(SAPP_KEYCODE_O).pressed) render_outlines^=true;
		if(GetKey(SAPP_KEYCODE_B).pressed) render_bounds^=true;
	}

	void updateCameraMatrixes() {
		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		//cam proj can change with window resize
		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(cam.fov_deg, asp, cam.near_plane, cam.far_plane);

		cam.view_proj=mat4::mul(cam.proj, cam.view);
	}

	void updateMouseRay() {
		//unprojection matrix
		mat4 inv_view_proj=mat4::inverse(cam.view_proj);

		//mouse coords from clip -> world
		float ndc_x=2*GetMouseX()/sapp_widthf()-1;
		float ndc_y=1-2*GetMouseY()/sapp_heightf();
		vf3d clip(ndc_x, ndc_y, 1);
		float w=1;
		vf3d world=matMulVec(inv_view_proj, clip, w);
		world/=w;

		//normalize direction
		mouse_dir=(world-cam.pos).norm();
	}

	//just a lot of fiddling.
	vf3d getWind(vf3d pos, float time) {
		const float scl=.1f;
		vf3d wind(
			.5f-noise_gen.noise(scl*pos.y, scl*pos.z, time),
			.5f-noise_gen.noise(100+scl*pos.x, 100+scl*pos.y, 100+time),
			.5f-noise_gen.noise(200+scl*pos.z, 200+scl*pos.x, 200+time)
		);

		//prevalent in x
		wind+={1.46f, .3f, 0};

		//scale z
		wind.z*=2.5f;

		float strength=6.4f;
		return strength*wind;
	}

	void handlePhysics(float dt) {
		//ensure similar update across multiple framerates?
		update_timer+=dt;
		while(update_timer>time_step) {
			//update springs
			for(auto& s:springs) {
				s.update();
			}

			//apply wind & drag
			for(auto& t:triangles) {
				auto& a=*t.a;
				auto& b=*t.b;
				auto& c=*t.c;

				vf3d ab=b.pos-a.pos;
				vf3d ac=c.pos-a.pos;

				vf3d norm=ab.cross(ac);
				float mag=norm.mag();
				if(mag<1e-6f) continue;

				float area=.5f*mag;
				norm/=mag;

				//particle velocities
				vf3d vel_a=(a.pos-a.old_pos)/time_step;
				vf3d vel_b=(b.pos-b.old_pos)/time_step;
				vf3d vel_c=(c.pos-c.old_pos)/time_step;

				vf3d ctr=(a.pos+b.pos+c.pos)/3;
				vf3d wind=getWind(ctr, .1f*total_dt);

				//relative wind vel
				vf3d vel=(vel_a+vel_b+vel_c)/3;
				vf3d rel_vel=wind-vel;

				float rel_spd=rel_vel.mag();
				if(rel_spd<1e-6f) continue;

				vf3d rel_dir=rel_vel/rel_spd;

				float exposure=std::abs(norm.dot(rel_dir));

				//aerodynamic pressure
				const float air_dens=1.225f;
				float pressure=.5f*air_dens*rel_spd*rel_spd;

				const float drag_coeff=1.2f;
				vf3d force=pressure*drag_coeff*exposure*area*rel_dir;

				vf3d per_vertex=force/3;

				a.applyForce(per_vertex);
				b.applyForce(per_vertex);
				c.applyForce(per_vertex);
			}

			//spring towards grab point
			if(grab_ptc) {
				Particle p_temp(grab_pt, 1);
				p_temp.locked=true;
				Spring s_temp(grab_ptc, &p_temp, 25, 0);
				s_temp.rest_len=0;
				s_temp.update();
			}

			//update particles
			for(auto& p:particles) {

				p.applyForce(p.mass*gravity);

				p.update(time_step);
			}

			update_timer-=time_step;
		}
	}
#pragma endregion

	bool user_update(float dt) override {
		handleUserInput(dt);

		cam.dir=polarToCartesian(cam.yaw, cam.pitch);

		updateCameraMatrixes();

		updateMouseRay();

		if(update_phys) handlePhysics(dt);

		total_dt+=dt;

		return true;
	}

#pragma region RENDER_HELPERS
	void renderFlag() {
		sgl_enable_texture();
		sgl_texture(flags[flag_ix], smp);

		sgl_begin_triangles();
		for(const auto& t:triangles) {
			sgl_v3f_t2f(t.a->pos.x, t.a->pos.y, t.a->pos.z, t.a->tex_u, t.a->tex_v);
			sgl_v3f_t2f(t.b->pos.x, t.b->pos.y, t.b->pos.z, t.b->tex_u, t.b->tex_v);
			sgl_v3f_t2f(t.c->pos.x, t.c->pos.y, t.c->pos.z, t.c->tex_u, t.c->tex_v);
		}
		sgl_end();

		sgl_disable_texture();
	}

	void renderGrabbing(float r, float g, float b) {
		if(!grab_ptc) return;

		sgl_begin_lines();
		sgl_c3f(r, g, b);
		sgl_v3f(grab_ptc->pos.x, grab_ptc->pos.y, grab_ptc->pos.z);
		sgl_v3f(grab_pt.x, grab_pt.y, grab_pt.z);
		sgl_end();
	}

	void renderBounds(float r, float g, float b) {
		vf3d min{1e30f, 1e30f, 1e30f};
		vf3d max{-1e30f, -1e30f, -1e30f};
		for(const auto& p:particles) {
			if(p.pos.x<min.x) min.x=p.pos.x;
			if(p.pos.y<min.y) min.y=p.pos.y;
			if(p.pos.z<min.z) min.z=p.pos.z;
			if(p.pos.x>max.x) max.x=p.pos.x;
			if(p.pos.y>max.y) max.y=p.pos.y;
			if(p.pos.z>max.z) max.z=p.pos.z;
		}
		sgl_begin_lines();
		sgl_c3f(r, g, b);
		//xy-
		sgl_v3f(min.x, min.y, min.z), sgl_v3f(max.x, min.y, min.z);
		sgl_v3f(max.x, min.y, min.z), sgl_v3f(max.x, max.y, min.z);
		sgl_v3f(max.x, max.y, min.z), sgl_v3f(min.x, max.y, min.z);
		sgl_v3f(min.x, max.y, min.z), sgl_v3f(min.x, min.y, min.z);
		//thru z
		sgl_v3f(min.x, min.y, min.z), sgl_v3f(min.x, min.y, max.z);
		sgl_v3f(max.x, min.y, min.z), sgl_v3f(max.x, min.y, max.z);
		sgl_v3f(min.x, max.y, min.z), sgl_v3f(min.x, max.y, max.z);
		sgl_v3f(max.x, max.y, min.z), sgl_v3f(max.x, max.y, max.z);
		//xy+
		sgl_v3f(min.x, min.y, max.z), sgl_v3f(max.x, min.y, max.z);
		sgl_v3f(max.x, min.y, max.z), sgl_v3f(max.x, max.y, max.z);
		sgl_v3f(max.x, max.y, max.z), sgl_v3f(min.x, max.y, max.z);
		sgl_v3f(min.x, max.y, max.z), sgl_v3f(min.x, min.y, max.z);
		sgl_end();
	}

	void renderSprings() {
		//find max stresses(mag)
		float max_tens=1e-6f, max_comp=1e-6f;
		for(const auto& s:springs) {
			if(s.strain>max_tens) max_tens=s.strain;
			if(-s.strain>max_comp) max_comp=-s.strain;
		}

		sgl_begin_lines();
		for(const auto& s:springs) {
			const auto& pa=s.a->pos;
			const auto& pb=s.b->pos;

			float r, g, b;
			if(s.strain>0) tensileGradient(s.strain/max_tens, &r, &g, &b);
			else compressiveGradient(-s.strain/max_comp, &r, &g, &b);

			sgl_v3f_c3f(pa.x, pa.y, pa.z, r, g, b);
			sgl_v3f_c3f(pb.x, pb.y, pb.z, r, g, b);
		}
		sgl_end();
	}
#pragma endregion

	bool user_render() override {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={.25f, .25f, .25f, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_matrix_mode_projection();
		sgl_load_matrix(cam.proj.m);
		sgl_matrix_mode_modelview();
		sgl_load_matrix(cam.view.m);

		sgl_load_pipeline(depth_pip);

		renderFlag();

		//purple
		renderGrabbing(.5f, 0, 1);

		//black
		if(render_bounds) renderBounds(0, 0, 0);
		
		sgl_load_default_pipeline();

		//draw on top
		if(render_outlines) renderSprings();

		sgl_draw();

		sg_end_pass();

		sg_commit();

		return true;
	}
};