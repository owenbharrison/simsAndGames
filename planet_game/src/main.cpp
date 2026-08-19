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

#include "cmn/math/v3d.h"

#include "icosphere.h"

#include "cmn/utils.h"

#include "imgui/include/imgui_singleheader.h"
#include "sokol/include/sokol_imgui.h"

using cmn::vf3d;

class PlanetGame : public cmn::SokolEngine {
	vf3d light_pos;

	struct {
		vf3d pos;
		vf3d dir;
		vf3d up{0, 1, 0};
	} cam;

	struct {
		float yaw=0;
		float pitch=0;
		vf3d pos;

		float dist;
	} third_person;

	struct {
		vf3d pos;
		const float rad=2;
	} planet;
	
	struct {
		vf3d pos;
		vf3d fwd, rgt, up;
		float pitch=0;

		vf3d look;
		const float sz=.1f;

		bool use_perspective=false;
		
		bool show_axes=false;
	} player;

	sgl_pipeline pip{};

#pragma region SETUP HELPERS
	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_setup(sgl_desc);
	}

	void setupImGui() {
		simgui_desc_t simgui_desc{};
		simgui_desc.ini_filename="assets/imgui.ini";
		simgui_setup(simgui_desc);
	}

	void setupPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.depth.write_enabled=true;
		pip_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pip=sgl_make_pipeline(pip_desc);
	}
#pragma endregion

public:
	bool user_create() override {
		app_title="Planet Game";
		
		setupSGL();
		
		light_pos=(3+planet.rad)*vf3d(0, 1, 1);

		{
			third_person.dist=3+planet.rad;
			third_person.pos=third_person.dist*normalize(vf3d(1, 1, 1));
			vf3d ryp=vf3d::cartesian(third_person.pos);
			third_person.yaw=ryp.y;
			third_person.pitch=ryp.z;
		}

		player.pos={0, planet.rad, 0};
		player.fwd={1, 0, 0};

		setupImGui();

		setupPipeline();

		return true;
	}

#pragma region UPDATE HELPERS
	//unintuitive
	void handleCameraMovement(float dt) {
		static float pmousex=0, pmousey=0;
		float mousex=GetMouseX(), mousey=GetMouseY();
		float dx=mousex-pmousex, dy=mousey-pmousey;
		pmousex=mousex, pmousey=mousey;

		if(!GetMouse(SAPP_MOUSEBUTTON_LEFT).held) return;

		third_person.yaw+=dx*dt;
		third_person.pitch+=dy*dt;
		third_person.pitch=cmn::clamp(
			third_person.pitch,
			.001f-cmn::Pi/2,
			cmn::Pi/2-.001f
		);

		third_person.pos=vf3d::polar({
			third_person.dist,
			third_person.yaw,
			third_person.pitch
			});
	}

	void handlePlayerMovement(float dt) {
		player.up=normalize(player.pos-planet.pos);
		player.rgt=normalize(cross(player.fwd, player.up));

		//walking forward & back
		const float walk_speed=1;
		{
			bool walk_fwd=GetKey(SAPP_KEYCODE_W).held;
			bool walk_back=GetKey(SAPP_KEYCODE_S).held;
			if(walk_fwd^walk_back) {
				//slow backwards
				float fb_modifier=walk_fwd?1:-.75f;
				float walk_amt=walk_speed*fb_modifier*dt;

				//move by forward dir
				player.pos+=player.fwd*walk_amt;

				//get new up dir
				player.up=normalize(player.pos-planet.pos);

				//reproject pos onto sphere...
				player.pos=planet.pos+planet.rad*player.up;

				//get new fwd dir (since rgt didnt change)
				player.fwd=cross(player.up, player.rgt);
			}
		}

		//strafing left & right
		{
			const float strafe_speed=.6f*walk_speed;
			bool strafe_left=GetKey(SAPP_KEYCODE_A).held;
			bool strafe_right=GetKey(SAPP_KEYCODE_D).held;
			if(strafe_left^strafe_right) {
				int lr_modifier=strafe_right?1:-1;
				float strafe_amt=strafe_speed*lr_modifier*dt;

				//move by rgt dir
				player.pos+=player.rgt*strafe_amt;

				//get new up dir
				player.up=normalize(player.pos-planet.pos);

				//reproject pos onto sphere...
				player.pos=planet.pos+planet.rad*player.up;

				//get new rgt dir (since fwd didnt change)
				player.rgt=cross(player.fwd, player.up);
			}
		}

		//turning left & right
		{
			const float turn_speed=2;
			bool turn_left=GetKey(SAPP_KEYCODE_LEFT).held;
			bool turn_right=GetKey(SAPP_KEYCODE_RIGHT).held;
			if(turn_right^turn_left) {
				int lr_modifier=turn_right?1:-1;
				float turn_amt=turn_speed*lr_modifier*dt;

				//get new fwd/rgt dirs (since up doesnt change)
				vf3d fwd_new=std::cos(turn_amt)*player.fwd+std::sin(turn_amt)*player.rgt;
				vf3d rgt_new=-std::sin(turn_amt)*player.fwd+std::cos(turn_amt)*player.rgt;
				player.fwd=fwd_new;
				player.rgt=rgt_new;
			}
		}

		//looking up & down
		{
			const float look_speed=2;
			bool look_up=GetKey(SAPP_KEYCODE_UP).held;
			bool look_down=GetKey(SAPP_KEYCODE_DOWN).held;
			if(look_up^look_down) {
				int ud_modifier=look_up?1:-1;
				float look_amt=look_speed*ud_modifier*dt;

				//get new pitch
				player.pitch+=look_amt;
				if(player.pitch<-cmn::Pi/2) player.pitch=.001f-cmn::Pi/2;
				if(player.pitch>cmn::Pi/2) player.pitch=cmn::Pi/2-.001f;
			}
		}

		//get new look dir
		player.look=std::cos(player.pitch)*player.fwd+std::sin(player.pitch)*player.up;
	}
