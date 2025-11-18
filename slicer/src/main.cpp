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
#define OLC_PGE_APPLICATION
#include "common/3d/engine_3d.h"
namespace olc {
	static const Pixel PURPLE(144, 0, 255);
	static const Pixel ORANGE(255, 115, 0);
}
using cmn::vf3d;
using cmn::Mat4;

#include "common/utils.h"

#include "conversions.h"

#include <vector>

#include <list>

#include <sstream>
#include <iomanip>

struct Prism {
	int x=0, y=0, z=0;
	int w=0, d=0;
};

const int valid_sizes[][2]{
	{2, 10},
	{2, 8},
	{2, 6},
	{1, 10},
	{1, 8},
	{2, 4},
	{1, 6},
	{2, 3},
	{1, 4},
	{2, 2},
	{1, 3},
	{1, 2},
	{1, 1}
};
const int num_valid_sizes=sizeof(valid_sizes)/sizeof(valid_sizes[0]);

class SlicerUI : public cmn::Engine3D {
	//camera direction
	float cam_yaw=2.05f;
	float cam_pitch=-.225f;
	vf3d light_pos;

	//orbit controls
	olc::vf2d* orbit_start=nullptr;
	float temp_yaw=0, temp_pitch=0;

	//scene stuff
	cmn::AABB3 build_volume;
	Mesh model;

	//default to lego stud width
	vf3d resolution{7.8f, 9.6f, 7.8f};

	VoxelSet voxels;
	std::vector<Prism> prisms;
	int min_layer=0;
	int max_layer=0;
	bool realize_edges=true;

	bool help_menu=false;

public:
	SlicerUI() {
		sAppName="Slicer Demo";
	}

	bool setupModel() {
		//try load model mesh
		auto status=Mesh::loadFromOBJ(model, "assets/3DBenchy.txt");
		if(!status.valid) {
			std::cout<<"  "<<status.msg<<'\n';

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

			model.updateMatrixes();
			model.updateTriangles();
		}

		return true;
	}

	bool user_create() override {
		std::srand(std::time(0));

		if(!setupModel()) return false;

		handleResliceAction();

		return true;
	}

#pragma region UPDATE HELPERS
	void handleCameraLooking(float dt) {
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
	}

	//dynamic camera system :D
	void handleCameraMovement(float dt) {
		//find sphere to envelope bounds
		vf3d ctr=build_volume.getCenter();
		float st_dist=1+(build_volume.max-ctr).mag();

		//step backwards along cam ray
		vf3d st=ctr-st_dist*cam_dir;

		//snap pt to bounds & make relative to origin
		float rad=st_dist-build_volume.intersectRay(st, cam_dir);

		//push cam back by some some margin
		cam_pos=-(110+rad)*cam_dir;

		//light at camera
		light_pos=cam_pos;
	}

