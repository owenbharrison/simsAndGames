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

#define MULTITHREADING
#ifdef __EMSCRIPTEN__
#undef MULTITHREADING
#endif

#define OLC_PGEX_DEAR_IMGUI_IMPLEMENTATION
#include "olcPGEX_ImGui.h"

#include "common/utils.h"

#include "conversions.h"

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

struct Prism {
	int x=0, y=0, z=0;
	int w=0, d=0;
};

#ifdef MULTITHREADING
#include "thread_pool.h"
#endif

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
	vf3d resolution{7.8f, 9.6f, 7.8f};
	
	//imgui stuff
	olc::imgui::PGE_ImGUI pge_imgui;
	int game_layer=0;

	bool prev_reslice=false;

	VoxelSet voxels;
	std::vector<Prism> prisms;
	int min_layer=0;
	int max_layer=0;
	bool show_edges=true;

	//multithreaded rendering
#ifdef MULTITHREADING
	const int mtr_tile_size=64;
	int mtr_num_x=0, mtr_num_y=0;
	std::vector<cmn::Triangle>* mtr_bins=nullptr;

	std::vector<olc::vi2d> mtr_jobs;

	ThreadPool* mtr_pool=nullptr;
#endif

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

#ifdef MULTITHREADING
		//allocate tiles
		mtr_num_x=1+ScreenWidth()/mtr_tile_size;
		mtr_num_y=1+ScreenHeight()/mtr_tile_size;
		mtr_bins=new std::vector<cmn::Triangle>[mtr_num_x*mtr_num_y];

		//allocate jobs
		for(int i=0; i<mtr_num_x; i++) {
			for(int j=0; j<mtr_num_y; j++) {
				mtr_jobs.push_back({i, j});
			}
		}

		//make thread pool
		mtr_pool=new ThreadPool(std::thread::hardware_concurrency());
#endif

		return true;
	}
	
#pragma region UPDATE HELPERS
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
			cam_pos=-(110+rad)*cam_dir;
			//light at camera
			light_pos=cam_pos;
		}
	}

	void reslice() {
		//voxellize mesh
		voxels=meshToVoxels(model, resolution);
		const int& v_w=voxels.getWidth();
		const int& v_h=voxels.getHeight();
		const int& v_d=voxels.getDepth();

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
		const vf2d mouse_pos=GetMousePos();

		handleCameraMovement(dt);

		ImGui::Begin("Slicer Options");
		
		//nanoblock -> duplo
		ImGui::Text("size_x(mm)");
		ImGui::SameLine();
		ImGui::SliderFloat("##sx", &resolution.x, 4, 15.6f);
		ImGui::Text("size_y(mm)");
		ImGui::SameLine();
		ImGui::SliderFloat("##sy", &resolution.y, 4, 15.6f);
		ImGui::Text("size_z(mm)");
		ImGui::SameLine();
		ImGui::SliderFloat("##sz", &resolution.z, 4, 15.6f);

		if(ImGui::Button("Reslice")) reslice();

		ImGui::Text("Min Layer");
		ImGui::SameLine();
		ImGui::SliderInt("##nl", &min_layer, 0, voxels.getHeight());
		ImGui::Text("Max Layer");
		ImGui::SameLine();
		ImGui::SliderInt("##xl", &max_layer, 0, voxels.getHeight());
		if(min_layer>max_layer) std::swap(min_layer, max_layer);

		ImGui::Text("Show Edges");
		ImGui::SameLine();
		ImGui::Checkbox("##se", &show_edges);

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

	void fillAABB(const cmn::AABB3& box, const olc::Pixel& col) {
		vf3d v0=box.min, v7=box.max;
		vf3d v1(v7.x, v0.y, v0.z);
		vf3d v2(v0.x, v7.y, v0.z);
		vf3d v3(v7.x, v7.y, v0.z);
		vf3d v4(v0.x, v0.y, v7.z);
		vf3d v5(v7.x, v0.y, v7.z);
		vf3d v6(v0.x, v7.y, v7.z);
		cmn::Triangle t1;
		t1={v0, v1, v4}, t1.col=col, tris_to_project.push_back(t1);
		t1={v1, v5, v4}, t1.col=col, tris_to_project.push_back(t1);
		t1={v0, v2, v1}, t1.col=col, tris_to_project.push_back(t1);
		t1={v2, v3, v1}, t1.col=col, tris_to_project.push_back(t1);
		t1={v1, v3, v7}, t1.col=col, tris_to_project.push_back(t1);
		t1={v1, v7, v5}, t1.col=col, tris_to_project.push_back(t1);
		t1={v5, v7, v6}, t1.col=col, tris_to_project.push_back(t1);
		t1={v5, v6, v4}, t1.col=col, tris_to_project.push_back(t1);
		t1={v0, v6, v2}, t1.col=col, tris_to_project.push_back(t1);
		t1={v0, v4, v6}, t1.col=col, tris_to_project.push_back(t1);
		t1={v2, v7, v3}, t1.col=col, tris_to_project.push_back(t1);
		t1={v2, v6, v7}, t1.col=col, tris_to_project.push_back(t1);
		t1={v0, v1, v2}, t1.col=col, tris_to_project.push_back(t1);
	}
#pragma endregion

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});

		//add prisms
		for(const auto& p:prisms) {
			//skip in not in layer range
			if(p.y<min_layer) continue;
			if(p.y>max_layer) continue;

			//scale to correct position
			vf3d pos=voxels.offset+voxels.scale*vf3d(p.x, p.y, p.z);
			vf3d size=voxels.scale*vf3d(p.w, 1, p.d);
			//shrink if showing edges
			float margin=show_edges?.25f:0;
			fillAABB({pos+margin, pos+size-margin}, olc::WHITE);
			if(show_edges) {
				//color aabb based on efficiency?
				olc::Pixel col=olc::RED;
				switch((p.w>1)+(p.d>1)) {
					case 1: col=olc::YELLOW; break;
					case 2: col=olc::BLUE; break;
				}
				addAABB({pos, pos+size}, col);
			}
		}

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

