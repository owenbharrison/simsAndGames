#define SOKOL_IMPL
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "vendor/sokol/sokol_app.h"
#include "vendor/sokol/sokol_gfx.h"
#include "vendor/sokol/sokol_glue.h"
#include "vendor/sokol/sokol_gl.h"

#include "common/sokol/sokol_engine.h"

#include "common/utils.h"

#include <list>

#include "object.h"

void grassGradient(float t, float* r, float* g, float* b) {
	static const float cols[][3]{
		{19/255.f, 109/255.f, 21/255.f},//green
		{97/255.f, 52/255.f, 8/255.f},//brown
		{156/255.f, 156/255.f, 156/255.f}//grey
	};
	static const int num=sizeof(cols)/sizeof(*cols);
	cmn::colorGradient(
		cols, num, t,
		r, g, b
	);
}

using cmn::vf3d;
using cmn::mat4;

class AStarNav : public cmn::SokolEngine {
	//scene
	struct {
		//somewhat roundabout...
		Mesh mesh;
		Object obj;
	} terrain;

	struct {
		std::list<Mesh> meshes;
		std::vector<Object> houses;
	} houses;
	
	//user input
	struct {
		vf3d pos{1.67f, 1.5f, 2.37f};
		float yaw=0, pitch=0;
		vf3d dir;
		mat4 view;

		float fov_deg=75;
		float near_plane=.01f, far_plane=100;
		mat4 proj;
	} cam;

	//graphics
	sgl_pipeline pip{};
	
	vf3d sun_pos{10, 10, 10};

public:
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

	bool setupTerrain() {
		if(!Mesh::loadFromOBJ(
			terrain.mesh,
			"assets/models/terrain.obj.txt"
		)) return false;

		terrain.obj.mesh=&terrain.mesh;

		return true;
	}

	bool setupHouses() {
		
		return true;
	}

	bool user_create() override {
		app_title="A* Nav";

		std::srand(std::time(0));

		setupSGL();

		lookAtOrigin();
		
		setupHouses();

		setupPipeline();


		return true;
	}

#pragma region UPDATE_HELPERS
	void lookAtOrigin() {
		vf3d ryp=vf3d::cartesian(-cam.pos);
		cam.yaw=ryp.y;
		cam.pitch=ryp.z;
	}

	void handleCameraLooking(float dt) {
		//look up, down
		if(GetKey(SAPP_KEYCODE_UP).held) cam.pitch+=dt;
		if(GetKey(SAPP_KEYCODE_DOWN).held) cam.pitch-=dt;
		const float min_pitch=.001f-cmn::Pi/2;
		const float max_pitch=cmn::Pi/2-.001f;
		if(cam.pitch<min_pitch) cam.pitch=min_pitch;
		if(cam.pitch>max_pitch) cam.pitch=max_pitch;

		//look left, right
		if(GetKey(SAPP_KEYCODE_LEFT).held) cam.yaw-=dt;
		if(GetKey(SAPP_KEYCODE_RIGHT).held) cam.yaw+=dt;
	}

	void handleCameraMovement(float dt) {
		//move up, down
		if(GetKey(SAPP_KEYCODE_SPACE).held) cam.pos.y+=4.f*dt;
		if(GetKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam.pos.y-=4.f*dt;

		//move forward, backward
		vf3d fwd=normalize(vf3d(1, 0, 1)*cam.dir);
		if(GetKey(SAPP_KEYCODE_W).held) cam.pos+=5.f*dt*fwd;
		if(GetKey(SAPP_KEYCODE_S).held) cam.pos-=3.f*dt*fwd;

		//move left, right
		vf3d rgt=cross(fwd, {0, 1, 0});
		if(GetKey(SAPP_KEYCODE_A).held) cam.pos-=4.f*dt*rgt;
		if(GetKey(SAPP_KEYCODE_D).held) cam.pos+=4.f*dt*rgt;
	}

	void handleUserInput(float dt) {
		handleCameraMovement(dt);

		handleCameraLooking(dt);
	}

	void updateCamera() {
		cam.dir=vf3d::polar({1, cam.yaw, cam.pitch});

		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		cam.view=mat4::inverse(look_at);

		cam.proj=mat4::makePerspective(
			cam.fov_deg,
			sapp_widthf()/sapp_heightf(),
			cam.near_plane,
			cam.far_plane
		);
	}
#pragma endregion

	bool user_update(float dt) override {
		handleUserInput(dt);

		updateCamera();

		return true;
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
			sgl_v3f(v[e[0]].x, v[e[0]].y, v[e[0]].z);
			sgl_v3f(v[e[1]].x, v[e[1]].y, v[e[1]].z);
		}
		sgl_end();
	}

	//shaded tri helper
	void sgl_tri(
		const vf3d& pa, const vf3d& pb, const vf3d& pc,
		float r, float g, float b
	) {
		vf3d norm=normalize(cross(pb-pa, pc-pa));
		vf3d ctr=(pa+pb+pc)/3;
		vf3d sun_dir=normalize(sun_pos-ctr);
		float dp=std::max(.3f, dot(sun_dir, norm));
		sgl_c3f(dp*r, dp*g, dp*b);
		sgl_v3f(pa.x, pa.y, pa.z);
		sgl_v3f(pb.x, pb.y, pb.z);
		sgl_v3f(pc.x, pc.y, pc.z);
	}

	//custom grass coloring
	void renderTerrain() {
		const auto& m=*terrain.obj.mesh;
		std::vector<vf3d>
		sgl_begin_triangles();
		for(const auto& t:m.tris) {
			const auto& a=m.vertexes[t.a];
			const auto& b=m.vertexes[t.a];
			const auto& c=m.vertexes[t.a];
			float t
			sgl_tri(
				m.vertexes[t.a],
				m.vertexes[t.b],
				m.vertexes[t.c],
				o.rgb[0], o.rgb[1], o.rgb[2]
			);
		}
		sgl_end();
	}

	void renderObject(const Object& o) {
		const auto& m=*o.mesh;
		std::vector<vf3d> vertexes;
		for()
		sgl_begin_triangles();
		for(const auto& t:m.tris) {
			sgl_tri(
				m.vertexes[t.a],
				m.vertexes[t.b],
				m.vertexes[t.c],
				o.rgb[0], o.rgb[1], o.rgb[2]
			);
		}
		sgl_end();
	}

	bool user_render() override {
		sg_pass pass{};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_projection();
		sgl_load_matrix(cam.proj.m);
		sgl_matrix_mode_modelview();
		sgl_load_matrix(cam.view.m);

		sgl_draw();

		sg_end_pass();

		sg_commit();

		return true;
	}

	void user_destroy() override {
		sgl_shutdown();
	}
};