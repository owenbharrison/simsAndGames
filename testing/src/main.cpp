#include "post_process_ui.h"

static PostProcessUI* ppui_ptr=nullptr;

static void init_cb() { ppui_ptr->init(); }
static void cleanup_cb() { ppui_ptr->cleanup(); }
static void frame_cb() { ppui_ptr->frame(); }
static void input_cb(const sapp_event* e) { ppui_ptr->input(e); }

sapp_desc sokol_main(int argc, char* argv[]) {
	static PostProcessUI ppui;
	ppui_ptr=&ppui;

	sapp_desc app_desc{};
	app_desc.init_cb=init_cb;
	app_desc.cleanup_cb=cleanup_cb;
	app_desc.frame_cb=frame_cb;
	app_desc.event_cb=input_cb;
	app_desc.width=640;
	app_desc.height=480;
	app_desc.icon.sokol_default=true;

	return app_desc;
}