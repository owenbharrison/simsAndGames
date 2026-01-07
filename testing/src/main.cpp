#include "terrain_demo.h"

static TerrainDemo* td_ptr=nullptr;

static void init_cb() { td_ptr->init(); }
static void cleanup_cb() { td_ptr->cleanup(); }
static void frame_cb() { td_ptr->frame(); }
static void input_cb(const sapp_event* e) { td_ptr->input(e); }

sapp_desc sokol_main(int argc, char* argv[]) {
	static TerrainDemo td;
	td_ptr=&td;

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