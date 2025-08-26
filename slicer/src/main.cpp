/*TODO
make bounding volume hierarchy
	to speed up voxelization

place stud models

textured models to find color

color palatte lookup

api prices for bill of materials

instructions
*/

#define OLC_GFX_OPENGL33
#include "common/3d/engine_3d.h"
using cmn::vf3d;
using olc::vf2d;

#define OLC_PGEX_DEAR_IMGUI_IMPLEMENTATION
#include "olcPGEX_ImGui.h"

#include "common/utils.h"

#include "conversions.h"

class SlicerUI : public cmn::Engine3D {
	//camera direction
	float cam_yaw=1.07f;
	float cam_pitch=-.56f;
	vf3d light_pos;

	//orbit controls
	olc::vf2d* orbit_start=nullptr;
	float temp_yaw=0, temp_pitch=0;
	
	//scene stuff
	cmn::AABB3 build_volume;
	Mesh model;

	//lego stud width
	float resolution=7.8f;
	
	//imgui stuff
	olc::imgui::PGE_ImGUI pge_imgui;
	int game_layer=0;

	bool prev_reslice=false;

	VoxelSet voxels;
	Mesh voxels_mesh;

public:
	SlicerUI() : pge_imgui(true) {
		sAppName="Slicer Demo";
	}

	bool user_create() override {
		std::srand(std::time(0));
		
		//try load test model
		try {
			model=Mesh::loadFromOBJ("assets/3DBenchy.txt");
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		//prusa mk4 build volume(250x210x220mm)
		build_volume={{-125, -105, -110}, {125, 105, 110}};

		//scale model into build volume
		{
			vf3d build_dims=build_volume.max-build_volume.min;
			cmn::AABB3 model_box=model.getLocalAABB();
			vf3d box_dims=model_box.max-model_box.min;

			//which dimension is the weak link?
			vf3d num=build_dims/box_dims;
			float scl=std::min(num.x, std::min(num.y, num.z));

			//scale & place model in center
			model.scale={scl, scl, scl};
			model.offset=build_volume.getCenter()-model_box.getCenter();

			model.updateTransforms();
			model.updateTriangles();
			model.colorNormally();

			//translate model onto floor?
		}

		game_layer=CreateLayer();
		EnableLayer(game_layer, true);

		return true;
	}

	void handleCameraMovement(float dt) {
		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch-=dt;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch+=dt;
		//clamp pitch
		if(cam_pitch<-cmn::Pi/2) cam_pitch=.001f-cmn::Pi/2;
		if(cam_pitch>cmn::Pi/2) cam_pitch=cmn::Pi/2-.001f;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw+=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw-=dt;

		//start orbit
		const auto orbit_action=GetMouse(olc::Mouse::RIGHT);
		if(orbit_action.bPressed) {
			orbit_start=new olc::vf2d(GetMousePos());
		}

		//temporary orbit direction
		temp_pitch=cam_pitch;
		temp_yaw=cam_yaw;
		if(orbit_start) {
			olc::vf2d diff=GetMousePos()-*orbit_start;
			temp_pitch-=.0067f*diff.y;
			//clamp new pitch
			if(temp_pitch<-cmn::Pi/2) temp_pitch=.001f-cmn::Pi/2;
			if(temp_pitch>cmn::Pi/2) temp_pitch=cmn::Pi/2-.001f;
			temp_yaw+=.01f*diff.x;
		}

		//end orbit
		if(orbit_action.bReleased) {
			//set actual direction to temp direction
			cam_yaw=temp_yaw;
			cam_pitch=temp_pitch;

			//clear pointer flags
			delete orbit_start;
			orbit_start=nullptr;
		}

		//angles to direction
		cam_dir={
			std::cos(temp_yaw)* std::cos(temp_pitch),
			std::sin(temp_pitch),
			std::sin(temp_yaw)* std::cos(temp_pitch)
		};

		//dynamic camera system :D
		{
			//find sphere to envelope bounds
			vf3d ctr=build_volume.getCenter();
			float st_dist=1+(build_volume.max-ctr).mag();
			//step backwards along cam ray
			vf3d st=ctr-st_dist*cam_dir;
			//snap pt to bounds & make relative to origin
			float rad=st_dist-build_volume.intersectRay(st, cam_dir);
			//push cam back by some some margin
			cam_pos=-(136.5f+rad)*cam_dir;
			//light at camera
			light_pos=cam_pos;
		}
	}

	void reslice() {
		voxels=meshToVoxels(model, resolution);
		voxels_mesh=voxelsToMesh(voxels);
	}

	bool user_update(float dt) override {
		const vf2d mouse_pos=GetMousePos();

		handleCameraMovement(dt);

		ImGui::Begin("Slicer Options");
		
		ImGui::Text("size(mm)");
		ImGui::SameLine();
		//nanoblock -> duplo
		ImGui::SliderFloat("", &resolution, 4, 15.6f);
		
		if(ImGui::Button("Reslice")) reslice();
		ImGui::SameLine();
		if(ImGui::Button("Color Normals")) voxels_mesh.colorNormally();

		ImGui::End();

		return true;
	}

#pragma region GEOMETRY HELPERS
	//this looks nice :D
	void addArrow(const vf3d& a, const vf3d& b, float sz, const olc::Pixel& col) {
		vf3d ba=b-a;
		float mag=ba.mag();
		vf3d ca=cam_pos-a;
		vf3d perp=.5f*sz*mag*ba.cross(ca).norm();
		vf3d c=b-sz*ba;
		vf3d l=c-perp, r=c+perp;
		cmn::Line l1{a, c}; l1.col=col;
		lines_to_project.push_back(l1);
		cmn::Line l2{l, r}; l2.col=col;
		lines_to_project.push_back(l2);
		cmn::Line l3{l, b}; l3.col=col;
		lines_to_project.push_back(l3);
		cmn::Line l4{r, b}; l4.col=col;
		lines_to_project.push_back(l4);
	}
#pragma endregion

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});

		//add model
		//tris_to_project.insert(tris_to_project.end(),
			//model.tris.begin(), model.tris.end()
		//);

		//add voxels mesh
		tris_to_project.insert(tris_to_project.end(),
			voxels_mesh.tris.begin(), voxels_mesh.tris.end()
		);

		//add scene bounds
		addAABB(build_volume, olc::BLACK);

		//add axis
		{
			vf3d corner=.1f+build_volume.min;
			float sz=.6f;
			addArrow(corner, corner+vf3d(sz, 0, 0), .2f, olc::RED);
			addArrow(corner, corner+vf3d(0, sz, 0), .2f, olc::BLUE);
			addArrow(corner, corner+vf3d(0, 0, sz), .2f, olc::GREEN);
		}

		return true;
	}

	bool user_render() override {
		SetDrawTarget(game_layer);
		
		Clear(olc::GREY);

		//render 3d stuff
		resetBuffers();

		for(const auto& t:tris_to_draw) {
			FillDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].w,
				t.col, t.id
			);
		}

		for(const auto& l:lines_to_draw) {
			DrawDepthLine(
				l.p[0].x, l.p[0].y, l.t[0].w,
				l.p[1].x, l.p[1].y, l.t[1].w,
				l.col, l.id
			);
		}
		
		return true;
	}
};

int main() {
	SlicerUI sui;
	if(sui.Construct(600, 600, 1, 1, false, true)) sui.Start();

	return 0;
}