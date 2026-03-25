#pragma once
#ifndef CMN_RENDER_UTILS_H
#define CMN_RENDER_UTILS_H

#include "sokol/include/sokol_gl.h"

//for sqrt
#include <cmath>

namespace cmn {
	inline constexpr float unit_circle[32][2]{
		{1.000000f, 0.000000f},
		{0.980785f, 0.195090f},
		{0.923880f, 0.382683f},
		{0.831470f, 0.555570f},
		{0.707107f, 0.707107f},
		{0.555570f, 0.831470f},
		{0.382683f, 0.923880f},
		{0.195090f, 0.980785f},
		{-0.000000f, 1.000000f},
		{-0.195090f, 0.980785f},
		{-0.382684f, 0.923880f},
		{-0.555570f, 0.831470f},
		{-0.707107f, 0.707107f},
		{-0.831470f, 0.555570f},
		{-0.923880f, 0.382683f},
		{-0.980785f, 0.195090f},
		{-1.000000f, -0.000000f},
		{-0.980785f, -0.195090f},
		{-0.923880f, -0.382683f},
		{-0.831470f, -0.555570f},
		{-0.707107f, -0.707107f},
		{-0.555570f, -0.831470f},
		{-0.382683f, -0.923880f},
		{-0.195090f, -0.980785f},
		{0.000000f, -1.000000f},
		{0.195090f, -0.980785f},
		{0.382684f, -0.923879f},
		{0.555570f, -0.831469f},
		{0.707107f, -0.707107f},
		{0.831470f, -0.555570f},
		{0.923880f, -0.382683f},
		{0.980785f, -0.195090f}
	};
	inline constexpr int unit_circle_num=sizeof(unit_circle)/sizeof(unit_circle[0]);

	static void draw_line(
		float ax, float ay, float bx, float by,
		const sg_color& col
	) {
		sgl_begin_lines();

		sgl_c4f(col.r, col.g, col.b, col.a);
		sgl_v2f(ax, ay);
		sgl_v2f(bx, by);

		sgl_end();
	}

	static void draw_triangle(
		float ax, float ay, float bx, float by, float cx, float cy,
		const sg_color& col
	) {
		draw_line(ax, ay, bx, by, col);
		draw_line(bx, by, cx, cy, col);
		draw_line(cx, cy, ax, ay, col);
	}

	static void fill_triangle(
		float ax, float ay, float bx, float by, float cx, float cy,
		const sg_color& col
	) {
		sgl_begin_triangles();

		sgl_c4f(col.r, col.g, col.b, col.a);
		sgl_v2f(ax, ay);
		sgl_v2f(bx, by);
		sgl_v2f(cx, cy);

		sgl_end();
	}

	static void draw_rect(
		float x, float y, float w, float h,
		const sg_color& col
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

	static void fill_rect(
		float x, float y, float w, float h,
		const sg_color& col
	) {
		sgl_begin_triangle_strip();
		sgl_c4f(col.r, col.g, col.b, col.a);
		sgl_v2f(x, y);
		sgl_v2f(x+w, y);
		sgl_v2f(x, y+h);
		sgl_v2f(x+w, y+h);
		sgl_end();
	}

	static void draw_circle(
		float cx, float cy, float rad,
		const sg_color& col
	) {
		sgl_begin_line_strip();

		sgl_c4f(col.r, col.g, col.b, col.a);

		for(int i=0; i<=unit_circle_num; i++) {
			float x=cx+rad*unit_circle[i%unit_circle_num][0];
			float y=cy+rad*unit_circle[i%unit_circle_num][1];

			sgl_v2f(x, y);
		}

		sgl_end();
	}

	static void fill_circle(
		float cx, float cy, float rad,
		const sg_color& col
	) {
		sgl_begin_triangle_strip();

		sgl_c4f(col.r, col.g, col.b, col.a);

		for(int i=0; i<=unit_circle_num; i++) {
			float x=cx+rad*unit_circle[i%unit_circle_num][0];
			float y=cy+rad*unit_circle[i%unit_circle_num][1];

			sgl_v2f(x, y);
			sgl_v2f(cx, cy);
		}

		sgl_end();
	}

	static void draw_thick_line(
		float ax, float ay, float bx, float by,
		float t, const sg_color& col
	) {
		//axis
		float bax=bx-ax, bay=by-ay;
		float ba=std::sqrt(bax*bax+bay*bay);

		//unit vectors
		float lx=bax/ba, ly=bay/ba;
		float wx=-ly, wy=lx;

		//deltas
		float dx=t/2*wx, dy=t/2*wy;

		sgl_begin_triangle_strip();

		sgl_c4f(col.r, col.g, col.b, col.a);
		sgl_v2f(ax+dx, ay+dy);
		sgl_v2f(bx+dx, by+dy);
		sgl_v2f(ax-dx, ay-dy);
		sgl_v2f(bx-dx, by-dy);

		sgl_end();
	}

	static void draw_thick_circle(
		float x, float y, float r, float t,
		const sg_color& col
	) {
		//first, prev
		float fx, fy, px, py;
		for(int i=0; i<unit_circle_num; i++) {
			float cx=x+r*unit_circle[i%unit_circle_num][0];
			float cy=y+r*unit_circle[i%unit_circle_num][1];

			fill_circle(cx, cy, t/2, col);

			if(i==0) fx=cx, fy=cy;
			else draw_thick_line(px, py, cx, cy, t, col);

			px=cx, py=cy;
		}
		draw_thick_line(fx, fy, px, py, t, col);
	}
}
#endif