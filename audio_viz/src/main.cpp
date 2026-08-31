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

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/include/miniaudio.h"

#include <cmath>

static ma_device audio_device;

static const int ring_bfr_sz=4096;
static ma_pcm_rb ring_bfr;

static const int disp_bfr_sz=50000;
static float disp_bfr[disp_bfr_sz];

static const int disp_bfr_ext_sz=1024;
static float disp_bfr_ext[disp_bfr_ext_sz];

void audio_callback(ma_device* device, void* out, const void* in, ma_uint32 frame_ct) {
	if(in==nullptr) return;

	void* p_buffer_out;
	ma_uint32 frames_to_write=frame_ct;
	if(ma_pcm_rb_acquire_write(&ring_bfr, &frames_to_write, &p_buffer_out)==MA_SUCCESS) {
		ma_copy_pcm_frames(p_buffer_out, in, frames_to_write, ma_format_f32, 1);

		ma_pcm_rb_commit_write(&ring_bfr, frames_to_write);
	}
}

void init() {
	sg_desc sg_desc{};
	sg_desc.environment=sglue_environment();
	sg_setup(sg_desc);

	sgl_desc_t sgl_desc{};
	sgl_desc.max_vertices=1000000;
	sgl_desc.max_commands=1000000;
	sgl_setup(sgl_desc);

	ma_pcm_rb_init(ma_format_f32, 1, ring_bfr_sz, NULL, NULL, &ring_bfr);

	ma_device_config dev_config=ma_device_config_init(ma_device_type_loopback);
	dev_config.capture.format=ma_format_f32;
	dev_config.capture.channels=1;
	dev_config.sampleRate=44100;
	dev_config.dataCallback=audio_callback;

	if(ma_device_init(NULL, &dev_config, &audio_device)==MA_SUCCESS) {
		ma_device_start(&audio_device);
	}
}

//sgl_begin_lines
void queue_line(float ax, float ay, float bx, float by) {
	sgl_v2f(ax, ay);
	sgl_v2f(bx, by);
}

//use w/ sgl_begin_quads
void queue_thick_line(float ax, float ay, float bx, float by, float t) {
	//axis
	float abx=bx-ax, aby=by-ay;
	float ab_l=std::sqrt(abx*abx+aby*aby);

	//unit vectors
	float lx=abx/ab_l, ly=aby/ab_l;
	float wx=-ly, wy=lx;

	//deltas
	float dx=t/2*wx, dy=t/2*wy;

	sgl_v2f(ax-dx, ay-dy);
	sgl_v2f(bx-dx, by-dy);
	sgl_v2f(bx+dx, by+dy);
	sgl_v2f(ax+dx, ay+dy);
}

void frame() {
	//get audio
	void* p_buffer_in;
	ma_uint32 ext_frames=disp_bfr_ext_sz;
	if(ma_pcm_rb_acquire_read(&ring_bfr, &ext_frames, &p_buffer_in)==MA_SUCCESS) {
		if(ext_frames>0) {
			//fill ring buffer
			ma_copy_pcm_frames(disp_bfr_ext, p_buffer_in, ext_frames, ma_format_f32, 1);
			ma_pcm_rb_commit_read(&ring_bfr, ext_frames);

			//fill display buffer
			//shift left
			int diff=disp_bfr_sz-ext_frames;
			for(int i=0; i<diff; i++) {
				disp_bfr[i]=disp_bfr[ext_frames+i];
			}
			//append right
			for(int i=0; i<ext_frames; i++) {
				disp_bfr[diff+i]=disp_bfr_ext[i];
			}
		}
	}

	sg_pass pass{};
	pass.swapchain=sglue_swapchain();
	sg_begin_pass(&pass);

	const float w_scr=sapp_widthf();
	const float h_scr=sapp_heightf();

	sgl_defaults();
	sgl_matrix_mode_projection();
	sgl_ortho(0, w_scr, h_scr, 0, -1, 1);

	//draw background and vertical grid
	{
		const float bg=.2f;
		sgl_begin_quads();
		sgl_c3f(bg, bg, bg);
		sgl_v2f(0, 0);
		sgl_v2f(w_scr, 0);
		sgl_v2f(w_scr, h_scr);
		sgl_v2f(0, h_scr);
		sgl_end();

		sgl_begin_lines();
		const int num=7;
		for(int i=0; i<=num; i++) {
			float t=float(i)/num;
			float shade=1+(bg-1)*t;
			sgl_c3f(shade, shade, shade);
			float yt=h_scr*(.5f+.5f*t);
			sgl_v2f(0, yt), sgl_v2f(w_scr, yt);
			float yb=h_scr*(.5f-.5f*t);
			sgl_v2f(0, yb), sgl_v2f(w_scr, yb);
		}
		sgl_end();
	}

	//draw waveform
	{
		const float recip=1/(disp_bfr_sz-1.f);

		float px, py;
		for(int t=0; t<2; t++) {
			if(t==0) sgl_begin_quads();
			else sgl_begin_lines();
			for(int i=0; i<disp_bfr_sz; i++) {
				float x=w_scr*i*recip;
				float y=h_scr*(.5f-.5f*disp_bfr[i]);
				if(i>0) {
					if(t==0) {
						sgl_c3f(0, 1, 1);
						queue_thick_line(px, py, x, y, 5);
					} else {
						sgl_c3f(0, 0, 1);
						queue_line(px, py, x, y);
					}
				}
				px=x, py=y;
			}
			sgl_end();
		}
	}

	//draw corner markers
	{
		const float m=10, s=20, t=2;

		sgl_begin_quads();
		sgl_c3f(1, 0, 0);
		queue_thick_line(m, m, m+s, m, t);
		queue_thick_line(m, m, m, m+s, t);
		sgl_c3f(0, 1, 0);
		queue_thick_line(w_scr-m, h_scr-m, w_scr-m-s, h_scr-m, t);
		queue_thick_line(w_scr-m, h_scr-m, w_scr-m, h_scr-m-s, t);
		sgl_end();
	}

	sgl_draw();

	sg_end_pass();

	sg_commit();
}

void cleanup() {
	ma_device_uninit(&audio_device);
	ma_pcm_rb_uninit(&ring_bfr);
	sgl_shutdown();
	sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
	sapp_desc app={};
	app.init_cb=init;
	app.frame_cb=frame;
	app.cleanup_cb=cleanup;
	app.width=480;
	app.height=360;
	app.window_title="[audio_viz]";

	return app;
}