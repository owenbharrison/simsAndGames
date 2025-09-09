#pragma once
#ifndef VEHICLE_STRUCT_H
#define VEHICLE_STRUCT_H

#include <cmath>

struct Vehicle {
	static float max_speed, max_force;
	
	vf2d pos, vel, acc;
	vf2d target;

	olc::Pixel col=olc::WHITE;

	Vehicle() {}

	Vehicle(const vf2d& p, const vf2d& t, const olc::Pixel& c) {
		pos=p;
		target=t;
		col=c;
	}

	void accelerate(const vf2d& a) {
		acc+=a;
	}

	void update(float dt) {
		vel+=acc*dt;
		pos+=vel*dt;

		//clear "forces"
		acc*=0;
	}

	//behavior forces
	vf2d getArrive(const vf2d& t) const {
		//where do i want to be?
		vf2d des=t-pos;
		float des_mag=des.mag();

		//if im there, forget it.
		if(des_mag==0) return {0, 0};
		
		//go as fast as possible
		float speed=max_speed;

		//unless close enough
		const float slow_rad=750;
		if(des_mag<slow_rad) speed=max_speed*des_mag/slow_rad;
		
		//set desired mag to speed
		des*=speed/des_mag;
		
		//limit force applied
		vf2d steer=des-vel;
		float steer_mag=steer.mag();
		if(steer_mag>max_force) steer*=max_force/steer_mag;

		return steer;
	}

	vf2d getFlee(const vf2d& t) const {
		//where DONT i want to be?
		vf2d des=pos-t;
		float des_mag=des.mag();

		//if im there, uhh...
		if(des_mag==0) return {0, 0};

		//opposite
		des*=max_speed/des_mag;

		//limit force applied
		vf2d steer=des-vel;
		float steer_mag=steer.mag();
		if(steer_mag>max_force) steer*=max_force/steer_mag;

		return steer;
	}
};

float Vehicle::max_speed=3500;
float Vehicle::max_force=230;
#endif