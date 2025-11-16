#pragma once
#ifndef SOKOL_ENGINE_CLASS_H
#define SOKOL_ENGINE_CLASS_H

#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"

//for memset, memcpy
#include <string>

//generalized struct initializer
template<typename T>
void zeroMem(T& t) {
	std::memset(&t, 0, sizeof(T));
}

class SokolEngine {
	static const int _num_keys=512;
	bool _keys_old[_num_keys],
		_keys_curr[_num_keys],
		_keys_new[_num_keys];
	bool _mouse_moving_new=false;
	bool _mouse_moving=false;

	float _mouse_x_new=0;
	float _mouse_y_new=0;

	float _mouse_dx_new=0;
	float _mouse_dy_new=0;

protected:
	float mouse_x=0;
	float mouse_y=0;

	float mouse_dx=0;
	float mouse_dy=0;

public:
	virtual void userCreate()=0;

	virtual void userDestroy() {};

	virtual void userUpdate(float dt)=0;

	virtual void userRender()=0;

	void init() {
		sg_desc desc; zeroMem(desc);
		desc.environment=sglue_environment();
		sg_setup(desc);

		std::memset(_keys_old, false, sizeof(bool)*_num_keys);
		std::memset(_keys_curr, false, sizeof(bool)*_num_keys);
		std::memset(_keys_new, false, sizeof(bool)*_num_keys);

		userCreate();
	}

	void cleanup() {
		userDestroy();

		sg_shutdown();
	}

	//dont immediately send new key states.
	void input(const sapp_event* e) {
		switch(e->type) {
			case SAPP_EVENTTYPE_KEY_DOWN:
				_keys_new[e->key_code]=true;
				break;
			case SAPP_EVENTTYPE_KEY_UP:
				_keys_new[e->key_code]=false;
				break;
			case SAPP_EVENTTYPE_MOUSE_MOVE:
				_mouse_moving=true;
				//apparently these are invalid?
				if(sapp_mouse_locked()) {
					_mouse_x_new=e->mouse_x;
					_mouse_y_new=e->mouse_y;
				}
				//always valid...
				_mouse_dx_new=e->mouse_dx;
				_mouse_dy_new=e->mouse_dy;
				break;
		}
	}

	//fun little helper.
	struct KeyState { bool pressed, held, released; };
	KeyState getKey(const sapp_keycode& kc) const {
		bool held=_keys_curr[kc];
		bool pressed=held&&!_keys_old[kc];
		return {pressed, held, !pressed};
	}

	void frame() {
		//update curr key values
		std::memcpy(_keys_curr, _keys_new, sizeof(bool)*_num_keys);

		//hmm...
		if(_mouse_moving) {
			mouse_dx=_mouse_dx_new;
			mouse_dy=_mouse_dy_new;
		} else {
			mouse_dx=0;
			mouse_dy=0;
		}

		const float dt=sapp_frame_duration();

		userUpdate(dt);

		userRender();

		_mouse_moving=false;

		//update prev key values
		std::memcpy(_keys_old, _keys_curr, sizeof(bool)*_num_keys);
	}
};
#endif