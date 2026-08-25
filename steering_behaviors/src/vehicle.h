#pragma once
#ifndef VEHICLE_STRUCT_H
#define VEHICLE_STRUCT_H

#include "cmn/math/v2d.h"

struct Vehicle {
	static float max_accel;
	static float max_speed;

	cmn::vf2d pos, vel, acc;

	cmn::vf2d target;

	float rgb[3]{1, 1, 1};

	void accelerate(const cmn::vf2d& a) {
		acc+=a;
	}

	void update(float dt) {
		float accel=length(acc);
		if(accel>max_accel) acc*=max_accel/accel;
		
		vel+=acc*dt;

		float speed=length(vel);
		if(speed>max_speed) vel*=max_speed/speed;

		pos+=vel*dt;
		
		acc*=0;
	}

	cmn::vf2d getSeek(const cmn::vf2d& tgt) const {
		cmn::vf2d to_tgt=tgt-pos;
		float dist=length(to_tgt);
		cmn::vf2d des;
		if(dist>1e-6f) des=max_speed/dist*to_tgt;
		return des-vel;
	}

	cmn::vf2d getFlee(const cmn::vf2d& tgt) const {
		return -getSeek(tgt);
	}

	cmn::vf2d getArrive(const cmn::vf2d& tgt) const {
		cmn::vf2d to_tgt=tgt-pos;
		float dist=length(to_tgt);
		cmn::vf2d des;
		if(dist>1e-6f) {
			//slowing radius: v^2=2as -> s=v^2/2/a
			float rad=max_speed*max_speed/2/max_accel;
			float des_speed=max_speed*(dist<rad?dist/rad:1);
			des=des_speed/dist*to_tgt;
		}
		return des-vel;
	}
};

float Vehicle::max_accel=50;
float Vehicle::max_speed=15;
#endif