	void handleResliceAction() {
		//voxellize mesh
		voxels=meshToVoxels(model, resolution);
		const int& v_w=voxels.getWidth();
		const int& v_h=voxels.getHeight();
		const int& v_d=voxels.getDepth();

		//reset layer range
		min_layer=0;
		max_layer=voxels.getHeight()-1;

		//clear prism list
		prisms.clear();

		//which voxels have i used?
		bool* meshed=new bool[v_w*v_d];
		auto ik_ix=[v_w] (int i, int k) {
			return i+v_w*k;
		};

		//generalized indexing logic
		const int dims[2]{v_w, v_d};
		int ik[2];

		//slice along height
		for(int j=0; j<v_h; j++) {
			//reset meshed
			std::memset(meshed, false, sizeof(bool)*v_w*v_d);

			//flip prioritized axis each slice
			const int axis_a=j%2, axis_b=(axis_a+1)%2;
			const int dim_a=dims[axis_a], dim_b=dims[axis_b];
			for(int a=0; a<dim_a; a++) {
				for(int b=0; b<dim_b; b++) {
					ik[axis_a]=a, ik[axis_b]=b;

					//skip if air or meshed
					if(!voxels.grid[voxels.ix(ik[0], j, ik[1])]
						||meshed[ik_ix(ik[0], ik[1])]) continue;

					//try combine in a
					int sz_a=1;
					for(int a_=1+a; a_<dim_a; a_++, sz_a++) {
						//stop if air or meshed
						ik[axis_a]=a_, ik[axis_b]=b;
						if(!voxels.grid[voxels.ix(ik[0], j, ik[1])]
							||meshed[ik_ix(ik[0], ik[1])]) break;
					}

					//try combine combination in b
					int sz_b=1;
					for(int b_=1+b; b_<dim_b; b_++, sz_b++) {
						bool able=true;
						for(int a_=0; a_<sz_a; a_++) {
							//stop if air or meshed
							ik[axis_a]=a+a_, ik[axis_b]=b_;
							if(!voxels.grid[voxels.ix(ik[0], j, ik[1])]
								||meshed[ik_ix(ik[0], ik[1])]) {
								able=false;
								break;
							}
						}
						if(!able) break;
					}

					//limit size to valid
					for(int s=0; s<num_valid_sizes; s++) {
						//check both ways
						const auto& w=valid_sizes[s][0];
						const auto& h=valid_sizes[s][1];
						if(w<=sz_a&&h<=sz_b) {
							sz_a=w, sz_b=h;
							break;
						}
						if(w<=sz_b&&h<=sz_a) {
							sz_b=w, sz_a=h;
							break;
						}
					}

					//set meshed
					for(int a_=0; a_<sz_a; a_++) {
						for(int b_=0; b_<sz_b; b_++) {
							ik[axis_a]=a+a_, ik[axis_b]=b+b_;
							meshed[ik_ix(ik[0], ik[1])]=true;
						}
					}

					//append to prism list
					Prism p;
					ik[axis_a]=a, ik[axis_b]=b;
					p.x=ik[0], p.y=j, p.z=ik[1];
					ik[axis_a]=sz_a, ik[axis_b]=sz_b;
					p.w=ik[0], p.d=ik[1];
					prisms.push_back(p);
				}
			}
		}

		delete[] meshed;
	}
#pragma endregion

	bool user_update(float dt) override {
		handleCameraLooking(dt);

		//3d polar to cartesian
		cam_dir={
			std::cos(temp_yaw)*std::cos(temp_pitch),
			std::sin(temp_pitch),
			std::sin(temp_yaw)*std::cos(temp_pitch)
		};

		handleCameraMovement(dt);

		if(GetKey(olc::Key::E).bPressed) realize_edges^=true;
		if(GetKey(olc::Key::R).bPressed) handleResliceAction();
		if(GetKey(olc::Key::H).bPressed) help_menu^=true;

		return true;
	}

#pragma region GEOMETRY HELPERS
	//this looks nice :D
	void realizeArrow(const vf3d& a, const vf3d& b, float sz, const olc::Pixel& col) {
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

	void realizeFilledAABB(const cmn::AABB3& box, const olc::Pixel& col) {
		vf3d v0=box.min, v7=box.max;
		vf3d v1(v7.x, v0.y, v0.z);
		vf3d v2(v0.x, v7.y, v0.z);
		vf3d v3(v7.x, v7.y, v0.z);
		vf3d v4(v0.x, v0.y, v7.z);
		vf3d v5(v7.x, v0.y, v7.z);
		vf3d v6(v0.x, v7.y, v7.z);
		std::vector<cmn::Triangle> tris{
			{v0, v1, v4},
			{v1, v5, v4},
			{v0, v2, v1},
			{v2, v3, v1},
			{v1, v3, v7},
			{v1, v7, v5},
			{v5, v7, v6},
			{v5, v6, v4},
			{v0, v6, v2},
			{v0, v4, v6},
			{v2, v7, v3},
			{v2, v6, v7},
			{v0, v1, v2}
		};
		for(auto& t:tris) {
			t.col=col;
			tris_to_project.push_back(t);
		}
	}

	void realizeAxes(const vf3d& pos, float sz) {
		realizeArrow(pos, pos+vf3d(sz, 0, 0), .2f, olc::RED);
		realizeArrow(pos, pos+vf3d(0, sz, 0), .2f, olc::BLUE);
		realizeArrow(pos, pos+vf3d(0, 0, sz), .2f, olc::GREEN);
	}

	void realizePrisms() {
		for(const auto& p:prisms) {
			//skip in not in layer range
			if(p.y<min_layer) continue;
			if(p.y>max_layer) continue;

			//scale to correct position
			vf3d pos=voxels.offset+voxels.scale*vf3d(p.x, p.y, p.z);
			vf3d size=voxels.scale*vf3d(p.w, 1, p.d);

			//shrink if showing edges
			float margin=realize_edges?.25f:0;
			realizeFilledAABB({pos+margin, pos+size-margin}, olc::WHITE);
			if(realize_edges) {
				//color aabb based on efficiency
				//  green=good, yellow=fine, red=bad
				olc::Pixel col(255, 71, 71);
				switch((p.w>1)+(p.d>1)) {
					case 1: col=olc::Pixel(255, 255, 71); break;
					case 2: col=olc::Pixel(71, 255, 71); break;
				}
				addAABB({pos, pos+size}, col);
			}
		}
	}
#pragma endregion

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});

		//add prisms
		realizePrisms();

		//add scene blue bounds
		addAABB(build_volume, olc::Pixel(71, 77, 255));

		//add axes at build volume corner
		realizeAxes(5.f+build_volume.min, 25);

		return true;
	}

