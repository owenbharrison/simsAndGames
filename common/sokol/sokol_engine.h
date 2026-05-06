#pragma once
#ifndef CMN_SOKOL_ENGINE_CLASS_H
#define CMN_SOKOL_ENGINE_CLASS_H

//for memset & memcpy
#include <string>

//for snprintf
#include <cstdio>

namespace cmn {
	class SokolEngine {
		static const int _num_keys=512;
		bool _keys_prev[_num_keys],
			_keys_curr[_num_keys],
			_keys_next[_num_keys];

		static const int _num_mouse_buttons=8;
		bool _mouse_buttons_prev[_num_mouse_buttons],
			_mouse_buttons_curr[_num_mouse_buttons],
			_mouse_buttons_next[_num_mouse_buttons];

		float _mouse_x_next=0, _mouse_y_next=0;
		float _mouse_x_curr=0, _mouse_y_curr=0;

	public:
		virtual bool user_create()=0;
		virtual bool user_update(float dt)=0;
		virtual bool user_render()=0;
		virtual void user_input(const sapp_event*) {};
		virtual void user_destroy() {};

		std::string app_title="[untitled]";

		void init() {
			sg_desc desc{};
			desc.environment=sglue_environment();
			sg_setup(desc);

			std::memset(_keys_prev, false, sizeof(bool)*_num_keys);
			std::memset(_keys_curr, false, sizeof(bool)*_num_keys);
			std::memset(_keys_next, false, sizeof(bool)*_num_keys);

			std::memset(_mouse_buttons_prev, false, sizeof(bool)*_num_mouse_buttons);
			std::memset(_mouse_buttons_curr, false, sizeof(bool)*_num_mouse_buttons);
			std::memset(_mouse_buttons_next, false, sizeof(bool)*_num_mouse_buttons);

			if(!user_create()) sapp_request_quit();
		}

		void input(const sapp_event* e) {
			switch(e->type) {
				case SAPP_EVENTTYPE_KEY_DOWN:
					_keys_next[e->key_code]=true;
					break;
				case SAPP_EVENTTYPE_KEY_UP:
					_keys_next[e->key_code]=false;
					break;
				case SAPP_EVENTTYPE_MOUSE_DOWN:
					_mouse_buttons_next[e->mouse_button]=true;
					break;
				case SAPP_EVENTTYPE_MOUSE_UP:
					_mouse_buttons_next[e->mouse_button]=false;
					break;
				case SAPP_EVENTTYPE_MOUSE_MOVE: {
					_mouse_x_next=e->mouse_x;
					_mouse_y_next=e->mouse_y;
					break;
				}
				default: break;
			}

			user_input(e);
		}

		void frame() {
			//update keys
			std::memcpy(_keys_prev, _keys_curr, sizeof(_keys_curr));
			std::memcpy(_keys_curr, _keys_next, sizeof(_keys_curr));

			//update mouse buttons
			std::memcpy(_mouse_buttons_prev, _mouse_buttons_curr, sizeof(_mouse_buttons_curr));
			std::memcpy(_mouse_buttons_curr, _mouse_buttons_next, sizeof(_mouse_buttons_curr));

			//update mouse pos
			_mouse_x_curr=_mouse_x_next;
			_mouse_y_curr=_mouse_y_next;

			float dt=sapp_frame_duration();

			//update title with fps
			{
				char buf[256];
				std::snprintf(
					buf,
					sizeof(buf),
					"%s - FPS: %d",
					app_title.c_str(),
					int(1/dt)
				);
				sapp_set_window_title(buf);
			}

			if(!user_update(dt)) sapp_request_quit();

			if(!user_render()) sapp_request_quit();
		}

		void cleanup() {
			user_destroy();

			sg_shutdown();
		}

		struct ButtonState { bool pressed, held, released; };

		ButtonState GetKey(const sapp_keycode& k) const {
			bool curr=_keys_curr[k], prev=_keys_prev[k];
			return {curr&&!prev, curr, !curr&&prev};
		}

		ButtonState GetMouse(const sapp_mousebutton& m) const {
			bool curr=_mouse_buttons_curr[m], prev=_mouse_buttons_prev[m];
			return {curr&&!prev, curr, !curr&&prev};
		}

		float GetMouseX() const { return _mouse_x_curr; }
		float GetMouseY() const { return _mouse_y_curr; }
	};
}

//convenience macro
#define CMN_SOKOL_ENGINE_LAUNCH(AppClass, init_w, init_h)\
static AppClass* app_ptr=nullptr;\
static void init_cb() { app_ptr->init(); }\
static void frame_cb() { app_ptr->frame(); }\
static void input_cb(const sapp_event* e) { app_ptr->input(e); }\
static void cleanup_cb() { app_ptr->cleanup(); }\
sapp_desc sokol_main(int argc, char* argv[]) {\
	static AppClass app;\
	app_ptr=&app;\
	sapp_desc app_desc{};\
	app_desc.init_cb=init_cb;\
	app_desc.frame_cb=frame_cb;\
	app_desc.event_cb=input_cb;\
	app_desc.cleanup_cb=cleanup_cb;\
	app_desc.width=init_w;\
	app_desc.height=init_h;\
	app_desc.icon.sokol_default=true;\
	return app_desc;\
}
#endif