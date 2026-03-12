#pragma once
#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H

#include "sokol/include/sokol_gl.h"

//for cos, sin, sqrt
#include <cmath>

static void sgl_draw_circle(
	float cx, float cy, float rad,
	sg_color col
) {
	sgl_begin_line_strip();

	sgl_c4f(col.r, col.g, col.b, col.a);

	const int num=32;
	float rgb[3];
	for(int i=0; i<=num; i++) {
		float angle=2*3.1415927f*i/num;

		float x=cx+rad*std::cos(angle);
		float y=cy+rad*std::sin(angle);

		sgl_v2f(x, y);
	}

	sgl_end();
}

static void sgl_fill_circle(
	float cx, float cy, float rad,
	sg_color col
) {
	sgl_begin_triangle_strip();

	sgl_c4f(col.r, col.g, col.b, col.a);
	
	const int num=32;
	float rgb[3];
	for(int i=0; i<=num; i++) {
		float angle=2*3.1415927f*i/num;

		float x=cx+rad*std::cos(angle);
		float y=cy+rad*std::sin(angle);

		sgl_v2f(x, y);
		sgl_v2f(cx, cy);
	}

	sgl_end();
}

static void sgl_draw_line(
	float ax, float ay, float bx, float by,
	sg_color col
) {
	sgl_begin_lines();
	sgl_c4f(col.r, col.g, col.b, col.a);
	sgl_v2f(ax, ay);
	sgl_v2f(bx, by);
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
	sgl_c4f(col.r, col.g, col.b, col.a);
	sgl_v2f(ax+dx, ay+dy);
	sgl_v2f(bx+dx, by+dy);
	sgl_v2f(ax-dx, ay-dy);
	sgl_v2f(bx-dx, by-dy);
	sgl_end();
}

static void sgl_draw_rect(
	float x, float y, float w, float h,
	sg_color col
) {
	sgl_begin_line_strip();
	sgl_c4f(col.r, col.g, col.b, col.a);
	sgl_v2f(x, y);
	sgl_v2f(x+w, y);
	sgl_v2f(x+w, y+h);
	sgl_v2f(x, y+h);
	sgl_v2f(x, y);
	sgl_end();
}

static void sgl_fill_rect(
	float x, float y, float w, float h,
	sg_color col,
	float l=0, float t=0, float r=1, float b=1
) {
	sgl_begin_triangle_strip();
	sgl_c4f(col.r, col.g, col.b, col.a);
	sgl_v2f_t2f(x, y, l, t);
	sgl_v2f_t2f(x+w, y, r, t);
	sgl_v2f_t2f(x, y+h, l, b);
	sgl_v2f_t2f(x+w, y+h, r, b);
	sgl_end();
}
#endif