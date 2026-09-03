#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "vendor/sokol/sokol_app.h"
#include "vendor/sokol/sokol_gfx.h"
#include "vendor/sokol/sokol_glue.h"

#include "vendor/sokol/sokol_gl.h"

#include "common/sokol/sokol_engine.h"

#include "common/math/v3d.h"

#include "common/utils.h"

cmn::vf3d segIntersectPlane(
	const cmn::vf3d& a, const cmn::vf3d& b,
	const cmn::vf3d& ctr, const cmn::vf3d& norm
) {
	float t=norm.dot(ctr-a)/norm.dot(b-a);
	return a+t*(b-a);
}

//rotate point about origin along 3 axes
cmn::vf3d rotateXYZ(
	const cmn::vf3d& p,
	float rx, float ry, float rz
) {
	//precompute
	float cx=std::cos(rx);
	float sx=std::sin(rx);
	float cy=std::cos(ry);
	float sy=std::sin(ry);
	float cz=std::cos(rz);
	float sz=std::sin(rz);
	//x rot
	float x=p.x;
	float y=p.y*cx-p.z*sx;
	float z=p.y*sx+p.z*cx;
	//y rot
	float x2=x*cy+z*sy;
	float z2=-x*sy+z*cy;
	//z rot
	float x3=x2*cz-y*sz;
	float y3=x2*sz+y*cz;
	return {x3, y3, z2};
}

using cmn::vf3d;

struct Demo : cmn::SokolEngine {
	struct {
		vf3d pos{1, 2, 3}, dir;
		float pitch=0, yaw=0;
	} cam;
	
	vf3d vtx[4]{
		{1, 1, 1},
		{-1, 1, -1},
		{1, -1, -1},
		{-1, -1, 1}
	};

	sgl_pipeline pip{};

	bool show_outlines=false;

	struct {
		vf3d ctr, norm{1, 0, 0};
		bool to_spin=true;
		float spin=0;
	} plane;

	bool user_create() override {
		app_title="[tetra]";

		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);

		vf3d ryp=vf3d::cartesian(-cam.pos);
		cam.yaw=ryp.y;
		cam.pitch=ryp.z;

		sg_pipeline_desc pip_desc{};
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip=sgl_make_pipeline(pip_desc);

