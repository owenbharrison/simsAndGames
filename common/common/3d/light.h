#pragma once

#ifndef LIGHT_STRUCT_H
#define LIGHT_STRUCT_H
namespace cmn {
	struct Light {
		vf3d pos;
		olc::Pixel col=olc::WHITE;
	};
}
#endif