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

#include <cstdint>
//for time
#include <ctime>

#include "common/sokol/font.h"

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"

//iterable queue
#include <deque>

typedef std::uint8_t gpio_t;

/*
 -a-    -0-
f   b  5   1
 -g-    -6-
e   c  4   2
 -d-    -3-
*/
struct Module {
	gpio_t gpio=0b00000000;
	bool seg[7]{0, 0, 0, 0, 0, 0, 0};
};

void getTime(int* hour, int* min) {
	auto now=std::time(0);
	auto local=std::localtime(&now);

	*hour=local->tm_hour;
	*min=local->tm_min;
}

//low level formatting
void formatTime(int hour, int minute, int* hh, int* hl, int* mh, int* ml) {
	//24 hour time -> 12 hour time
	if(hour>12) hour-=12;

	//extract place values
	*hh=hour/10;
	*hl=hour%10;
	*mh=minute/10;
	*ml=minute%10;
}

class ClockSim : public cmn::SokolEngine {
	//graphics
	sgl_pipeline pip{};

	sg_sampler sampler{};

	sg_view segments[7];

	cmn::Font font;

	bool render_tasks=false;

	//sound
	struct {
		ma_engine engine;

		ma_sound clicks[9];
		int num_clicks=sizeof(clicks)/sizeof(clicks[0]);
	} sound;

	//simulation
	struct {
		int w_mod_img;
		int h_mod_img;

		int num_x=0;
		int num_y=0;
		int num_ttl=0;
		Module* array=nullptr;

		bool render_gpio=false;

		int ix(int i, int j) const {
			return i+num_x*j;
		}

		int hour_high, hour_low;
		int minute_high, minute_low;
	} clock;

	struct Job {
		int mod=0;
		gpio_t gpio=0;
		int delay_ms=0;
	};
	std::deque<Job> tasks;
	float job_timer_ms=0;

public:
#pragma region SETUP HELPERS
	void setupSGL() {
		sgl_desc_t desc{};
		sgl_setup(desc);
	}

	//alpha blending
	void setupPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.colors[0].blend.enabled=true;
		pip_desc.colors[0].blend.src_factor_rgb=SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha=SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha=SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip=sgl_make_pipeline(pip_desc);
	}

	void setupSampler() {
		sg_sampler_desc desc{};
		sampler=sg_make_sampler(desc);
	}

	bool setupTextures() {
		for(int i=0; i<7; i++) {
			auto& t=segments[i];
			auto name="assets/img/seg"+std::to_string(i)+".png";
			bool status=cmn::makeTextureFromFile(t, name);
			if(!status) return false;
		}

		return true;
	}

	void setupFont() {
		font=cmn::Font("assets/img/fancy_8x16.png", 8, 16);
	}

	bool setupSounds() {
		//setup engine
		auto status=ma_engine_init(NULL, &sound.engine);
		if(status!=MA_SUCCESS) return false;

		//setup click sounds
		for(int i=0; i<sound.num_clicks; i++) {
			std::string file="assets/sfx/click"+std::to_string(i)+".wav";
			if(ma_sound_init_from_file(
				&sound.engine,
				file.c_str(),
				MA_SOUND_FLAG_DECODE,
				NULL,
				NULL,
				&sound.clicks[i]
			)!=MA_SUCCESS) return false;
		}

		return true;
	}

	void makeClickNoise() {
		//choose random click
		int ix=std::rand()%sound.num_clicks;
		auto& click=sound.clicks[ix];

		//restart & play
		ma_sound_seek_to_pcm_frame(&click, 0);
		ma_sound_start(&click);
	}

	//extended 0
	void queueLoop(int si, int sj, int ei, int ej, int spd) {
		//turn on, then off
		for(int f=1; f>=0; f--) {
			int mi=si, mj=sj;
			for(int s=0; s<6; s++) {
				//flip seg
				gpio_t seg_on=(1<<7)|(~(1<<s));
				int m=clock.ix(mi, mj);
				tasks.push_back({m, gpio_t(f?seg_on:~seg_on), 20});
				tasks.push_back({m, 0b00000000, spd});

				//module change & segment halt logic
				switch(s) {
					case 0: if(mi!=ei) mi++, s--; break;//top
					case 2: if(mj!=ej) mj++, s-=2; break;//right
					case 3: if(mi!=si) mi--, s--; break;//bottom
					case 5: if(mj!=sj)mj--, s-=2;  break;//left
				}
			}
		}
	}

	void queueDigit(int m, int d, bool on) {
		//segment writing direction
		//-1 is the stop flag
		const int digits[16][7]={
			{00, 05, 04, 03, 02, 01, -1},//0
			{01, 02, -1, -1, -1, -1, -1},//1
			{00, 01, 06, 04, 03, -1, -1},//2
			{00, 01, 06, 02, 03, -1, -1},//3
			{05, 06, 01, 02, -1, -1, -1},//4
			{00, 05, 06, 02, 03, -1, -1},//5
			{00, 05, 04, 03, 02, 06, -1},//6
			{00, 01, 02, -1, -1, -1, -1},//7
			{00, 05, 06, 01, 04, 03, 02},//8?
			{06, 05, 00, 01, 02, 03, -1},//9
			{04, 05, 00, 01, 02, 06, -1},//A
			{05, 04, 03, 02, 06, -1, -1},//b
			{06, 04, 03, -1, -1, -1, -1},//C
			{06, 04, 03, 02, 01, -1, -1},//d
			{06, 05, 00, 01, 02, -1, -1},//E
			{06, 05, 00, 01, 02, -1, -1}//F
		};
		for(int i=0; i<7; i++) {
			int s=digits[d][i];
			if(s==-1) break;

			gpio_t seg_on=(1<<7)|(~(1<<s));
			tasks.push_back({m, gpio_t(on?seg_on:~seg_on), 20});
			tasks.push_back({m, 0b00000000, 60});
		}
	}

	void setupClock() {
		auto img=sg_query_view_image(segments[0]);
		clock.w_mod_img=sg_query_image_width(img);
		clock.h_mod_img=sg_query_image_height(img);

		clock.num_x=4;
		clock.num_y=1;
		clock.num_ttl=clock.num_x*clock.num_y;
		clock.array=new Module[clock.num_ttl];

		//do some fun loops w/ slight delays
		queueLoop(0, 0, 1, 0, 60);
		tasks.push_back({-1, 0, 150});
		queueLoop(2, 0, 3, 0, 60);
		tasks.push_back({-1, 0, 150});
		queueLoop(0, 0, 3, 0, 50);
		tasks.push_back({-1, 0, 500});

		//"setup time"
		int hour, minute;
		getTime(&hour, &minute);
		formatTime(
			hour, minute,
			&clock.hour_high, &clock.hour_low,
			&clock.minute_high, &clock.minute_low
		);
		if(clock.hour_high) queueDigit(0, clock.hour_high, true);
		queueDigit(1, clock.hour_low, true);
		queueDigit(2, clock.minute_high, true);
		queueDigit(3, clock.minute_low, true);
	}
