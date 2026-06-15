#define SOKOL_IMPL
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/include/sokol_app.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "sokol/sokol_engine.h"

#include "sokol/include/sokol_gl.h"

#include "cmn/utils.h"

#include "cmn/math/v3d.h"
#include "cmn/math/mat4.h"

#include "imgui/include/imgui_singleheader.h"
#include "sokol/include/sokol_imgui.h"

//#include "rendering.h"

float rad2deg(float r) {
	return r/cmn::Pi*180;
}

float deg2rad(float d) {
	return d/180*cmn::Pi;
}

//https://en.wikipedia.org/wiki/Rotation_matrix
void axisAngle(
	const cmn::vf3d& u,
	float angle,
	cmn::vf3d* x,
	cmn::vf3d* y,
	cmn::vf3d* z
) {
	float c=std::cos(angle), s=std::sin(angle), c1=1-c;
	x->x=u.x*u.x*c1+c, x->y=u.x*u.y*c1-u.z*s, x->z=u.x*u.z*c1+u.y*s;
	y->x=u.x*u.y*c1+u.z*s, y->y=u.y*u.y*c1+c, y->z=u.y*u.z*c1-u.x*s;
	z->x=u.x*u.z*c1-u.y*s, z->y=u.y*u.z*c1+u.x*s, z->z=u.z*u.z*c1+c;
}

struct Arm {
	cmn::vf3d axis;
	float angle=0;
	cmn::vf3d off;
	float rgb[3];
};

using cmn::vf3d;
using cmn::mat4;

class Robot : public cmn::SokolEngine {
	Arm arms[3]{
		{{0, 1, 0}, 0, {0, 2, 0}, {0, 1, 1}},//cyan
		{{0, 0, 1}, 0, {1.5f, 0, 0}, {1, 0, 1}},//magenta
		{{0, 0, 1}, 0, {1, 0, 0}, {1, 1, 0}}//yellow
	};

	struct {
		vf3d pos{2.5f, 3.5f, 2.5f};
		float yaw=0, pitch=0;
		vf3d dir;

		float fov_deg=90;

		mat4 proj;
		mat4 view;
	} cam;

	//graphics
	sgl_pipeline pip{};

public:
	//look at origin
	void setupCamera() {
		vf3d ryp=vf3d::cartesian(-cam.pos);
		cam.yaw=ryp.y;
		cam.pitch=ryp.z;
	}

	void setupImGui() {
		simgui_desc_t simgui_desc{};
		simgui_desc.ini_filename="assets/imgui.ini";
		simgui_setup(simgui_desc);
	}

	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);
	}

	void setupPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.face_winding=SG_FACEWINDING_CCW;
		pip_desc.cull_mode=SG_CULLMODE_BACK;
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip=sgl_make_pipeline(pip_desc);
	}

	bool user_create() override {
		app_title="Robot";

		setupCamera();

		setupImGui();

		setupSGL();

		setupPipeline();

		return true;
	}

	void user_destroy() override {
		simgui_shutdown();
		sgl_shutdown();
	}

	void user_input(const sapp_event* e) override {
		simgui_handle_event(e);
	}

