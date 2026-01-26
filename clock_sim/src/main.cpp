#include "clock_ui.h"

static ClockUI* cui_ptr=nullptr;

static void init_cb() { cui_ptr->init(); }
static void cleanup_cb() { cui_ptr->cleanup(); }
static void frame_cb() { cui_ptr->frame(); }
static void input_cb(const sapp_event* e) { cui_ptr->input(e); }

sapp_desc sokol_main(int argc, char* argv[]) {
	static ClockUI cui;
	cui_ptr=&cui;

	sapp_desc app_desc{};
	app_desc.init_cb=init_cb;
	app_desc.cleanup_cb=cleanup_cb;
	app_desc.frame_cb=frame_cb;
	app_desc.event_cb=input_cb;
	app_desc.width=640;
	app_desc.height=270;
	app_desc.icon.sokol_default=true;

	return app_desc;
}