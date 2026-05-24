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

#include "canvas.h"

#include "post_process.glsl.h"

#include "game/ship.h"

#include "game/asteroid.h"

#include "game/bullet.h"

#include "game/particle.h"

#include <vector>

struct Glyph {
	float r, g, b;
	char c;
};

using cmn::vf2d;
using cmn::vi2d;

class Asteroids : public cmn::SokolEngine {
	//graphics
	sgl_pipeline sgl_pip{};

	sg_sampler smp{};

	int width, height;
	Canvas canvas[2];

	sg_pipeline ascii_pip{};
	sg_pipeline crt_pip{};

	sg_buffer quad_vbuf{};

	float total_dt=0;

	//game
	float particle_timer=0;
	float bullet_timer=0;
	float score_timer=0;
	float warning_timer=0;
	float end_timer=0;

	Ship ship;
	cmn::AABBf2 screen_bounds;
	std::vector<Asteroid> asteroids;
	std::vector<Bullet> bullets;
	std::vector<Particle> particles;

	int score=0, level=0;
	enum State {
		Playing,
		Won,
		Lost
	} state=Playing;
	int warning_ct=0, end_ct=0;
	bool debug_mode=false;

#pragma region SETUP_HELPERS
	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);

		sg_pipeline_desc pip_desc{};
		pip_desc.colors[0].write_mask=SG_COLORMASK_RGBA;
		sgl_pip=sgl_make_pipeline(pip_desc);
	}

	void setupSampler() {
		sg_sampler_desc smp_desc{};
		smp_desc.wrap_u=SG_WRAP_CLAMP_TO_EDGE;
		smp_desc.wrap_v=SG_WRAP_CLAMP_TO_EDGE;
		smp=sg_make_sampler(smp_desc);
	}

	void setupASCIIPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_ascii_i_pos].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_ascii_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(ascii_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		ascii_pip=sg_make_pipeline(pip_desc);
	}

	void setupCRTPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_crt_i_pos].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_crt_i_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader=sg_make_shader(crt_shader_desc(sg_query_backend()));
		pip_desc.primitive_type=SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		crt_pip=sg_make_pipeline(pip_desc);
	}

	void setupQuadVertexBuffer() {
		//xyuv
		float vertexes[][4]{
			{-1, -1, 0, 0},
			{1, -1, 1, 0},
			{-1, 1, 0, 1},
			{1, 1, 1, 1}
		};
		sg_buffer_desc buffer_desc{};
		buffer_desc.data=SG_RANGE(vertexes);
		quad_vbuf=sg_make_buffer(buffer_desc);
	}

	void setupGame() {
		const vf2d res(width, height);

		//extend screen bounds past edges
		vf2d margin=2.5f*vf2d(1, 1);
		screen_bounds={-margin, res+margin};

		//ship in center of screen
		ship=Ship(res/2, 2.5f);
		ship.rot=cmn::randFloat(2*cmn::Pi);
	}
#pragma endregion

	bool user_create() override {
		app_title="Asteroids";

		setupSGL();

		setupSampler();

		width=sapp_width()/8;
		height=sapp_height()/8;
		canvas[0].resize(width, height);
		canvas[1].resize(8*width, 8*height);

		setupASCIIPipeline();

		setupCRTPipeline();

		setupQuadVertexBuffer();

		setupGame();

		return true;
	}

