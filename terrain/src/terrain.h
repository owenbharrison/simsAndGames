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

#include "noise.h"

using cmn::vf3d;
using cmn::mat4;

class Terrain : public cmn::SokolEngine {
	//scene
	struct {
		bool to_update=true;

		float width=2, depth=2;
		float resolution=.1f;
		float y_min=-6, y_max=6;

		int num_x=0, num_z=0;
		int ix(int i, int k) { return i+num_x*k; }

		vf3d* grid=nullptr;

		float scl_noise=.1f;
		float x_noise_offset=0;
		float z_noise_offset=0;
		float y_noise=36.1f;
		int octaves=7;

		bool show_base=true;
		bool show_stems=true;
		bool fill_surface=true;
	} terrain;

	//user input
	struct {
		vf3d pos{1.5f, 1.5f, 1.5f};
		float yaw=0, pitch=0;
		vf3d dir;

		mat4 proj;
		mat4 view;
	} cam;

	//graphics
	sgl_pipeline pip{};

public:
#pragma region CREATE_HELPERS
	void setupImGui() {
		simgui_desc_t simgui_desc{};
		simgui_desc.ini_filename="assets/imgui.ini";
		simgui_setup(simgui_desc);
	}

	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_desc.max_vertices=1000000;
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
#pragma endregion