#pragma endregion

	bool user_create() override {
		std::srand(std::time(0));

		setupSGL();

		setupPipeline();

		setupSampler();

		if(!setupTextures()) return false;

		setupFont();

		if(!setupSounds()) return false;

		setupClock();

		return true;
	}

	void user_destroy() override {
		for(auto& c:sound.clicks) {
			ma_sound_uninit(&c);
		}

		ma_engine_uninit(&sound.engine);

		sgl_shutdown();
	}

	void handleClock() {
		//get time
		int hour, minute;
		getTime(&hour, &minute);

		//format time
		int hh=0, hl=0, mh=0, ml=0;
		formatTime(hour, minute, &hh, &hl, &mh, &ml);

		//set if changed
		bool set_hh=hh!=clock.hour_high;
		bool set_hl=hl!=clock.hour_low;
		bool set_mh=mh!=clock.minute_high;
		bool set_ml=ml!=clock.minute_low;

		//clear old backwards
		if(set_ml) queueDigit(3, clock.minute_low, false);
		if(set_mh) queueDigit(2, clock.minute_high, false);
		if(set_hl) queueDigit(1, clock.hour_low, false);
		if(set_hh&&clock.hour_high) queueDigit(0, clock.hour_high, false);

		//set new
		clock.hour_high=hh;
		clock.hour_low=hl;
		clock.minute_high=mh;
		clock.minute_low=ml;

		//write new
		if(set_hh&&clock.hour_high) queueDigit(0, clock.hour_high, true);
		if(set_hl) queueDigit(1, clock.hour_low, true);
		if(set_mh) queueDigit(2, clock.minute_high, true);
		if(set_ml) queueDigit(3, clock.minute_low, true);
	}

	void handleTasks(float dt) {
		//could add time multiplier
		job_timer_ms-=1000*dt;

		if(job_timer_ms<0) {
			if(tasks.size()) {
				//pop job off queue
				auto job=tasks.front();
				tasks.pop_front();

				//do it
				if(job.mod!=-1) {
					clock.array[job.mod].gpio=job.gpio;
				}

				//update timer
				job_timer_ms+=job.delay_ms;
			} else job_timer_ms=0;
		}
	}

	void updateSegs() {
		//update segs based on gpio state
		for(int i=0; i<clock.num_ttl; i++) {
			auto& mod=clock.array[i];
			//is the mod high/low
			bool gm=1&(mod.gpio>>7);
			for(int j=0; j<7; j++) {
				//is the seg high/low
				bool gs=1&(mod.gpio>>j);

				//will current flow?
				if(gm==gs) continue;

				//click if state flipped
				bool& s=mod.seg[j];
				if(s!=gm) {
					s=gm;
					makeClickNoise();
				}
			}
		}
	}

	bool user_update(float dt) override {
		//toggle gpio render
		if(GetKey(SAPP_KEYCODE_G).pressed) clock.render_gpio^=true;

		//toggle task render
		if(GetKey(SAPP_KEYCODE_T).pressed) render_tasks^=true;

		handleClock();

		handleTasks(dt);

		updateSegs();

		return true;
	}