#pragma endregion

	bool user_update(float dt) override {
		handleCameraMovement(dt);

		handlePlayerMovement(dt);

		if(player.use_perspective) {
			cam.pos=player.pos+player.sz*player.up;
			cam.dir=player.look;
			cam.up=player.up;
		} else {
			cam.pos=third_person.pos;
			cam.dir=normalize(planet.pos-third_person.pos);
			cam.up={0, 1, 0};
		}

		return true;
	}

#pragma region RENDER HELPERS
	//this looks nice :D
	void renderArrow(const vf3d& a, const vf3d& b, float sz) {
		vf3d ba=b-a;
		float mag=ba.mag();
		vf3d ca=cam.pos-a;
		vf3d norm=normalize(cross(ba, ca));
		vf3d perp=.5f*sz*mag*norm;
		vf3d c=b-sz*ba;
		vf3d l=c-perp, r=c+perp;
		sgl_begin_lines();
		sgl_v3f(a.x, a.y, a.z), sgl_v3f(c.x, c.y, c.z);
		sgl_v3f(l.x, l.y, l.z), sgl_v3f(r.x, r.y, r.z);
		sgl_v3f(l.x, l.y, l.z), sgl_v3f(b.x, b.y, b.z);
		sgl_v3f(r.x, r.y, r.z), sgl_v3f(b.x, b.y, b.z);
		sgl_end();
	}

	void renderSphere(const vf3d& pos, float rad, float r, float g, float b) {
		sgl_begin_triangles();
		for(const auto& t:icosphere::tris) {
			const auto& pa=icosphere::vertexes[t[0]-1];
			const auto& pb=icosphere::vertexes[t[1]-1];
			const auto& pc=icosphere::vertexes[t[2]-1];
			vf3d va=pos+rad*vf3d(pa[0], pa[1], pa[2]);
			vf3d vb=pos+rad*vf3d(pb[0], pb[1], pb[2]);
			vf3d vc=pos+rad*vf3d(pc[0], pc[1], pc[2]);
			vf3d norm=normalize(cross(vb-va, vc-va));
			vf3d ctr=(va+vb+vc)/3;
			vf3d sun_dir=normalize(light_pos-ctr);
			float dp=std::max(.2f, dot(sun_dir, norm));
			sgl_c3f(dp*r, dp*g, dp*b);
			sgl_v3f(va.x, va.y, va.z);
			sgl_v3f(vb.x, vb.y, vb.z);
			sgl_v3f(vc.x, vc.y, vc.z);
		}
		sgl_end();
	}

	void renderAxes(const vf3d& pos, float len, float sz) {
		sgl_c3f(1, 0, 0);
		renderArrow(pos, pos+len*vf3d(1, 0, 0), sz);
		sgl_c3f(0, 1, 0);
		renderArrow(pos, pos+len*vf3d(0, 1, 0), sz);
		sgl_c3f(0, 0, 1);
		renderArrow(pos, pos+len*vf3d(0, 0, 1), sz);
	}

	//show player directions with arrows
	void renderPlayerAxes(float sz) {
		sgl_c3f(0, 0, 0);
		renderArrow(player.pos, player.pos+sz*player.look, .2f);
		sgl_c3f(1, 0, 1);
		renderArrow(player.pos, player.pos+sz*player.rgt, .2f);//~x
		sgl_c3f(0, 1, 1);
		renderArrow(player.pos, player.pos+sz*player.up, .2f);//~y
		sgl_c3f(1, 1, 0);
		renderArrow(player.pos, player.pos+sz*player.fwd, .2f);//~z
	}

	void renderImGui() {
		simgui_frame_desc_t simgui_frame_desc{};
		simgui_frame_desc.width=sapp_width();
		simgui_frame_desc.height=sapp_height();
		simgui_frame_desc.delta_time=sapp_frame_duration();
		simgui_frame_desc.dpi_scale=sapp_dpi_scale();
		simgui_new_frame(simgui_frame_desc);

		ImGui::Begin("Controls");
		ImGui::Text("W/A/S/D to move around");
		ImGui::Text("ARROWS to look around");
		ImGui::Text("Drag LMB to orbit planet");
		ImGui::End();

		ImGui::Begin("Info");
		ImGui::SeparatorText("World Axes");
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "rgt(x)");
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "up(y)");
		ImGui::TextColored(ImVec4(0, 0, 1, 1), "fwd(z)");
		ImGui::SeparatorText("Player axes");
		ImGui::TextColored(ImVec4(1, 0, 1, 1), "rgt(~x)");
		ImGui::TextColored(ImVec4(0, 1, 1, 1), "up(~y)");
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "fwd(~z)");
		ImGui::End();

		ImGui::Begin("Options");
		ImGui::Checkbox("Player view", &player.use_perspective);
		if(player.use_perspective) {
			ImGui::SameLine();
			ImGui::Checkbox("Mini axes", &player.show_axes);
		}
		if(ImGui::Button("Set light position")) {
			light_pos=third_person.pos;
		}
		ImGui::End();

		simgui_render();
	}