#pragma region RENDER HELPERS
	void FillDepthTriangleWithin(
		int x1, int y1, float w1,
		int x2, int y2, float w2,
		int x3, int y3, float w3,
		olc::Pixel col, int id,
		int nx, int ny, int mx, int my
	) {
		//sort by y
		if(y2<y1) {
			std::swap(x1, x2);
			std::swap(y1, y2);
			std::swap(w1, w2);
		}
		if(y3<y1) {
			std::swap(x1, x3);
			std::swap(y1, y3);
			std::swap(w1, w3);
		}
		if(y3<y2) {
			std::swap(x2, x3);
			std::swap(y2, y3);
			std::swap(w2, w3);
		}

		//calculate slopes
		int dx1=x2-x1;
		int dy1=y2-y1;
		float dw1=w2-w1;

		int dx2=x3-x1;
		int dy2=y3-y1;
		float dw2=w3-w1;

		float t;

		float tex_w;

		float dax_step=0, dbx_step=0,
			dw1_step=0, dw2_step=0;

		if(dy1) dax_step=dx1/std::fabsf(dy1);
		if(dy2) dbx_step=dx2/std::fabsf(dy2);

		if(dy1) dw1_step=dw1/std::fabsf(dy1);
		if(dy2) dw2_step=dw2/std::fabsf(dy2);

		if(dy1) for(int j=y1; j<=y2; j++) {
			if(j<ny||j>=my) continue;
			int ax=x1+dax_step*(j-y1);
			int bx=x1+dbx_step*(j-y1);
			float tex_sw=w1+dw1_step*(j-y1);
			float tex_ew=w1+dw2_step*(j-y1);
			//sort along x
			if(ax>bx) {
				std::swap(ax, bx);
				std::swap(tex_sw, tex_ew);
			}
			float t_step=1.f/(bx-ax);
			for(int i=ax; i<bx; i++) {
				if(i<nx||i>=mx) continue;
				t=t_step*(i-ax);
				tex_w=tex_sw+t*(tex_ew-tex_sw);
				float& depth=depth_buffer[bufferIX(i, j)];
				if(tex_w>depth) {
					Draw(i, j, col);
					depth=tex_w;
					id_buffer[bufferIX(i, j)]=id;
				}
			}
		}

		//recalculate slopes
		dx1=x3-x2;
		dy1=y3-y2;
		dw1=w3-w2;

		if(dy1) dax_step=dx1/std::fabsf(dy1);

		if(dy1) dw1_step=dw1/std::fabsf(dy1);

		for(int j=y2; j<=y3; j++) {
			if(j<ny||j>=my) continue;
			int ax=x2+dax_step*(j-y2);
			int bx=x1+dbx_step*(j-y1);
			float tex_sw=w2+dw1_step*(j-y2);
			float tex_ew=w1+dw2_step*(j-y1);
			//sort along x
			if(ax>bx) {
				std::swap(ax, bx);
				std::swap(tex_sw, tex_ew);
			}
			float t_step=1.f/(bx-ax);
			for(int i=ax; i<bx; i++) {
				if(i<nx||i>=mx) continue;
				t=t_step*(i-ax);
				tex_w=tex_sw+t*(tex_ew-tex_sw);
				float& depth=depth_buffer[bufferIX(i, j)];
				if(tex_w>depth) {
					Draw(i, j, col);
					depth=tex_w;
					id_buffer[bufferIX(i, j)]=id;
				}
			}
		}
	}
