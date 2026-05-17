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

#include "cmn/utils.h"

//for time
#include <ctime>

using cmn::vf3d;
using cmn::mat4;

//y p => x y z
//0 0 => 0 0 1
static cmn::vf3d polarToCartesian(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

//x y z => y p
//0 0 1 => 0 0
static void cartesianToPolar(const cmn::vf3d& pt, float* yaw, float* pitch) {
	//flatten onto xz
	*yaw=std::atan2(pt.x, pt.z);
	//vertical triangle
	*pitch=std::atan2(pt.y, std::sqrt(pt.x*pt.x+pt.z*pt.z));
}

class Fracture : public cmn::SokolEngine {
	//scene
	std::vector<Mesh> meshes;
	int mesh_ix=0;
	
	struct {
		vf3d ctr, norm;

		bool to_spin=true;
		vf3d rot;
	} plane;
	
	//user input
	struct {
		vf3d pos{-1, 1, 2.5f};
		float yaw=0, pitch=0;
		vf3d dir;

		float fov_deg=75;

		const float near_plane=.001f, far_plane=1000;
		mat4 proj;

		mat4 view;

		mat4 view_proj;
	} cam;

	//graphics
	sgl_pipeline pip{};
	
	bool offset_meshes=true;
	bool show_bounds=false;
	bool fill_triangles=true;

public:
#pragma region CREATE_HELPERS
	bool setupMeshes() {
		const std::vector<std::string> filenames{
			"assets/armadillo.txt",
			"assets/bunny.txt",
			"assets/cow.txt",
			"assets/dragon.txt",
			"assets/horse.txt",
			"assets/jeep.txt",
			"assets/monkey.txt"
		};
		for(const auto& f:filenames) {
			Mesh m;
			if(!Mesh::loadFromOBJ(m, f)) return false;
			meshes.push_back(m);
		}

		return true;
	}

	void randomizeMesh() {
		mesh_ix=std::rand()%meshes.size();
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
#pragma endregion

	bool user_create() override {
		app_title="Fracture";
		
		std::srand(std::time(0));

		if(!setupMeshes()) return false;

		randomizeMesh();

		cartesianToPolar(-cam.pos, &cam.yaw, &cam.pitch);

		setupSGL();

		setupPipeline();

		return true;
	}

#pragma region UPDATE_HELPERS
	void handleCameraLooking(float dt) {
		//look up, down
		if(GetKey(SAPP_KEYCODE_UP).held) cam.pitch+=dt;
		if(GetKey(SAPP_KEYCODE_DOWN).held) cam.pitch-=dt;
		cam.pitch=cmn::clamp(cam.pitch, .001f-cmn::Pi/2, cmn::Pi/2-.001f);

		//look left, right
		if(GetKey(SAPP_KEYCODE_LEFT).held) cam.yaw+=dt;
		if(GetKey(SAPP_KEYCODE_RIGHT).held) cam.yaw-=dt;
	}

	void handleCameraMovement(float dt) {
		//move up, down
		if(GetKey(SAPP_KEYCODE_SPACE).held) cam.pos.y+=4.f*dt;
		if(GetKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam.pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::sin(cam.yaw), 0, std::cos(cam.yaw));
		if(GetKey(SAPP_KEYCODE_W).held) cam.pos+=5.f*dt*fb_dir;
		if(GetKey(SAPP_KEYCODE_S).held) cam.pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(SAPP_KEYCODE_A).held) cam.pos+=4.f*dt*lr_dir;
		if(GetKey(SAPP_KEYCODE_D).held) cam.pos-=4.f*dt*lr_dir;
	}

	void updateCameraMatrixes() {
		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		//cam proj can change with window resize
		float asp=sapp_widthf()/sapp_heightf();
		cam.proj=mat4::makePerspective(cam.fov_deg, asp, cam.near_plane, cam.far_plane);

		cam.view_proj=mat4::mul(cam.proj, cam.view);
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		cam.dir=polarToCartesian(cam.yaw, cam.pitch);

		handleCameraMovement(dt);

		updateCameraMatrixes();

		//debug toggles
		if(GetKey(SAPP_KEYCODE_ENTER).pressed) plane.to_spin^=true;
		if(GetKey(SAPP_KEYCODE_O).pressed) offset_meshes^=true;
		if(GetKey(SAPP_KEYCODE_B).pressed) show_bounds^=true;
		if(GetKey(SAPP_KEYCODE_F).pressed) fill_triangles^=true;
		if(GetKey(SAPP_KEYCODE_R).pressed) randomizeMesh();
	}
#pragma endregion

	bool user_update(float dt) override {
		handleUserInput(dt);

		if(plane.to_spin) plane.rot+=dt*vf3d(.3f, .2f, .4f);

		//update split plane
		mat4 rot_x=mat4::makeRotX(plane.rot.x);
		mat4 rot_y=mat4::makeRotY(plane.rot.y);
		mat4 rot_z=mat4::makeRotZ(plane.rot.z);
		mat4 rot_xyz=mat4::mul(rot_z, mat4::mul(rot_y, rot_x));
		float w=1;
		plane.norm=matMulVec(rot_xyz, {0, 1, 0}, w);

		return true;
	}

#pragma region RENDER_HELPERS
	void renderMesh(const Mesh& m) {
		if(fill_triangles) {
			sgl_begin_triangles();

			for(const auto& t:m.tris) {
				const auto& a=m.verts[t.ix[0]];
				const auto& b=m.verts[t.ix[1]];
				const auto& c=m.verts[t.ix[2]];

				vf3d ab=b-a, ac=c-a;
				vf3d norm=normalize(cross(ab, ac));
				vf3d ctr=(a+b+c)/3;

				//if pointing away from cam, use flipped normal
				if(norm.dot(cam.pos-ctr)<0) norm*=-1;

				//simple shading
				vf3d light_dir=(cam.pos-ctr).norm();//light=cam
				float dp=cmn::clamp(norm.dot(light_dir), .5f, 1.f);
				sgl_c3f(dp*t.r, dp*t.g, dp*t.b);

				sgl_v3f(a.x, a.y, a.z);
				sgl_v3f(b.x, b.y, b.z);
				sgl_v3f(c.x, c.y, c.z);
			}

			sgl_end();
		} else {
			sgl_begin_lines();

			for(const auto& t:m.tris) {
				sgl_c3f(t.r, t.g, t.b);

				const auto& a=m.verts[t.ix[0]];
				const auto& b=m.verts[t.ix[1]];
				const auto& c=m.verts[t.ix[2]];

				sgl_v3f(a.x, a.y, a.z), sgl_v3f(b.x, b.y, b.z);
				sgl_v3f(b.x, b.y, b.z), sgl_v3f(c.x, c.y, c.z);
				sgl_v3f(c.x, c.y, c.z), sgl_v3f(a.x, a.y, a.z);
			}

			sgl_end();
		}
	}

	void renderBox(
		const vf3d& min, const vf3d& max,
		float r, float g, float b
	) {
		sgl_begin_lines();

		sgl_c3f(r, g, b);

		//xy-
		sgl_v3f(min.x, min.y, min.z), sgl_v3f(max.x, min.y, min.z);
		sgl_v3f(max.x, min.y, min.z), sgl_v3f(max.x, max.y, min.z);
		sgl_v3f(max.x, max.y, min.z), sgl_v3f(min.x, max.y, min.z);
		sgl_v3f(min.x, max.y, min.z), sgl_v3f(min.x, min.y, min.z);
		//thru z
		sgl_v3f(min.x, min.y, min.z), sgl_v3f(min.x, min.y, max.z);
		sgl_v3f(max.x, min.y, min.z), sgl_v3f(max.x, min.y, max.z);
		sgl_v3f(min.x, max.y, min.z), sgl_v3f(min.x, max.y, max.z);
		sgl_v3f(max.x, max.y, min.z), sgl_v3f(max.x, max.y, max.z);
		//xy+
		sgl_v3f(min.x, min.y, max.z), sgl_v3f(max.x, min.y, max.z);
		sgl_v3f(max.x, min.y, max.z), sgl_v3f(max.x, max.y, max.z);
		sgl_v3f(max.x, max.y, max.z), sgl_v3f(min.x, max.y, max.z);
		sgl_v3f(min.x, max.y, max.z), sgl_v3f(min.x, min.y, max.z);

		sgl_end();
	}

	//get bounds of mesh vertexes
	void renderBounds(const Mesh& m, float r, float g, float b) {
		vf3d min{1e30f, 1e30f, 1e30f};
		vf3d max{-1e30f, -1e30f, -1e30f};
		for(const auto& v:m.verts) {
			if(v.x<min.x) min.x=v.x;
			if(v.y<min.y) min.y=v.y;
			if(v.z<min.z) min.z=v.z;
			if(v.x>max.x) max.x=v.x;
			if(v.y>max.y) max.y=v.y;
			if(v.z>max.z) max.z=v.z;
		}
		renderBox(min, max, r, g, b);
	}

	//make simple coordinate system w/ up & norm
	void renderPlane(float r, float g, float b) {
		const vf3d up(0, 1, 0);
		vf3d va=plane.norm.cross(up).norm();
		vf3d vb=plane.norm.cross(va);
		vf3d v0=plane.ctr-va-vb, v1=plane.ctr-va+vb;
		vf3d v2=plane.ctr+va-vb, v3=plane.ctr+va+vb;

		sgl_begin_line_strip();

		sgl_c3f(r, g, b);

		sgl_v3f(v0.x, v0.y, v0.z);
		sgl_v3f(v1.x, v1.y, v1.z);
		sgl_v3f(v3.x, v3.y, v3.z);
		sgl_v3f(v0.x, v0.y, v0.z);
		sgl_v3f(v2.x, v2.y, v2.z);
		sgl_v3f(v3.x, v3.y, v3.z);

		sgl_end();
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

		//split meshes & offset by plane
		Mesh ahead, behind;
		if(meshes[mesh_ix].splitByPlane(plane.ctr, plane.norm, ahead, behind)) {
			if(offset_meshes) {
				vf3d offset=.075f*plane.norm;
				for(auto& v:ahead.verts) v+=offset;
				for(auto& v:behind.verts) v-=offset;
			}

			renderMesh(ahead);
			renderMesh(behind);

			if(show_bounds) {
				//orange
				renderBounds(ahead, 1, .5f, 0);
				//purple
				renderBounds(behind, .5f, 0, 1);
			}
		}

		//red
		renderPlane(1, 0, 0);

		sgl_draw();

		sg_end_pass();

		sg_commit();

		return true;
	}
};