#pragma region UPDATE_HELPERS
	void handleUserInput(float dt) {
		//if "alive"
		bool boosting=GetKey(SAPP_KEYCODE_UP).held;
		if(state==Playing) {
			//ship movement
			if(boosting)  ship.boost(87.43f);

			//limit number of particles spawned
			while(particle_timer<0) {
				particle_timer+=.006f;
				if(boosting) particles.push_back(ship.emitParticle());
			}
			particle_timer-=dt;

			float turn_speed=2.78f;
			if(GetKey(SAPP_KEYCODE_RIGHT).held) ship.turn(turn_speed*dt);
			if(GetKey(SAPP_KEYCODE_LEFT).held) ship.turn(-turn_speed*dt);

			//limit number of bullets shot
			if(bullet_timer<0) {
				if(GetKey(SAPP_KEYCODE_SPACE).pressed) {
					bullet_timer=.25f;
					bullets.push_back(ship.getBullet());
				}
			} else bullet_timer-=dt;
		}

		//toggles
		if(GetKey(SAPP_KEYCODE_D).pressed) debug_mode^=true;
	}

	void handleAsteroidBulletCollision() {
		std::vector<Asteroid> to_add;
		for(auto ait=asteroids.begin(); ait!=asteroids.end();) {
			auto& a=*ait;

			//check all bullets
			bool hit_bullet=false;
			for(auto bit=bullets.begin(); bit!=bullets.end(); bit++) {
				if(a.contains(bit->pos)) {
					//delete bullet
					bullets.erase(bit);
					hit_bullet=true;
					break;
				}
			}

			if(hit_bullet) {
				//if we can, split the asteroid
				int num_ptc=0;
				vf2d pos;
				Asteroid a0, a1;
				if(a.split(a0, a1)) {
					to_add.push_back(a0);
					to_add.push_back(a1);

					//emit some particles for fx
					num_ptc=a.base_rad*cmn::randFloat(2, 4);
					pos=a.pos;

					//increment score
					score+=8;
				} else {//when we "fully" break an asteroid
					//emit more particles
					num_ptc=a.base_rad*cmn::randFloat(5, 7);
					pos=a.pos;

					//increment score more
					score+=24;
				}

				//spawn particles
				for(int k=0; k<num_ptc; k++) {
					particles.push_back(Particle::makeRandom(pos, .7f, .7f, .7f));
				}

				//finally, remove it
				ait=asteroids.erase(ait);
			} else ait++;
		}
		asteroids.insert(asteroids.end(), to_add.begin(), to_add.end());
	}

	void handleAsteroidShipCollision() {
		vf2d ship_outline[3];
		ship.getOutline(ship_outline[0], ship_outline[1], ship_outline[2]);
		for(const auto& a:asteroids) {
			if(state==Playing) {
				bool ship_hit=false;

				//check asteroid against all ship lines
				for(int i=0; i<3; i++) {
					vf2d sp0=a.wld2loc(ship_outline[i]);
					vf2d sp1=a.wld2loc(ship_outline[(i+1)%3]);

					//to all asteroid lines
					for(int j=0; j<a.num_pts; j++) {
						vf2d ap0=a.model[j];
						vf2d ap1=a.model[(j+1)%a.num_pts];
						vf2d tu=cmn::lineLineIntersection(sp0, sp1, ap0, ap1);

						//if even one hits, game over
						if(tu.x>0&&tu.x<1&&tu.y>0&&tu.y<1) {
							ship_hit=true;
							break;
						}
					}
				}
				if(ship_hit) {
					//end game
					state=Lost;
					int num_ptc=cmn::randInt(56, 84);
					for(int i=0; i<num_ptc; i++) {
						particles.push_back(Particle::makeRandom(ship.pos, 1, 0, 0));
					}
				}
			}
		}
	}

	void handlePhysics(float dt) {
		//ship update
		ship.update(dt);
		ship.checkAABB(screen_bounds);

		//update particles
		for(auto it=particles.begin(); it!=particles.end(); ) {
			auto& p=*it;
			p.update(dt);

			//"dynamically" clear those too old
			if(p.isDead()) {
				it=particles.erase(it);
			} else it++;
		}

		//update bullets
		for(auto it=bullets.begin(); it!=bullets.end(); ) {
			auto& b=*it;
			b.update(dt);

			//"dynamically" clear those offscreen
			if(!screen_bounds.contains({b.pos.x, b.pos.y})) {
				it=bullets.erase(it);
			} else it++;
		}

		//update asteroids
		for(auto& a:asteroids) {
			a.update(dt);
			a.checkAABB(screen_bounds);
		}

		handleAsteroidBulletCollision();

		handleAsteroidShipCollision();
	}

	//award points for living longer
	void handleScoring(float dt) {
		if(state==Playing) {
			if(score_timer<0) {
				score_timer+=2;
				//based on how "hard" to live
				score+=asteroids.size();
			}
			score_timer-=dt;
		}
	}

	//spawn new asteroids on clear
	void handleLevelling(float dt) {
		if(asteroids.empty()) {
			//win case
			if(level==4) state=Won;
			else {
				//blinking
				if(warning_timer<0) {
					//reset
					warning_timer=.4f;
					if(warning_ct<8) warning_ct++;
					else {//add asteroids, next level
						warning_ct=0;

						//add asteroids based on level
						for(int i=0; i<1+2*level; i++) asteroids.push_back(Asteroid::makeRandom(screen_bounds));
						level++;
					}
				}
				warning_timer-=dt;
			}
		}
	}

	void handleBlinking(float dt) {
		if(state!=Playing) {
			if(end_timer<0) {
				end_timer+=.7f;

				end_ct++;
			}

			end_timer-=dt;
		}
	}
#pragma endregion

	bool user_update(float dt) override {
		handleUserInput(dt);

		handlePhysics(dt);

		handleScoring(dt);

		handleLevelling(dt);

		handleBlinking(dt);

		total_dt+=dt;

		return true;
	}