#pragma region UPDATE_HELPERS
	void handleCameraLooking(float dt) {
		//up/down
		if(GetKey(SAPP_KEYCODE_UP).held) cam.pitch+=dt;
		if(GetKey(SAPP_KEYCODE_DOWN).held) cam.pitch-=dt;
		cam.pitch=cmn::clamp(cam.pitch, .001f-cmn::Pi/2, cmn::Pi/2-.001f);

		//left/right
		if(GetKey(SAPP_KEYCODE_LEFT).held) cam.yaw-=dt;
		if(GetKey(SAPP_KEYCODE_RIGHT).held) cam.yaw+=dt;
	}

	void handleCameraMovement(float dt) {
		//up/down
		if(GetKey(SAPP_KEYCODE_SPACE).held) cam.pos.y+=4.f*dt;
		if(GetKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam.pos.y-=4.f*dt;

		//forward/backward
		vf3d fwd=normalize(vf3d(1, 0, 1)*cam.dir);
		if(GetKey(SAPP_KEYCODE_W).held) cam.pos+=5.f*dt*fwd;
		if(GetKey(SAPP_KEYCODE_S).held) cam.pos-=3.f*dt*fwd;

		//left/right
		vf3d rgt=normalize(cross(fwd, vf3d(0, 1, 0)));
		if(GetKey(SAPP_KEYCODE_A).held) cam.pos-=4.f*dt*rgt;
		if(GetKey(SAPP_KEYCODE_D).held) cam.pos+=4.f*dt*rgt;
	}

	void updateCameraMatrixes() {
		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(cam.fov_deg, asp, .1f, 100);
	}
#pragma endregion

	bool user_update(float dt) override {
		handleCameraLooking(dt);

		cam.dir=vf3d::polar({1, cam.yaw, cam.pitch});

		handleCameraMovement(dt);

		updateCameraMatrixes();

		return true;
	}

#pragma region RENDER_HELPERS
	void sgl_vec(const vf3d v) {
		sgl_v3f(v.x, v.y, v.z);
	}
	
	//basic lighting
	void sgl_tri(
		const vf3d& t0,
		const vf3d& t1,
		const vf3d& t2,
		float r, float g, float b
	) {
		vf3d norm=normalize(cross(t1-t0, t2-t0));
		vf3d ctr=(t0+t1+t2)/3;
		vf3d light_dir=normalize(cam.pos-ctr);
		float dp=std::max(.5f, dot(light_dir, norm));
		sgl_c3f(dp*r, dp*g, dp*b);
		sgl_vec(t0), sgl_vec(t1), sgl_vec(t2);
	}

	//origin & axes
	void renderCoordinateSystem(
		const vf3d& o,
		const vf3d& x,
		const vf3d& y,
		const vf3d& z,
		float sz=1
	) {
		sgl_begin_lines();

		sgl_c3f(1, 0, 0);//red
		sgl_vec(o), sgl_vec(o+sz*x);
		sgl_c3f(0, 1, 0);//green
		sgl_vec(o), sgl_vec(o+sz*y);
		sgl_c3f(0, 0, 1);//blue
		sgl_vec(o), sgl_vec(o+sz*z);

		sgl_end();
	}

	void renderCylinder(
		const vf3d& p,
		const vf3d& q,
		float rad,
		float r, float g, float b
	) {
		static const int num=32;
		static vf3d verts[2*num];

		vf3d x=normalize(q-p);
		vf3d y=normalize(
			std::abs(x.x)>std::abs(x.z)?
			vf3d(-x.y, x.x, 0):
			vf3d(0, -x.z, x.y)
		);
		vf3d z=cross(x, y);

		for(int i=0; i<num; i++) {
			float angle=2*cmn::Pi*i/num;
			float c=std::cos(angle);
			float s=std::sin(angle);
			vf3d o=rad*(c*y+s*z);
			verts[i]=p+o;
			verts[num+i]=q+o;
		}

		sgl_begin_triangles();

		//caps
		for(int i=2; i<num; i++) {
			sgl_tri(
				verts[0],
				verts[i],
				verts[i-1],
				r, g, b
			);
			sgl_tri(
				verts[num+0],
				verts[num+i-1],
				verts[num+i],
				r, g, b
			);
		}

		//edge
		for(int i=0; i<num; i++) {
			int j=(i+1)%num;
			const auto& v0=verts[i], & v1=verts[j];
			const auto& v2=verts[num+i], & v3=verts[num+j];
			sgl_tri(v0, v1, v2, r, g, b);
			sgl_tri(v1, v3, v2, r, g, b);
		}

		sgl_end();
	}

	void renderRobot() {
		//base system
		vf3d x_prev{1, 0, 0};
		vf3d y_prev{0, 1, 0};
		vf3d z_prev{0, 0, 1};
		vf3d orig_prev;

		renderCoordinateSystem(orig_prev, x_prev, y_prev, z_prev);

		for(const auto& a:arms) {
			//system in local space
			vf3d x_l, y_l, z_l;
			axisAngle(
				a.axis, a.angle,
				&x_l, &y_l, &z_l
			);

			//system in world space
			vf3d x=x_l.x*x_prev+x_l.y*y_prev+x_l.z*z_prev;
			vf3d y=y_l.x*x_prev+y_l.y*y_prev+y_l.z*z_prev;
			vf3d z=z_l.x*x_prev+z_l.y*y_prev+z_l.z*z_prev;
			vf3d axis=a.axis.x*x+a.axis.y*y+a.axis.z*z;
			vf3d orig=orig_prev+a.off.x*x+a.off.y*y+a.off.z*z;

			renderCoordinateSystem(orig, x, y, z, .4f);

			//base
			renderCylinder(
				orig_prev-.2f*axis, orig_prev+.2f*axis, .3f,
				a.rgb[0], a.rgb[1], a.rgb[2]
			);

			//arm
			renderCylinder(
				orig_prev, orig, .15f,
				a.rgb[0], a.rgb[1], a.rgb[2]
			);

			orig_prev=orig;
			x_prev=x;
			y_prev=y;
			z_prev=z;
		}
	}

	void renderImGui() {
		simgui_frame_desc_t simgui_frame_desc{};
		simgui_frame_desc.width=sapp_width();
		simgui_frame_desc.height=sapp_height();
		simgui_frame_desc.delta_time=sapp_frame_duration();
		simgui_frame_desc.dpi_scale=sapp_dpi_scale();
		simgui_new_frame(simgui_frame_desc);

		ImGui::Begin("options");
		for(auto& a:arms) {
			int i=&a-arms;

			if(!ImGui::TreeNodeEx(
				(void*)i, ImGuiTreeNodeFlags_DefaultOpen,
				"arm %d", 1+i
			)) continue;

			float axis[3]{a.axis.x, a.axis.y, a.axis.z};
			ImGui::SliderFloat3("axis", axis, -1, 1);
			a.axis.x=axis[0], a.axis.y=axis[1], a.axis.z=axis[2];
			float axis_mag=length(a.axis);
			if(axis_mag!=0) a.axis/=axis_mag;

			float angle_deg=rad2deg(a.angle);
			ImGui::DragFloat("angle", &angle_deg, .5f);
			a.angle=deg2rad(angle_deg);

			float off[3]{a.off.x, a.off.y, a.off.z};
			ImGui::DragFloat3("offset", off, .05f);
			a.off.x=off[0], a.off.y=off[1], a.off.z=off[2];

			ImGui::ColorEdit3("color", a.rgb);

			ImGui::TreePop();
		}
		ImGui::End();

		simgui_render();
	}
#pragma endregion

	bool user_render() override {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={.12f, .12f, .12f, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_projection();
		sgl_load_matrix(cam.proj.m);
		sgl_matrix_mode_modelview();
		sgl_load_matrix(cam.view.m);

		renderRobot();

		sgl_draw();

		renderImGui();

		sg_end_pass();

		sg_commit();

		return true;
	}
};