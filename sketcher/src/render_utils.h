#pragma once
#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H

#include "sokol/include/sokol_gl.h"

//for sin, cos, sqrt
#include <cmath>

static void sgl_draw_circle(
	float cx, float cy, float rad,
	sg_color col
) {
	sgl_begin_line_strip();

	const int num=32;
	float rgb[3];
	for(int i=0; i<=num; i++) {
		float angle=2*3.1415927f*i/num;

		float x=cx+rad*std::cos(angle);
		float y=cy+rad*std::sin(angle);

		sgl_v2f_c4f(x, y, col.r, col.g, col.b, col.a);
	}

	sgl_end();
}

static void sgl_fill_circle(
	float cx, float cy, float rad,
	sg_color col
) {
	sgl_begin_triangle_strip();

	const int num=32;
	float rgb[3];
	for(int i=0; i<=num; i++) {
		float angle=2*3.1415927f*i/num;

		float x=cx+rad*std::cos(angle);
		float y=cy+rad*std::sin(angle);

		sgl_v2f_c4f(x, y, col.r, col.g, col.b, col.a);
		sgl_v2f_c4f(cx, cy, col.r, col.g, col.b, col.a);
	}

	sgl_end();
}

static void sgl_draw_line(
	float ax, float ay, float bx, float by,
	sg_color col
) {
	sgl_begin_lines();
	sgl_v2f_c4f(ax, ay, col.r, col.g, col.b, col.a);
	sgl_v2f_c4f(bx, by, col.r, col.g, col.b, col.a);
	sgl_end();
}

static void sgl_fill_line(
	float ax, float ay, float bx, float by,
	float w, sg_color col
) {
	//axis
	float bax=bx-ax, bay=by-ay;
	float ba=std::sqrt(bax*bax+bay*bay);

	//unit vectors
	float lx=bax/ba, ly=bay/ba;
	float wx=-ly, wy=lx;

	//deltas
	float dx=w/2*wx, dy=w/2*wy;

	sgl_begin_triangle_strip();
	sgl_v2f_c4f(ax+dx, ay+dy, col.r, col.g, col.b, col.a);
	sgl_v2f_c4f(bx+dx, by+dy, col.r, col.g, col.b, col.a);
	sgl_v2f_c4f(ax-dx, ay-dy, col.r, col.g, col.b, col.a);
	sgl_v2f_c4f(bx-dx, by-dy, col.r, col.g, col.b, col.a);
	sgl_end();
}

static void sgl_draw_rect(
	float x, float y, float w, float h,
	sg_color col
) {
	sgl_begin_line_strip();
	sgl_v2f_c4f(x, y, col.r, col.g, col.b, col.a);
	sgl_v2f_c4f(x+w, y, col.r, col.g, col.b, col.a);
	sgl_v2f_c4f(x+w, y+h, col.r, col.g, col.b, col.a);
	sgl_v2f_c4f(x, y+h, col.r, col.g, col.b, col.a);
	sgl_v2f_c4f(x, y, col.r, col.g, col.b, col.a);
	sgl_end();
}

static void sgl_fill_rect(
	float x, float y, float w, float h,
	sg_color col
) {
	sgl_begin_triangle_strip();
	sgl_v2f_c4f(x, y, col.r, col.g, col.b, col.a);
	sgl_v2f_c4f(x+w, y, col.r, col.g, col.b, col.a);
	sgl_v2f_c4f(x, y+h, col.r, col.g, col.b, col.a);
	sgl_v2f_c4f(x+w, y+h, col.r, col.g, col.b, col.a);
	sgl_end();
}
#endif