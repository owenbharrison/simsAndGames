/*TODO
make bounding volume hierarchy
	to speed up voxelization

place stud models

textured models to find color

color palatte lookup

api prices for bill of materials

instructions
*/
#define SOKOL_IMPL
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/include/sokol_app.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"
#include "sokol/include/sokol_gl.h"

#include "sokol/sokol_engine.h"

#include "mesh.h"

#include "voxel_set.h"

#include "prism.h"

#include "conversions.h"

//for time
#include <ctime>

#include "imgui/include/imgui_singleheader.h"
#include "sokol/include/sokol_imgui.h"

using cmn::vf3d;
using cmn::mat4;

class Slicer : public cmn::SokolEngine {
	//scene stuff
	//prusa mk4 build volume(250x210x220mm)
	cmn::AABBf3 build_volume{
		{-125, -105, -110},
		{125, 105, 110}
	};
	Mesh model;

	//default to lego stud width
	vf3d resolution{7.8f, 9.6f, 7.8f};

	VoxelSet voxels;
	std::vector<Prism> prisms;

	//user input
	float mouse_x=0, mouse_y=0;
	float mouse_px=0, mouse_py=0;

	struct {
		vf3d pos{5, 4, 6};
		float yaw=0, pitch=0;
		vf3d dir;

		mat4 proj;
		mat4 view;
	} cam;

	bool imguiing=false;

	//graphics
	sgl_pipeline pip{};

	int min_layer=0;
	int max_layer=0;
	bool show_build_volume=true;
	bool show_grid=true;
	bool show_edges=false;

public:
	bool setupModel() {
		//load model
		if(!Mesh::loadFromOBJ(model, "assets/benchy.txt")) return false;

		//scale model into build volume
		const cmn::AABBf3 model_box=model.getLocalAABB();
		vf3d model_dims=model_box.max-model_box.min;

		//find limiting dimension
		vf3d build_volume_dims=build_volume.max-build_volume.min;
		vf3d num=build_volume_dims/model_dims;
		float scl=std::min(num.x, std::min(num.y, num.z));
		model.scale={scl, scl, scl};

		//center model on bed
		vf3d model_ctr=model_box.getCenter();
		vf3d build_volume_ctr=build_volume.getCenter();
		model.trans.x=build_volume_ctr.x-scl*model_ctr.x;
		model.trans.y=build_volume.min.y-scl*model_box.min.y;
		model.trans.z=build_volume_ctr.z-scl*model_ctr.z;

		model.updateMatrixes();

		return true;
	}

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
		sgl_desc.max_commands=500000;
		sgl_desc.max_vertices=500000;
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
		app_title="Slicer";

		std::srand(std::time(0));

		if(!setupModel()) return false;

		handleResliceAction();

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
		if(imguiing) return;
		
		//orbit
		if(GetMouse(SAPP_MOUSEBUTTON_LEFT).held) {
			float mouse_dx=mouse_x-mouse_px;
			float mouse_dy=mouse_y-mouse_py;
			float speed=.7f*dt;
			cam.yaw+=mouse_dx*speed;
			cam.pitch-=mouse_dy*speed;
		}

		//look left, right
		if(GetKey(SAPP_KEYCODE_LEFT).held) cam.yaw+=dt;
		if(GetKey(SAPP_KEYCODE_RIGHT).held) cam.yaw-=dt;

		//look up, down
		if(GetKey(SAPP_KEYCODE_UP).held) cam.pitch-=dt;
		if(GetKey(SAPP_KEYCODE_DOWN).held) cam.pitch+=dt;

		cam.pitch=cmn::clamp(cam.pitch, .001f-cmn::Pi/2, cmn::Pi/2-.001f);
	}

	//dynamic camera system :D
	void handleCameraPlacement() {
		//find sphere to envelope bounds
		vf3d ctr=build_volume.getCenter();
		float st_dist=1+length(build_volume.max-ctr);

		//step backwards along cam ray
		vf3d st=ctr-st_dist*cam.dir;

		//snap pt to bounds & make relative to origin
		float rad=st_dist-build_volume.intersectRay(st, cam.dir);

		//push cam back by some margin
		cam.pos=-(110+rad)*cam.dir;
	}

	void handleResliceAction() {
		//voxellize mesh
		voxels=meshToVoxels(model, resolution);

		//"prismize" voxels
		prisms=voxelsToPrisms(voxels);

		//reset layer range
		min_layer=0;
		max_layer=voxels.getHeight()-1;
	}

	void updateCameraMatrixes() {
		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(90, asp, 1, 1000);
	}