#pragma endregion

	bool user_render() override {
		sg_pass pass{};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();
		sgl_load_pipeline(pip);
		sgl_matrix_mode_modelview();
		sgl_lookat(
			cam.pos.x,
			cam.pos.y,
			cam.pos.z,
			cam.pos.x+cam.dir.x,
			cam.pos.y+cam.dir.y,
			cam.pos.z+cam.dir.z,
			cam.up.x, cam.up.y, cam.up.z
		);
		sgl_matrix_mode_projection();
		sgl_perspective(
			sgl_rad(90),
			sapp_widthf()/sapp_heightf(),
			.01f,
			100
		);

		renderAxes({0, 0, 0}, 1.5f+planet.rad, .1f);

		//planet as sphere
		renderSphere(planet.pos, planet.rad, 1, 1, 1);

		if(!player.use_perspective) {
			//player as sphere
			renderSphere(player.pos, .075f, .5f, 0, 1);
			renderPlayerAxes(.33f);
		} else {
			//show axes right in front of player
			if(player.show_axes) {
				renderAxes(cam.pos+player.sz*player.look, .25f*player.sz, .2f);
			}
		}

		sgl_draw();

		renderImGui();

		sg_end_pass();

		sg_commit();

		return true;
	}

	void user_input(const sapp_event* e) override {
		simgui_handle_event(e);
	}

	void user_destroy() override {
		simgui_shutdown();
		sgl_shutdown();
	}
};

CMN_SOKOL_ENGINE_LAUNCH(PlanetGame, 640, 480)