#pragma endregion

	bool user_render() override {
		SetDrawTarget(game_layer);
		
		Clear(olc::GREY);

		resetBuffers();
#ifdef MULTITHREADING
		//clear tile bins
		for(int i=0; i<mtr_num_x*mtr_num_y; i++) mtr_bins[i].clear();

		//bin triangles
		for(const auto& tri:tris_to_draw) {
			//compute screen space bounds
			float min_x=std::min(tri.p[0].x, std::min(tri.p[1].x, tri.p[2].x));
			float min_y=std::min(tri.p[0].y, std::min(tri.p[1].y, tri.p[2].y));
			float max_x=std::max(tri.p[0].x, std::max(tri.p[1].x, tri.p[2].x));
			float max_y=std::max(tri.p[0].y, std::max(tri.p[1].y, tri.p[2].y));

			//which tiles does it overlap
			int sx=std::clamp(int(min_x/mtr_tile_size), 0, mtr_num_x-1);
			int sy=std::clamp(int(min_y/mtr_tile_size), 0, mtr_num_y-1);
			int ex=std::clamp(int(max_x/mtr_tile_size), 0, mtr_num_x-1);
			int ey=std::clamp(int(max_y/mtr_tile_size), 0, mtr_num_y-1);

			//add tris to corresponding bins
			for(int tx=sx; tx<=ex; tx++) {
				for(int ty=sy; ty<=ey; ty++) {
					mtr_bins[tx+mtr_num_x*ty].push_back(tri);
				}
			}
		}

		//queue work
		std::atomic<int> tiles_remaining=mtr_jobs.size();
		for(const auto& j:mtr_jobs) {
			mtr_pool->enqueue([&] {
				int nx=std::clamp(mtr_tile_size*j.x, 0, ScreenWidth());
				int ny=std::clamp(mtr_tile_size*j.y, 0, ScreenHeight());
				int mx=std::clamp(mtr_tile_size+nx, 0, ScreenWidth());
				int my=std::clamp(mtr_tile_size+ny, 0, ScreenHeight());
				const auto& bin=mtr_bins[j.x+mtr_num_x*j.y];
				for(const auto& t:bin) {
					FillDepthTriangleWithin(
						t.p[0].x, t.p[0].y, t.t[0].w,
						t.p[1].x, t.p[1].y, t.t[1].w,
						t.p[2].x, t.p[2].y, t.t[2].w,
						t.col, t.id,
						nx, ny, mx, my
					);
				}
				tiles_remaining--;
			});
		}

		//wait for raster to finish
		while(tiles_remaining);
#else
		for(const auto& t:tris_to_draw) {
			FillDepthTriangle(
				t.p[0].x, t.p[0].y, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].w,
				t.col, t.id
			);
		}
#endif

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
	bool vsync=true;
	if(sui.Construct(800, 800, 1, 1, false, vsync)) sui.Start();

	return 0;
}