		return true;
	}
	
	bool user_update(float dt) override {
		vf3d fwd=normalize(vf3d(1, 0, 1)*cam.dir);
		if(GetKey(SAPP_KEYCODE_W).held) cam.pos+=5*dt*fwd;
		if(GetKey(SAPP_KEYCODE_S).held) cam.pos-=3*dt*fwd;

		vf3d rgt=cross(fwd, vf3d(0, 1, 0));
		if(GetKey(SAPP_KEYCODE_A).held) cam.pos-=4*dt*rgt;
		if(GetKey(SAPP_KEYCODE_D).held) cam.pos+=4*dt*rgt;

		if(GetKey(SAPP_KEYCODE_SPACE).held) cam.pos.y+=4*dt;
		if(GetKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam.pos.y-=4*dt;

		if(GetKey(SAPP_KEYCODE_UP).held) cam.pitch+=dt;
		if(GetKey(SAPP_KEYCODE_DOWN).held) cam.pitch-=dt;
		cam.pitch=cmn::clamp(cam.pitch, .01f-.5f*cmn::Pi, .5f*cmn::Pi-.01f);

		if(GetKey(SAPP_KEYCODE_LEFT).held) cam.yaw-=dt;
		if(GetKey(SAPP_KEYCODE_RIGHT).held) cam.yaw+=dt;

		cam.dir=vf3d::polar({1, cam.yaw, cam.pitch});

		if(GetKey(SAPP_KEYCODE_1).held) vtx[0]=cam.pos;
		if(GetKey(SAPP_KEYCODE_2).held) vtx[1]=cam.pos;
		if(GetKey(SAPP_KEYCODE_3).held) vtx[2]=cam.pos;
		if(GetKey(SAPP_KEYCODE_4).held) vtx[3]=cam.pos;

		if(GetKey(SAPP_KEYCODE_O).pressed) show_outlines^=true;

		if(GetKey(SAPP_KEYCODE_ENTER).pressed) plane.to_spin^=true;

		if(plane.to_spin) {
			plane.spin+=dt;
			
			//irrational speeds
			plane.norm=rotateXYZ(
				normalize(vf3d(1, 1, 1)),
				plane.spin/std::sqrt(3),
				plane.spin/std::sqrt(5),
				plane.spin/std::sqrt(6)
			);

			float amt=std::sin(plane.spin/std::sqrt(7));
			plane.ctr=amt*plane.norm;
		}

		return true;
	}

#pragma region RENDER HELPERS
	void sgl_vec(const vf3d& v) {
		sgl_v3f(v.x, v.y, v.z);
	}

	void sgl_tri(const vf3d& a, const vf3d& b, const vf3d& c) {
		sgl_vec(a), sgl_vec(b), sgl_vec(c);
	}

	void renderBox(const vf3d& a, const vf3d& b) {
		static const int edges[][2]{
			{0, 1}, {2, 3}, {4, 5}, {6, 7},//thru x
			{0, 2}, {1, 3}, {4, 6}, {5, 7},//thru y
			{0, 4}, {1, 5}, {2, 6}, {3, 7}//thru z
		};

		vf3d v[8];//interpolators: 000, 001, 010...
		for(int i=0; i<8; i++) {
			v[i]=a+vf3d(1&(i>>2), 1&(i>>1), 1&i)*(b-a);
		}

		sgl_begin_lines();
		for(const auto& e:edges) {
			sgl_vec(v[e[0]]), sgl_vec(v[e[1]]);
		}
		sgl_end();
	}

	void renderAxes(const vf3d& o, float sz) {		
		sgl_begin_lines();
		sgl_c3f(1, 0, 0);
		sgl_vec(o), sgl_vec(o+vf3d(sz, 0, 0));
		sgl_c3f(0, 1, 0);
		sgl_vec(o), sgl_vec(o+vf3d(0, sz, 0));
		sgl_c3f(0, 0, 1);
		sgl_vec(o), sgl_vec(o+vf3d(0, 0, sz));
		sgl_end();
	}

	void renderPlane(float sz) {
		vf3d fwd=plane.norm;
		vf3d rgt=normalize(cross(fwd, vf3d(0, 1, 0)));
		vf3d up=cross(rgt, fwd);

		vf3d tl=plane.ctr+.5f*sz*(up-rgt);
		vf3d tr=plane.ctr+.5f*sz*(up+rgt);
		vf3d bl=plane.ctr+.5f*sz*(-up-rgt);
		vf3d br=plane.ctr+.5f*sz*(-up+rgt);

		sgl_begin_line_strip();
		sgl_vec(tl);
		sgl_vec(tr);
		sgl_vec(br);
		sgl_vec(bl);
		sgl_vec(tl);
		sgl_vec(br);
		sgl_end();
	}

	void renderFilledTetra(
		const vf3d& a,
		const vf3d& b,
		const vf3d& c,
		const vf3d& d
	) {
		sgl_begin_triangles();
		sgl_tri(a, b, c);
		sgl_tri(a, b, d);
		sgl_tri(a, c, d);
		sgl_tri(b, c, d);
		sgl_end();
	}

	//ab, ac, ad, bc, bd, cd
	void renderOutlinedTetra(
		const vf3d& a,
		const vf3d& b,
		const vf3d& c,
		const vf3d& d
	) {
		sgl_begin_lines();
		sgl_vec(a), sgl_vec(b);
		sgl_vec(a), sgl_vec(c);
		sgl_vec(a), sgl_vec(d);
		sgl_vec(b), sgl_vec(c);
		sgl_vec(b), sgl_vec(d);
		sgl_vec(c), sgl_vec(d);
		sgl_end();
	}

	void renderTetra(
		const vf3d& a,
		const vf3d& b,
		const vf3d& c,
		const vf3d& d
	) {
		if(show_outlines) renderOutlinedTetra(a, b, c, d);
		else renderFilledTetra(a, b, c, d);
	}

	void splitBy(vf3d ctr, vf3d norm) {
		vf3d v[4]{vtx[0], vtx[1], vtx[2], vtx[3]};

		//categorize vertexes
		int pos_ct=0, neg_ct=0;
		int pos_ix[4], neg_ix[4];
		for(int i=0; i<4; i++) {
			bool s=dot(v[i]-ctr, norm)>0;
			if(s) pos_ix[pos_ct++]=i;
			else neg_ix[neg_ct++]=i;
		}

		//construct new tetras
		switch(pos_ct) {
			case 0://red
				sgl_c3f(1, 0, 0);
				renderTetra(v[0], v[1], v[2], v[3]);
				break;
			case 1: {//yellow & light grey
				const auto& p0=v[pos_ix[0]];
				const auto& n0=v[neg_ix[0]];
				const auto& n1=v[neg_ix[1]];
				const auto& n2=v[neg_ix[2]];
				vf3d i00=segIntersectPlane(p0, n0, ctr, norm);
				vf3d i01=segIntersectPlane(p0, n1, ctr, norm);
				vf3d i02=segIntersectPlane(p0, n2, ctr, norm);
				
				sgl_c3f(1, 1, 0);
				renderTetra(p0, i00, i01, i02);

				sgl_c3f(.75f, .75f, .75f);
				renderTetra(n0, i00, i01, i02);
				renderTetra(n0, n1, i01, i02);
				renderTetra(n0, n1, n2, i02);

				break;
			}
			case 2: {//green & grey
				const auto& p0=v[pos_ix[0]];
				const auto& p1=v[pos_ix[1]];
				const auto& n0=v[neg_ix[0]];
				const auto& n1=v[neg_ix[1]];
				vf3d i00=segIntersectPlane(p0, n0, ctr, norm);
				vf3d i01=segIntersectPlane(p0, n1, ctr, norm);
				vf3d i10=segIntersectPlane(p1, n0, ctr, norm);
				vf3d i11=segIntersectPlane(p1, n1, ctr, norm);

				sgl_c3f(0, 1, 0);
				renderTetra(p0, p1, i00, i01);
				renderTetra(p1, i00, i11, i01);
				renderTetra(p1, i11, i10, i00);

				sgl_c3f(.5f, .5f, .5f);
				renderTetra(n0, n1, i00, i10);
				renderTetra(n1, i11, i00, i10);
				renderTetra(n1, i11, i01, i00);

				break;
			}
			case 3: {//cyan & dark grey
				const auto& p0=v[pos_ix[0]];
				const auto& p1=v[pos_ix[1]];
				const auto& p2=v[pos_ix[2]];
				const auto& n0=v[neg_ix[0]];
				vf3d i00=segIntersectPlane(p0, n0, ctr, norm);
				vf3d i10=segIntersectPlane(p1, n0, ctr, norm);
				vf3d i20=segIntersectPlane(p2, n0, ctr, norm);

				sgl_c3f(0, 1, 1);
				renderTetra(p0, i00, i10, i20);
				renderTetra(p0, p1, i10, i20);
				renderTetra(p0, p1, p2, i20);

				sgl_c3f(.25f, .25f, .25f);
				renderTetra(n0, i00, i10, i20);

				break;
			}
			case 4://blue
				sgl_c3f(0, 0, 1);
				renderTetra(v[0], v[1], v[2], v[3]);
				break;
		}
	}
#pragma endregion

	bool user_render() override {
		//white bg
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={1, 1, 1, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_modelview();
		vf3d e=cam.pos, t=e+cam.dir, u(0, 1, 0);
		sgl_lookat(e.x, e.y, e.z, t.x, t.y, t.z, u.x, u.y, u.z);
		sgl_matrix_mode_projection();
		sgl_perspective(sgl_rad(90), sapp_widthf()/sapp_heightf(), .01f, 100);

		{
			static bool show=true;
			if(GetKey(SAPP_KEYCODE_TAB).pressed) show^=true;
			if(show) renderAxes(cam.pos+cam.dir, .1f);
		}

		//black
		sgl_c3f(0, 0, 0);
		renderBox({-1, -1, -1}, {1, 1, 1});
		
		//purple
		sgl_c3f(.5f, 0, 1);
		renderPlane(3);

		splitBy(plane.ctr, plane.norm);

		sgl_draw();

		sg_end_pass();

		sg_commit();

		return true;
	}
};

CMN_SOKOL_ENGINE_LAUNCH(Demo, 640, 480)