#pragma region RENDER_HELPERS
	//offset these by .5 b/c frag is in ctr?
	void draw_pixel(const vf2d& pos, const Glyph& g) {
		sgl_begin_points();

		sgl_v2f_c4f(.5f+pos.x, .5f+pos.y, g.r, g.g, g.b, g.c/255.f);

		sgl_end();
	}

	void draw_line(const vf2d& a, const vf2d& b, const Glyph& g) {
		cmn::draw_line(
			.5f+a.x, .5f+a.y, .5f+b.x, .5f+b.y,
			g.r, g.g, g.b, g.c/255.f
		);
	}

	void draw_rect(const vf2d& pos, const vf2d& sz, const Glyph& g) {
		cmn::draw_rect(
			.5f+pos.x, .5f+pos.y, sz.x, sz.y,
			g.r, g.g, g.b, g.c/255.f
		);
	}

	void fill_rect(const vf2d& pos, const vf2d& sz, const Glyph& g) {
		cmn::fill_rect(
			.5f+pos.x, .5f+pos.y, sz.x, sz.y,
			g.r, g.g, g.b, g.c/255.f
		);
	}

	void draw_string(const vf2d& pos, const std::string& str, float r, float g, float b) {
		vi2d off;
		for(const auto& c:str) {
			if(c==' ') off.x++;
			//tabsize=2
			else if(c=='\t') off.x+=2;
			else if(c=='\n') off.x=0, off.y++;
			else if(c>='!'&&c<='~') {
				draw_pixel(pos+off, {r, g, b, c});
				off.x++;
			}
		}
	}

	void renderDebug() {
		const Glyph red{1, 0, 0, '.'};
		const Glyph blue{0, 0, 1, '.'};
		const Glyph green{0, 1, 0, '.'};
		const Glyph yellow{.8f, .8f, 0, '.'};

		//show asteroid pos, vel, & bounds
		for(const auto& a:asteroids) {
			draw_line(a.pos, ship.pos, red);
			draw_line(a.pos, a.pos+a.vel, blue);
			auto a_box=a.getAABB();
			vf2d tl{a_box.min.x, a_box.min.y};
			vf2d br{a_box.max.x, a_box.max.y};
			draw_rect(tl, br-tl, green);
		}

		//show bullet pos and vel
		for(const auto& b:bullets) {
			draw_line(b.pos, ship.pos, yellow);
			draw_line(b.pos, b.pos+b.vel, blue);
		}

		//show ship vel & bounds
		draw_line(ship.pos, ship.pos+ship.vel, blue);
		auto ship_box=ship.getAABB();
		vf2d tl{ship_box.min.x, ship_box.min.y};
		vf2d br{ship_box.max.x, ship_box.max.y};
		draw_rect(tl, br-tl, green);
	}

	//show bullets as tiny circles
	void renderBullets(const Glyph& g) {
		for(const auto& b:bullets) {
			fill_rect(b.pos-1, {2, 2}, g);
		}
	}

	//show asteroid outlines
	void renderAsteroids(const Glyph& g) {
		for(const auto& ast:asteroids) {
			for(int i=0; i<ast.num_pts; i++) {
				const auto& a=ast.loc2wld(ast.model[i]);
				const auto& b=ast.loc2wld(ast.model[(i+1)%ast.num_pts]);
				draw_line(a, b, g);
			}
		}
	}

	//show ship outline
	void renderShip(const Glyph& g) {
		vf2d a, b, c;
		ship.getOutline(a, b, c);
		draw_line(a, b, g);
		draw_line(b, c, g);
		draw_line(c, a, g);
	}

	//show particles as dots
	void renderParticles() {
		static const std::string ascii=" .:-=+*#%@";
		static const int ascii_size=ascii.length();

		for(const auto& p:particles) {
			//ramp to show how "young" or "vibrant"
			float pct=1-p.age/p.lifespan;
			int asi=cmn::clamp(int(ascii_size*pct), 0, ascii_size-1);
			draw_pixel(p.pos, {p.r, p.g, p.b, ascii[asi]});
		}
	}

	//imminent asteroids overlay
	void renderWarningSign(const vf2d& ctr) {
		fill_rect(ctr-vi2d(11, 2), {22, 5}, {0, 0, 0, ' '});
		draw_rect(ctr-vi2d(11, 2), {21, 4}, {0, 1, 1, '#'});
		draw_string(ctr-vi2d(9, 0), "ASTEROIDS IMMINENT", 0, 1, 1);
	}

	//win lose overlay
	void renderEndSign(const vi2d& ctr) {
		fill_rect(ctr-vi2d(11, 2), {21, 4}, {' ', 0, 0, 0});

		if(state==Won) {
			//green
			draw_string(ctr-vi2d(4, 1), "You Win!", 0, 1, 0);
			std::string str="Score: "+std::to_string(score);
			draw_string(ctr-vi2d(str.length()/2, -1), str, 0, 1, 0);
			draw_rect(ctr-vi2d(11, 2), {21, 4}, {0, 1, 0, '#'});
		}

		if(state==Lost) {
			//red
			draw_string(ctr-vi2d(5, 1), "GAME OVER!", 1, 0, 0);
			draw_string(ctr+vi2d(-10, 1), "You hit an asteroid.", 1, 0, 0);
			draw_rect(ctr-vi2d(11, 2), {21, 4}, {1, 0, 0, '#'});
		}
	}

	//top left & right
	void renderStats(float r, float g, float b) {
		draw_string({1, 1}, "Score: "+std::to_string(score), r, g, b);
		std::string level_str="Level: "+std::to_string(level);
		draw_string(vi2d(width-level_str.length()-1, 1), level_str, r, g, b);
	}

	//show at bottom right
	void renderAsteroidStats() {
		//yellow
		draw_string(vi2d(width-14, height-2), "Asteroid Data", .9f, .9f, 0);

		//dark yellow
		for(int i=0; i<asteroids.size(); i++) {
			const auto& a=asteroids.at(i);

			auto ix_str=std::to_string(i);
			auto x_str=std::to_string(int(a.pos.x));
			auto y_str=std::to_string(int(a.pos.y));
			auto pos_str="[x:"+x_str+", y:"+y_str+"]";
			auto num_str=std::to_string(a.num_pts);
			auto rad_str=std::to_string(int(a.base_rad));
			auto str=ix_str+": [p: "+pos_str+", n: "+num_str+", r: "+rad_str+"]";
			draw_string(vi2d(width-str.length()-1, height-i-3), str, .7f, .7f, 0);
		}
	}

	//render basic shapes into canvas[0]
	void renderGame() {
		sg_pass pass{};
		pass.attachments.colors[0]=canvas[0].color_attach;
		pass.attachments.depth_stencil=canvas[0].depth_attach;
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(sgl_pip);
		sgl_matrix_mode_projection();
		sgl_ortho(0, width, height, 0, -1, 1);

		sgl_viewport(0, 0, width, height, true);

		//background
		const vi2d res(width, height);
		fill_rect({0, 0}, res, {0, 0, 0, ' '});

		if(debug_mode) renderDebug();

		renderParticles();

		//light blue
		renderBullets({.1f, .67f, 1, 'B'});

		//grey
		renderAsteroids({.5f, .5f, .5f, 'A'});

		if(state!=Lost) renderShip({1, 1, 1, 'S'});

		//blinking overlays
		if(warning_ct%2) renderWarningSign(res/2);
		if(end_ct%2) renderEndSign(res/2);

		renderStats(0, 1, 1);

		//only when debugging
		if(debug_mode) renderAsteroidStats();

		sgl_draw();

		sg_end_pass();
	}

	//render canvas[0] into canvas[1] w/ upscaling & ascii effect
	void renderAscii() {
		sg_pass pass{};
		pass.attachments.colors[0]=canvas[1].color_attach;
		pass.attachments.depth_stencil=canvas[1].depth_attach;
		sg_begin_pass(pass);

		sg_apply_pipeline(ascii_pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=quad_vbuf;
		bind.samplers[SMP_b_ascii_smp]=smp;
		bind.views[VIEW_b_ascii_tex]=canvas[0].color_tex;
		sg_apply_bindings(bind);

		sg_apply_viewport(0, 0, 8*width, 8*height, true);

		sg_draw(0, 4, 1);

		sg_end_pass();
	}

	//render canvas[1] onto screen w/ crt effect
	void renderCRT() {
		sg_pass pass{};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sg_apply_pipeline(crt_pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=quad_vbuf;
		bind.samplers[SMP_b_crt_smp]=smp;
		bind.views[VIEW_b_crt_tex]=canvas[1].color_tex;
		sg_apply_bindings(bind);

		p_fs_crt_t p_fs_crt{};
		p_fs_crt.u_time=total_dt;
		sg_apply_uniforms(UB_p_fs_crt, SG_RANGE(p_fs_crt));

		sg_apply_viewport(0, 0, sapp_width(), sapp_height(), true);

		sg_draw(0, 4, 1);

		sg_end_pass();
	}
#pragma endregion

	bool user_render() override {
		renderGame();

		renderAscii();

		renderCRT();

		sg_commit();

		return true;
	}
};