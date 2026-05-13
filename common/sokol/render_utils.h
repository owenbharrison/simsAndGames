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
	inline constexpr int num_unit_circle=sizeof(unit_circle)/sizeof(unit_circle[0]);

	inline void draw_line(
		float x1, float y1, float x2, float y2,
		float r, float g, float b, float a=1
	) {
		sgl_begin_lines();

		sgl_c4f(r, g, b, a);

		sgl_v2f(x1, y1);
		sgl_v2f(x2, y2);

		sgl_end();
	}

	inline void draw_triangle(
		float x1, float y1,
		float x2, float y2,
		float x3, float y3,
		float r, float g, float b, float a=1
	) {
		sgl_begin_line_strip();

		sgl_c4f(r, g, b, a);
		
		sgl_v2f(x1, y1);
		sgl_v2f(x2, y2);
		sgl_v2f(x3, y3);
		sgl_v2f(x1, y1);

		sgl_end();
	}

	inline void fill_triangle(
		float x1, float y1,
		float x2, float y2,
		float x3, float y3,
		float r, float g, float b, float a=1
	) {
		sgl_begin_triangles();

		sgl_c4f(r, g, b, a);

		sgl_v2f(x1, y1);
		sgl_v2f(x2, y2);
		sgl_v2f(x3, y3);

		sgl_end();
	}

	inline void draw_rect(
		float x, float y, float w, float h,
		float r, float g, float b, float a=1
	) {
		sgl_begin_line_strip();

		sgl_c4f(r, g, b, a);

		sgl_v2f(x, y);
		sgl_v2f(x+w, y);
		sgl_v2f(x+w, y+h);
		sgl_v2f(x, y+h);
		sgl_v2f(x, y);

		sgl_end();
	}

	inline void fill_rect(
		float x, float y, float w, float h,
		float r, float g, float b, float a=1
	) {
		sgl_begin_quads();

		sgl_c4f(r, g, b, a);

		sgl_v2f(x, y);
		sgl_v2f(x+w, y);
		sgl_v2f(x+w, y+h);
		sgl_v2f(x, y+h);

		sgl_end();
	}

	inline void draw_circle(
		float x, float y, float rad,
		float r, float g, float b, float a=1
	) {
		sgl_begin_line_strip();

		sgl_c4f(r, g, b, a);

		for(int i=0; i<=num_unit_circle; i++) {
			sgl_v2f(
				x+rad*unit_circle[i%num_unit_circle][0],
				y+rad*unit_circle[i%num_unit_circle][1]
			);
		}

		sgl_end();
	}

	inline void fill_circle(
		float x, float y, float rad,
		float r, float g, float b, float a=1
	) {
		sgl_begin_triangles();

		sgl_c4f(r, g, b, a);

		for(int i=0; i<num_unit_circle; i++) {
			float cx=x+rad*unit_circle[i][0];
			float cy=y+rad*unit_circle[i][1];
			float nx=x+rad*unit_circle[(1+i)%num_unit_circle][0];
			float ny=y+rad*unit_circle[(1+i)%num_unit_circle][1];
			sgl_v2f(x, y);
			sgl_v2f(cx, cy);
			sgl_v2f(nx, ny);
		}

		sgl_end();
	}

	inline void draw_thick_line(
		float ax, float ay, float bx, float by,
		float t,
		float r, float g, float b, float a=1
	) {
		//axis
		float abx=bx-ax, aby=by-ay;
		float ab_l=std::sqrt(abx*abx+aby*aby);

		//unit vectors
		float lx=abx/ab_l, ly=aby/ab_l;
		float wx=-ly, wy=lx;

		//deltas
		float dx=t/2*wx, dy=t/2*wy;

		sgl_begin_quads();

		sgl_c4f(r, g, b, a);

		sgl_v2f(ax-dx, ay-dy);
		sgl_v2f(bx-dx, by-dy);
		sgl_v2f(bx+dx, by+dy);
		sgl_v2f(ax+dx, ay+dy);

		sgl_end();
	}

	//thick circle :3
	inline void fill_torus(
		float x, float y,
		float r_in, float r_out,
		float r, float g, float b, float a=1
	) {
		sgl_begin_triangle_strip();

		sgl_c4f(r, g, b, a);

		for(int i=0; i<=num_unit_circle; i++) {
			float ux=unit_circle[i%num_unit_circle][0];
			float uy=unit_circle[i%num_unit_circle][1];
			sgl_v2f(x+r_in*ux, y+r_in*uy);
			sgl_v2f(x+r_out*ux, y+r_out*uy);
		}

		sgl_end();
	}

	inline void draw_thick_rect(
		float x, float y, float w, float h,
		float t,
		float r, float g, float b, float a=1
	) {
		float x1=x, x2=x1+t, x4=x+w, x3=x4-t;
		float y1=y, y2=y1+t, y4=y+h, y3=y4-t;
		
		sgl_begin_quads();

		sgl_c4f(r, g, b, a);

		sgl_v2f(x1, y1), sgl_v2f(x4, y1), sgl_v2f(x3, y2), sgl_v2f(x2, y2);
		sgl_v2f(x4, y1), sgl_v2f(x4, y4), sgl_v2f(x3, y3), sgl_v2f(x3, y2);
		sgl_v2f(x4, y4), sgl_v2f(x1, y4), sgl_v2f(x2, y3), sgl_v2f(x3, y3);
		sgl_v2f(x1, y4), sgl_v2f(x1, y1), sgl_v2f(x2, y2), sgl_v2f(x2, y3);

		sgl_end();
	}
}
#endif