#pragma region RENDER HELPERS
	void renderTex(
		float x, float y, float w, float h,
		const sg_view& tex,
		sg_color col={1, 1, 1, 1},
		float l=0, float t=0, float r=1, float b=1
	) {
		sgl_enable_texture();
		sgl_texture(tex, sampler);

		sgl_begin_quads();

		sgl_c4f(col.r, col.g, col.b, col.a);

		sgl_v2f_t2f(x, y, l, t);
		sgl_v2f_t2f(x+w, y, r, t);
		sgl_v2f_t2f(x+w, y+h, r, b);
		sgl_v2f_t2f(x, y+h, l, b);

		sgl_end();

		sgl_disable_texture();
	}

	void renderString(
		float sx, float sy,
		const std::string& str, float scl,
		sg_color col={1, 1, 1, 1}
	) {
		float w=scl*font.char_w;
		float h=scl*font.char_h;

		int ox=0, oy=0;
		for(const auto& c:str) {
			if(c==' ') ox++;
			//tabsize=2
			else if(c=='\t') ox+=2;
			else if(c=='\n') ox=0, oy++;
			else if(c>='!'&&c<='~') {
				float l, t, r, b;
				font.getRegion(c, l, t, r, b);

				float x=sx+w*ox;
				float y=sy+h*oy;
				renderTex(
					x, y, w, h,
					font.tex,
					col,
					l, t, r, b
				);

				ox++;
			}
		}
	}

	void renderModule(const Module& m, float x, float y, float scl) {
		float w_mod=scl*clock.w_mod_img;
		float h_mod=scl*clock.h_mod_img;

		//black background
		sgl_begin_quads();
		sgl_c3f(0, 0, 0);
		sgl_v2f(x, y);
		sgl_v2f(x+w_mod, y);
		sgl_v2f(x+w_mod, y+h_mod);
		sgl_v2f(x, y+h_mod);
		sgl_end();

		//segments
		for(int i=0; i<7; i++) {
			//skip if off
			if(!m.seg[i]) continue;

			renderTex(
				x, y, w_mod, h_mod,
				segments[i],
				{1, 1, 1, 1},
				0, 0, 1, 1
			);
		}

		if(clock.render_gpio) {
			float w_char=w_mod/8;
			float scl_char=w_char/font.char_w;
			float sx_char=x+w_mod-w_char;
			float y_char=y+h_mod-scl_char*font.char_h;

			//gpio
			const sg_color red{1, 0, 0, 1}, green{0, 1, 0, 1};
			for(int i=0; i<8; i++) {
				//right to left
				float x_char=sx_char-i*w_char;
				char c=i==7?'M':('0'+i);
				std::string str(1, c);
				auto col=(1&(m.gpio>>i))?green:red;
				renderString(x_char, y_char, str, scl_char, col);
			}
		}
	}

	void renderClock() {
		//screen box to fit into
		//inset by margin
		int m_scr=15;
		int w_scr=sapp_width()-2*m_scr;
		int h_scr=sapp_height()-2*m_scr;

		//margin between clock
		int m_mod=8;
		int w_mod_imgs=clock.num_x*clock.w_mod_img;
		int h_mod_imgs=clock.num_y*clock.h_mod_img;
		float scl_x=float(w_scr-(clock.num_x-1)*m_mod)/w_mod_imgs;
		float scl_y=float(h_scr-(clock.num_y-1)*m_mod)/h_mod_imgs;

		//constraining dimension
		float scl=scl_x<scl_y?scl_x:scl_y;

		float w_mod=scl*clock.w_mod_img;
		float h_mod=scl*clock.h_mod_img;
		float w_clock=clock.num_x*w_mod+(clock.num_x-1)*m_mod;
		float h_clock=clock.num_y*h_mod+(clock.num_y-1)*m_mod;

		float sx=.5f*(sapp_widthf()-w_clock);
		float sy=.5f*(sapp_heightf()-h_clock);
		for(int i=0; i<clock.num_x; i++) {
			for(int j=0; j<clock.num_y; j++) {
				float x=sx+i*(w_mod+m_mod);
				float y=sy+j*(h_mod+m_mod);
				renderModule(clock.array[clock.ix(i, j)], x, y, scl);
			}
		}
	}

	void renderTasks() {
		float y=0;
		for(const auto& t:tasks) {
			auto m=std::to_string(t.mod);
			std::string g="0b";
			for(int i=7; i>=0; i--) {
				g+=(1&(t.gpio>>i))?'0':'1';
			}
			auto d=std::to_string(t.delay_ms);
			auto str="[m: "+m+", g: "+g+", d: "+d+"]";
			renderString(0, y, str, 1, {1, 1, 0, 1});

			y+=font.char_h;
		}
	}
#pragma endregion

	bool user_render() override {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={.5f, .5f, .5f, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_projection();
		sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);

		renderClock();

		if(render_tasks) renderTasks();

		sgl_draw();

		sg_end_pass();

		sg_commit();

		return true;
	}

	ClockSim() {
		app_title="7-Segment Clock Simulation";
	}
};