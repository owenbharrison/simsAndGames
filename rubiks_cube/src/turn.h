#pragma once
#ifndef TURN_STRUCT_H
#define TURN_STRUCT_H

struct Turn {
	enum : int {
		XAxis=0, YAxis=1, ZAxis=2
	};
	int axis=XAxis;

	//+: 1-based indexing from start
	//-: 1-based indexing from end
	//0: rotate all
	int slice=1;

	bool ccw=true;

	static const Turn
		F, f, U, u, L, l,
		B, b, D, d, R, r,
		X, x, Y, y, Z, z;
};

//looking at +z face:
//upper=CW, lower=CCW
const Turn
	Turn::R={Turn::XAxis, -1, 0}, Turn::r={Turn::XAxis, -1, 1},
	Turn::U={Turn::YAxis, -1, 0}, Turn::u={Turn::YAxis, -1, 1},
	Turn::F={Turn::ZAxis, -1, 0}, Turn::f={Turn::ZAxis, -1, 1},
	Turn::L={Turn::XAxis, +1, 1}, Turn::l={Turn::XAxis, +1, 0},
	Turn::D={Turn::YAxis, +1, 1}, Turn::d={Turn::YAxis, +1, 0},
	Turn::B={Turn::ZAxis, +1, 1}, Turn::b={Turn::ZAxis, +1, 0},
	Turn::X={Turn::XAxis, 0, 0}, Turn::x={Turn::XAxis, 0, 1},
	Turn::Y={Turn::YAxis, 0, 0}, Turn::y={Turn::YAxis, 0, 1},
	Turn::Z={Turn::ZAxis, 0, 0}, Turn::z={Turn::ZAxis, 0, 1};
#endif