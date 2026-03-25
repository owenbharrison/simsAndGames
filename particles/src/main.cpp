#include "particles_ui.h"

static ParticlesUI* pui_ptr=nullptr;

static void init_cb() { pui_ptr->init(); }
static void frame_cb() { pui_ptr->frame(); }
static void input_cb(const sapp_event* e) { pui_ptr->input(e); }
static void cleanup_cb() { pui_ptr->cleanup(); }

sapp_desc sokol_main(int argc, char* argv[]) {
	static ParticlesUI pui;
	pui_ptr=&pui;

	sapp_desc app_desc{};
	app_desc.init_cb=init_cb;
	app_desc.frame_cb=frame_cb;
	app_desc.event_cb=input_cb;
	app_desc.cleanup_cb=cleanup_cb;
	app_desc.width=720;
	app_desc.height=400;
	app_desc.icon.sokol_default=true;

	return app_desc;
}