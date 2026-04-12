#include "splitter_ui.h"

static SplitterUI* sui_ptr=nullptr;

static void init_cb() { sui_ptr->init(); }
static void frame_cb() { sui_ptr->frame(); }
static void input_cb(const sapp_event* e) { sui_ptr->input(e); }
static void cleanup_cb() { sui_ptr->cleanup(); }

sapp_desc sokol_main(int argc, char* argv[]) {
	static SplitterUI sui;
	sui_ptr=&sui;

	sapp_desc app_desc{};
	app_desc.init_cb=init_cb;
	app_desc.frame_cb=frame_cb;
	app_desc.event_cb=input_cb;
	app_desc.cleanup_cb=cleanup_cb;
	app_desc.width=680;
	app_desc.height=360;
	app_desc.icon.sokol_default=true;

	return app_desc;
}