#pragma region RENDER HELPERS
	//handle & render a generic type slider.
	template<typename T>
	void renderSlider(
		int x, int y, std::string title, const olc::Pixel& col,
		int width, T* val, T min, T max
	) {
		int nx=x+2+8*title.length();

		//update
		if(GetMouse(olc::Mouse::LEFT).bHeld) {
			int rx=GetMouseX()-nx;
			int ry=GetMouseY()-(y+1);
			if(rx>=0&&ry>=0&&rx<width&&ry<8) {
				float t=rx/(width-1.f);
				if(t<0) t=0;
				if(t>1) t=1;
				*val=min+t*(max-min);
			}
		}

		//render
		{
			//background
			FillRect(x, y, 3+8*title.length()+width, 10, olc::Pixel(60, 60, 60));

			//draw slid portion
			float t=float(*val-min)/(max-min);
			FillRect(nx, y+1, std::round(t*width), 8, col);
			DrawString(x+1, y+1, title, col);

			//draw value in center with at most 2 dig precision
			std::ostringstream oss;
			oss<<std::fixed<<std::setprecision(2)<<*val;
			auto val_str=oss.str();
			DrawString(nx+width/2-4*val_str.length(), y+1, val_str, olc::WHITE);
		}
	}

	void renderSliders() {
		const int width=80;

		//resolution sliders
		renderSlider<float>(
			10, ScreenHeight()-60,
			"Size X(mm)", olc::RED, width,
			&resolution.x, 4, 15.6f
		);
		renderSlider<float>(
			10, ScreenHeight()-40,
			"Size Y(mm)", olc::BLUE, width,
			&resolution.y, 4, 15.6f
		);
		renderSlider<float>(
			10, ScreenHeight()-20,
			"Size Z(mm)", olc::GREEN, width,
			&resolution.z, 4, 15.6f
		);

		//layer sliders
		renderSlider<int>(
			ScreenWidth()-3-8*13-width, ScreenHeight()-40,
			"Min Layer(#)", olc::ORANGE, width,
			&min_layer, 0, voxels.getHeight()-1
		);
		renderSlider<int>(
			ScreenWidth()-3-8*13-width, ScreenHeight()-20,
			"Max Layer(#)", olc::PURPLE, width,
			&max_layer, 0, voxels.getHeight()-1
		);
		if(min_layer>max_layer) std::swap(min_layer, max_layer);
	}

	void renderHelpHints() {
		int cx=ScreenWidth()/2;
		if(help_menu) {
			DrawString(8, 8, "Movement Controls");
			DrawString(8, 16, "ARROWS to orbit");
			DrawString(8, 24, "MouseRight to orbit");

			DrawString(ScreenWidth()-8*17, 8, "General Controls");
			DrawString(ScreenWidth()-8*18, 16, "E to toggle edges", realize_edges?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*19, 24, "R to reslice model", olc::WHITE);
			DrawString(ScreenWidth()-8*26, 32, "MouseLeft to drag sliders");

			DrawString(cx-4*18, ScreenHeight()-8, "[Press H to close]");
		} else {
			DrawString(cx-4*18, ScreenHeight()-8, "[Press H for help]");
		}
	}
#pragma endregion

	bool user_render() override {
		Clear(olc::Pixel(20, 20, 20));

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

		renderSliders();

		renderHelpHints();

		return true;
	}
};

int main() {
	SlicerUI sui;
	bool vsync=true;
	if(sui.Construct(800, 600, 1, 1, false, vsync)) sui.Start();

	return 0;
}