	bool user_create() override {
		app_title="Terrain";

		//look at origin
		{
			vf3d ryp=vf3d::cartesian(-cam.pos);
			cam.yaw=ryp.y;
			cam.pitch=ryp.z;
		}

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

	void handleCameraLooking(float dt) {
		//up/down
		if(GetKey(SAPP_KEYCODE_UP).held) cam.pitch+=dt;
		if(GetKey(SAPP_KEYCODE_DOWN).held) cam.pitch-=dt;
		cam.pitch=cmn::clamp(cam.pitch, .001f-cmn::Pi/2, cmn::Pi/2-.001f);

		//left/right
		if(GetKey(SAPP_KEYCODE_LEFT).held) cam.yaw-=dt;
		if(GetKey(SAPP_KEYCODE_RIGHT).held) cam.yaw+=dt;
	}

	void updateCamera() {
		cam.dir=vf3d::polar({1, cam.yaw, cam.pitch});

		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(90, asp, .1f, 1000);
	}

	void updateSizing() {
		//determine spacing
		int num_x=1+terrain.width/terrain.resolution;
		int num_z=1+terrain.depth/terrain.resolution;

		//avoid allocations
		if(num_x==terrain.num_x&&num_z==terrain.num_z) return;

		//resize & realloc
		terrain.num_x=num_x;
		terrain.num_z=num_z;
		delete[] terrain.grid;
		terrain.grid=new vf3d[terrain.num_x*terrain.num_z];
	}

	void updateGrid() {
		for(int i=0; i<terrain.num_x; i++) {
			for(int k=0; k<terrain.num_z; k++) {
				//centered about 0,0
				float x=terrain.resolution*(i-.5f*terrain.num_x);
				float z=terrain.resolution*(k-.5f*terrain.num_z);
				float x_noise=terrain.scl_noise*(terrain.x_noise_offset+x);
				float y_noise=terrain.scl_noise*terrain.y_noise;
				float z_noise=terrain.scl_noise*(terrain.z_noise_offset+z);
				float y01=fbm13(x_noise, y_noise, z_noise, terrain.octaves);
				float y=mix(terrain.y_min, terrain.y_max, y01);
				terrain.grid[terrain.ix(i, k)]={x, y, z};
			}
		}
	}
#pragma endregion

	bool user_update(float dt) override {
		handleCameraMovement(dt);

		handleCameraLooking(dt);

		updateCamera();

		if(terrain.to_update) {
			updateSizing();

			updateGrid();

			terrain.to_update=false;
		}

		return true;
	}

#pragma region RENDER_HELPERS
	void renderBase(float r, float g, float b) {
		sgl_begin_lines();

		sgl_c3f(r, g, b);

		for(int i=0; i<terrain.num_x; i++) {
			for(int k=0; k<terrain.num_z; k++) {
				const auto& c=terrain.grid[terrain.ix(i, k)];
				if(i>0) {
					const auto& o=terrain.grid[terrain.ix(i-1, k)];
					sgl_v3f(c.x, 0, c.z), sgl_v3f(o.x, 0, o.z);
				}
				if(k>0) {
					const auto& o=terrain.grid[terrain.ix(i, k-1)];
					sgl_v3f(c.x, 0, c.z), sgl_v3f(o.x, 0, o.z);
				}
			}
		}

		sgl_end();
	}

	void renderStems(
		float r1, float g1, float b1,
		float r2, float g2, float b2
	) {
		sgl_begin_lines();

		for(int i=0; i<terrain.num_x; i++) {
			for(int k=0; k<terrain.num_z; k++) {
				const auto& c=terrain.grid[terrain.ix(i, k)];
				if(c.y>0) sgl_c3f(r1, g1, b1);
				else sgl_c3f(r2, g2, b2);
				sgl_v3f(c.x, 0, c.z), sgl_v3f(c.x, c.y, c.z);
			}
		}

		sgl_end();
	}

	void renderOutlinedSurface(float r, float g, float b) {
		sgl_begin_lines();

		sgl_c3f(r, g, b);

		for(int i=0; i<terrain.num_x; i++) {
			for(int k=0; k<terrain.num_z; k++) {
				const auto& c=terrain.grid[terrain.ix(i, k)];
				if(i>0) {
					const auto& o=terrain.grid[terrain.ix(i-1, k)];
					sgl_v3f(c.x, c.y, c.z), sgl_v3f(o.x, o.y, o.z);
				}
				if(k>0) {
					const auto& o=terrain.grid[terrain.ix(i, k-1)];
					sgl_v3f(c.x, c.y, c.z), sgl_v3f(o.x, o.y, o.z);
				}
			}
		}

		sgl_end();
	}

	void renderFilledSurface(float r, float g, float b) {
		sgl_begin_triangles();

		sgl_c3f(r, g, b);

		//simple dot product lighting
		for(int i=1; i<terrain.num_x; i++) {
			for(int k=1; k<terrain.num_z; k++) {
				const auto& v0=terrain.grid[terrain.ix(i-1, k-1)];
				const auto& v1=terrain.grid[terrain.ix(i, k-1)];
				const auto& v2=terrain.grid[terrain.ix(i-1, k)];
				const auto& v3=terrain.grid[terrain.ix(i, k)];

				vf3d n1=normalize(cross(v2-v0, v1-v0));
				vf3d c1=(v0+v2+v1)/3;
				vf3d ld1=normalize(cam.pos-c1);
				float dp1=std::max(.5f, dot(n1, ld1));

				sgl_c3f(dp1*r, dp1*g, dp1*b);
				sgl_v3f(v0.x, v0.y, v0.z);
				sgl_v3f(v2.x, v2.y, v2.z);
				sgl_v3f(v1.x, v1.y, v1.z);

				vf3d n2=normalize(cross(v2-v1, v3-v1));
				vf3d c2=(v1+v2+v3)/3;
				vf3d ld2=normalize(cam.pos-c2);
				float dp2=std::max(.3f, dot(n2, ld2));

				sgl_c3f(dp2*r, dp2*g, dp2*b);
				sgl_v3f(v1.x, v1.y, v1.z);
				sgl_v3f(v2.x, v2.y, v2.z);
				sgl_v3f(v3.x, v3.y, v3.z);
			}
		}

		sgl_end();
	}

	void renderImGui() {
		simgui_frame_desc_t simgui_frame_desc{};
		simgui_frame_desc.width=sapp_width();
		simgui_frame_desc.height=sapp_height();
		simgui_frame_desc.delta_time=sapp_frame_duration();
		simgui_frame_desc.dpi_scale=sapp_dpi_scale();
		simgui_new_frame(simgui_frame_desc);

		ImGui::Begin("sizing");
		terrain.to_update|=ImGui::SliderFloat("width", &terrain.width, 1, 25);
		terrain.to_update|=ImGui::SliderFloat("depth", &terrain.depth, 1, 25);
		terrain.to_update|=ImGui::SliderFloat("resolution", &terrain.resolution, .1f, 1);
		terrain.to_update|=ImGui::SliderFloat("y min", &terrain.y_min, -10, 10);
		terrain.to_update|=ImGui::SliderFloat("y max", &terrain.y_max, -10, 10);
		ImGui::End();

		ImGui::Begin("noise");
		terrain.to_update|=ImGui::SliderFloat("scale", &terrain.scl_noise, 0, 1);
		terrain.to_update|=ImGui::DragFloat("offset x", &terrain.x_noise_offset);
		terrain.to_update|=ImGui::DragFloat("offset y", &terrain.z_noise_offset);
		terrain.to_update|=ImGui::DragFloat("noise y", &terrain.y_noise, .1f);
		terrain.to_update|=ImGui::SliderInt("octaves", &terrain.octaves, 1, 12);
		ImGui::End();

		ImGui::Begin("graphics");
		ImGui::Checkbox("show base", &terrain.show_base);
		ImGui::Checkbox("show stems", &terrain.show_stems);
		ImGui::Checkbox("fill surface", &terrain.fill_surface);
		ImGui::End();

		simgui_render();
	}
#pragma endregion

	bool user_render() override {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={0, 0, 0, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_projection();
		sgl_load_matrix(cam.proj.m);
		sgl_matrix_mode_modelview();
		sgl_load_matrix(cam.view.m);

		//cyan
		if(terrain.show_base) renderBase(0, 1, 1);

		//yellow or magenta
		if(terrain.show_stems) renderStems(1, 1, 0, 1, 0, 1);

		//white
		if(terrain.fill_surface) renderFilledSurface(1, 1, 1);
		else renderOutlinedSurface(1, 1, 1);

		sgl_draw();

		renderImGui();

		sg_end_pass();

		sg_commit();

		return true;
	}
};