#pragma endregion

	bool user_update(float dt) override {
		mouse_px=mouse_x;
		mouse_py=mouse_y;
		mouse_x=GetMouseX();
		mouse_y=GetMouseY();

		handleCameraLooking(dt);

		cam.dir=vf3d::polar({1, cam.yaw, cam.pitch});

		handleCameraPlacement();

		updateCameraMatrixes();

		return true;
	}

#pragma region RENDER_HELPERS
	//this looks nice :D
	void renderArrow(
		const vf3d& s0, const vf3d& s1,
		float t,
		float r, float g, float b
	) {
		vf3d s01=s1-s0;
		float mag=length(s01);
		vf3d s1p=cam.pos-s1;
		vf3d norm=normalize(cross(s01, s1p));
		vf3d perp=.5f*t*mag*norm;
		vf3d mid=s1-t*s01;
		vf3d lft=mid-perp;
		vf3d rgt=mid+perp;

		sgl_begin_line_strip();

		sgl_c3f(r, g, b);

		sgl_v3f(s0.x, s0.y, s0.z);
		sgl_v3f(mid.x, mid.y, mid.z);
		sgl_v3f(lft.x, lft.y, lft.z);
		sgl_v3f(s1.x, s1.y, s1.z);
		sgl_v3f(rgt.x, rgt.y, rgt.z);
		sgl_v3f(mid.x, mid.y, mid.z);

		sgl_end();
	}

	//billboarded-ish
	void renderThickLine(
		const vf3d& v0, const vf3d& v1,
		float t,
		float r, float g, float b
	) {
		vf3d v01=v1-v0;
		vf3d m=(v0+v1)/2;
		vf3d mp=cam.pos-m;
		vf3d norm=normalize(cross(v01, mp));
		vf3d rgt=.5f*t*norm;
		vf3d l0=v0-rgt, r0=v0+rgt;
		vf3d l1=v1-rgt, r1=v1+rgt;

		sgl_begin_quads();

		sgl_c3f(r, g, b);

		sgl_v3f(l0.x, l0.y, l0.z);
		sgl_v3f(r0.x, r0.y, r0.z);
		sgl_v3f(r1.x, r1.y, r1.z);
		sgl_v3f(l1.x, l1.y, l1.z);

		sgl_end();
	}

	void renderFilledAABB(const cmn::AABBf3& box, float r, float g, float b) {
		static constexpr int tris[12][3]{
			{1, 3, 5}, {5, 3, 7},//+x
			{3, 2, 7}, {7, 2, 6},//+y
			{5, 7, 4}, {4, 7, 6},//+z
			{4, 6, 0}, {0, 6, 2},//-x
			{0, 1, 4}, {4, 1, 5},//-y
			{0, 2, 1}, {1, 2, 3}//-z
		};
		const vf3d verts[8]{
			{box.min.x, box.min.y, box.min.z},
			{box.max.x, box.min.y, box.min.z},
			{box.min.x, box.max.y, box.min.z},
			{box.max.x, box.max.y, box.min.z},
			{box.min.x, box.min.y, box.max.z},
			{box.max.x, box.min.y, box.max.z},
			{box.min.x, box.max.y, box.max.z},
			{box.max.x, box.max.y, box.max.z}
		};

		sgl_begin_triangles();

		sgl_c3f(r, g, b);

		for(int i=0; i<12; i++) {
			const auto& t0=verts[tris[i][0]];
			const auto& t1=verts[tris[i][1]];
			const auto& t2=verts[tris[i][2]];

			vf3d t01=t1-t0, t02=t2-t0;
			vf3d norm=normalize(cross(t01, t02));
			vf3d ctr=(t0+t1+t2)/3;

			//simple shading
			vf3d light_dir=normalize(cam.pos-ctr);//light=cam
			float dp=cmn::clamp(dot(norm, light_dir), .5f, 1.f);
			sgl_c3f(dp*r, dp*g, dp*b);

			sgl_v3f(t0.x, t0.y, t0.z);
			sgl_v3f(t1.x, t1.y, t1.z);
			sgl_v3f(t2.x, t2.y, t2.z);
		}

		sgl_end();
	}

	void renderOutlinedAABB(const cmn::AABBf3& box, float r, float g, float b) {
		static constexpr int lines[12][2]{
			{0, 1}, {1, 3}, {3, 2}, {2, 0},
			{0, 4}, {1, 5}, {2, 6}, {3, 7},
			{4, 5}, {5, 7}, {7, 6}, {6, 4}
		};
		const vf3d verts[8]{
			{box.min.x, box.min.y, box.min.z},
			{box.max.x, box.min.y, box.min.z},
			{box.min.x, box.max.y, box.min.z},
			{box.max.x, box.max.y, box.min.z},
			{box.min.x, box.min.y, box.max.z},
			{box.max.x, box.min.y, box.max.z},
			{box.min.x, box.max.y, box.max.z},
			{box.max.x, box.max.y, box.max.z}
		};
		sgl_begin_lines();

		sgl_c3f(r, g, b);

		for(int i=0; i<12; i++) {
			for(int j=0; j<2; j++) {
				const auto& v=verts[lines[i][j]];
				sgl_v3f(v.x, v.y, v.z);
			}
		}

		sgl_end();
	}

	void renderBuildVolume(float t, float r, float g, float b) {
		static constexpr int lines[12][2]{
			{0, 1}, {1, 3}, {3, 2}, {2, 0},
			{0, 4}, {1, 5}, {2, 6}, {3, 7},
			{4, 5}, {5, 7}, {7, 6}, {6, 4}
		};
		const vf3d verts[8]{
			{build_volume.min.x, build_volume.min.y, build_volume.min.z},
			{build_volume.max.x, build_volume.min.y, build_volume.min.z},
			{build_volume.min.x, build_volume.max.y, build_volume.min.z},
			{build_volume.max.x, build_volume.max.y, build_volume.min.z},
			{build_volume.min.x, build_volume.min.y, build_volume.max.z},
			{build_volume.max.x, build_volume.min.y, build_volume.max.z},
			{build_volume.min.x, build_volume.max.y, build_volume.max.z},
			{build_volume.max.x, build_volume.max.y, build_volume.max.z}
		};

		for(int i=0; i<12; i++) {
			renderThickLine(
				verts[lines[i][0]], verts[lines[i][1]],
				t, r, g, b
			);
		}
	}

	void renderGrid() {
		const auto& min=build_volume.min;
		const auto& max=build_volume.max;

		const vf3d dims=max-min;
		const int num_x=1+dims.x/resolution.x;
		const int num_y=1+dims.y/resolution.y;
		const int num_z=1+dims.z/resolution.z;

		const vf3d ctr=build_volume.getCenter();
		const vf3d view=cam.pos-ctr;
		const vf3d plane(
			view.x>0?min.x:max.x,
			view.y>0?min.y:max.y,
			view.z>0?min.z:max.z
		);

		sgl_begin_lines();

		//draw x lines
		sgl_c3f(1, 0, 0);
		for(int j=1; j<num_y; j++) {
			float y=min.y+resolution.y*j;
			sgl_v3f(min.x, y, plane.z);
			sgl_v3f(max.x, y, plane.z);
		}
		for(int k=1; k<num_z; k++) {
			float z=min.z+resolution.z*k;
			sgl_v3f(min.x, plane.y, z);
			sgl_v3f(max.x, plane.y, z);
		}

		//draw y lines
		sgl_c3f(0, 1, 0);
		for(int k=1; k<num_z; k++) {
			float z=min.z+resolution.z*k;
			sgl_v3f(plane.x, min.y, z);
			sgl_v3f(plane.x, max.y, z);
		}
		for(int i=1; i<num_x; i++) {
			float x=min.x+resolution.x*i;
			sgl_v3f(x, min.y, plane.z);
			sgl_v3f(x, max.y, plane.z);
		}

		//draw z lines
		sgl_c3f(0, 0, 1);
		for(int i=1; i<num_x; i++) {
			float x=min.x+resolution.x*i;
			sgl_v3f(x, plane.y, min.z);
			sgl_v3f(x, plane.y, max.z);
		}
		for(int j=1; j<num_y; j++) {
			float y=min.y+resolution.y*j;
			sgl_v3f(plane.x, y, min.z);
			sgl_v3f(plane.x, y, max.z);
		}

		sgl_end();
	}

	void renderAxes(const vf3d& pos, float sz) {
		renderArrow(pos, pos+vf3d(sz, 0, 0), .2f, 1, 0, 0);
		renderArrow(pos, pos+vf3d(0, sz, 0), .2f, 0, 1, 0);
		renderArrow(pos, pos+vf3d(0, 0, sz), .2f, 0, 0, 1);
	}

	void renderPrisms() {
		for(const auto& p:prisms) {
			//skip in not in layer range
			if(p.y<min_layer) continue;
			if(p.y>max_layer) continue;

			//scale to correct position
			vf3d pos=voxels.trans+voxels.scale*vf3d(p.x, p.y, p.z);
			vf3d size=voxels.scale*vf3d(p.w, 1, p.d);

			//shrink if showing edges
			float margin=show_edges?.25f:0;
			renderFilledAABB({pos+margin, pos+size-margin}, 1, 1, 1);
			//color aabb based on "efficiency"
			if(show_edges) {
				//green=good, yellow=fine, red=bad
				float r=1, g=.25f, b=.25f;
				switch((p.w>1)+(p.d>1)) {
					case 1: r=1, g=1, b=.25f; break;
					case 2: r=.25f, g=1, b=.25f; break;
				}
				renderOutlinedAABB({pos, pos+size}, r, g, b);
			}
		}
	}

	void renderImGui() {
		simgui_frame_desc_t simgui_frame_desc{};
		simgui_frame_desc.width=sapp_width();
		simgui_frame_desc.height=sapp_height();
		simgui_frame_desc.delta_time=sapp_frame_duration();
		simgui_frame_desc.dpi_scale=sapp_dpi_scale();
		simgui_new_frame(simgui_frame_desc);

		imguiing=ImGui::GetIO().WantCaptureMouse;

		ImGui::Begin("Slicing Options");
		ImGui::Text("Block Resolution(mm)");
		ImGui::SliderFloat("X", &resolution.x, 4, 15.6f);
		ImGui::SliderFloat("Y", &resolution.y, 4, 15.6f);
		ImGui::SliderFloat("Z", &resolution.z, 4, 15.6f);
		if(ImGui::Button("Reslice")) handleResliceAction();
		ImGui::SameLine();
		ImGui::Text("(note: may take awhile)");
		if(min_layer>max_layer) std::swap(min_layer, max_layer);
		ImGui::End();

		ImGui::Begin("Graphics Options");
		ImGui::Text("Layer Range");
		ImGui::SliderInt("Min", &min_layer, 0, voxels.getHeight()-1);
		ImGui::SliderInt("Max", &max_layer, 0, voxels.getHeight()-1);
		ImGui::Checkbox("Show Build Volume", &show_build_volume);
		ImGui::Checkbox("Show Grid", &show_grid);
		ImGui::Checkbox("Show Edges", &show_edges);
		if(min_layer>max_layer) std::swap(min_layer, max_layer);
		ImGui::End();

		simgui_render();
	}
#pragma endregion

	bool user_render() override {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={.08f, .08f, .08f, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_projection();
		sgl_load_matrix(cam.proj.m);
		sgl_matrix_mode_modelview();
		sgl_load_matrix(cam.view.m);

		if(show_build_volume) renderBuildVolume(1, 1, 1, 1);

		if(show_grid) renderGrid();

		//add axes at build volume corner
		renderAxes(5.f+build_volume.min, 25);

		renderPrisms();

		sgl_draw();

		renderImGui();

		sg_end_pass();

		sg_commit();

		return true;
	}
};