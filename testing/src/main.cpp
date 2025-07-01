#define OLC_GFX_OPENGL33
#include "common/3d/engine_3d.h"
using olc::vf2d;
using cmn::vf3d;
using cmn::Mat4;

#include "scene.h"

#include "common/utils.h"

#include "thread_pool.h"

#define OLC_PGEX_SHADERS
#include "olcPGEX_Shaders.h"

struct Example : cmn::Engine3D {
	Example() {
		sAppName="3d builder";
	}

	//camera positioning
	float cam_yaw=.71f;
	float cam_pitch=-.58f;

	//scene stuff
	Scene scene;

	const int tile_size=64;
	int tiles_x=0, tiles_y=0;
	std::vector<cmn::Triangle>* tile_bins=nullptr;

	std::vector<olc::vi2d> tile_jobs;
	ThreadPool* raster_pool=nullptr;

	olc::Shade shade;
	olc::Effect shader;

	bool user_create() override {
		cam_pos={-5.61f, 8.42f, -4.89f};

		//try load scene
		try {
			scene=Scene::load("assets/bench.scn");
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		//allocate tiles
		tiles_x=ScreenWidth()/tile_size;
		tiles_y=ScreenHeight()/tile_size;
		tile_bins=new std::vector<cmn::Triangle>[tiles_x*tiles_y];

		//allocate jobs
		for(int i=0; i<tiles_x; i++) {
			for(int j=0; j<tiles_y; j++) {
				tile_jobs.push_back({i, j});
			}
		}

		//make thread pool
		raster_pool=new ThreadPool(std::thread::hardware_concurrency());

		return true;
	}

	bool user_destroy() override {
		delete[] tile_bins;

		delete raster_pool;

		return true;
	}

	//mostly camera controls
	bool user_update(float dt) override {
		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>cmn::Pi/2) cam_pitch=cmn::Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-cmn::Pi/2) cam_pitch=.001f-cmn::Pi/2;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw-=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw+=dt;

		//polar to cartesian
		cam_dir=vf3d(
			std::cosf(cam_yaw)*std::cosf(cam_pitch),
			std::sinf(cam_pitch),
			std::sinf(cam_yaw)*std::cosf(cam_pitch)
		);

		//move up, down
		if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*dt;
		if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::cosf(cam_yaw), 0, std::sinf(cam_yaw));
		if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*dt*fb_dir;
		if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*dt*lr_dir;
		if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*dt*lr_dir;

		//set light pos
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		return true;
	}

	bool user_geometry() override {
		for(const auto& m:scene.meshes) {
			for(const auto& t:m.tris){
				tris_to_project.push_back(t);
				//lines_to_project.push_back({t.p[0], t.p[1]});
				//lines_to_project.push_back({t.p[1], t.p[2]});
				//lines_to_project.push_back({t.p[2], t.p[0]});
			}
		}

		return true;
	}

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

		int si, ei, sj, ej;

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

	bool user_render() override {
		Clear(olc::Pixel(100, 100, 100));

		//render 3d stuff
		resetBuffers();

#if 1
		//clear bins
		for(int i=0; i<tiles_x*tiles_y; i++) tile_bins[i].clear();

		//bin triangles
		for(const auto& tri:tris_to_draw) {
			//compute screen space bounds
			float min_x=std::min(tri.p[0].x, std::min(tri.p[1].x, tri.p[2].x));
			float min_y=std::min(tri.p[0].y, std::min(tri.p[1].y, tri.p[2].y));
			float max_x=std::max(tri.p[0].x, std::max(tri.p[1].x, tri.p[2].x));
			float max_y=std::max(tri.p[0].y, std::max(tri.p[1].y, tri.p[2].y));

			//which tiles does it overlap
			int sx=std::clamp(int(min_x/tile_size), 0, tiles_x-1);
			int sy=std::clamp(int(min_y/tile_size), 0, tiles_y-1);
			int ex=std::clamp(int(max_x/tile_size), 0, tiles_x-1);
			int ey=std::clamp(int(max_y/tile_size), 0, tiles_y-1);

			for(int tx=sx; tx<=ex; tx++) {
				for(int ty=sy; ty<=ey; ty++) {
					tile_bins[tx+tiles_x*ty].push_back(tri);
				}
			}
		}

		//queue work
		std::atomic<int> tiles_remaining=tile_jobs.size();
		for(const auto& j:tile_jobs) {
			raster_pool->enqueue([=, &tiles_remaining] {
				int nx=std::clamp(tile_size*j.x, 0, ScreenWidth());
				int ny=std::clamp(tile_size*j.y, 0, ScreenHeight());
				int mx=std::clamp(tile_size+nx, 0, ScreenWidth());
				int my=std::clamp(tile_size+ny, 0, ScreenHeight());
				for(const auto& t:tile_bins[j.x+tiles_x*j.y]) {
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
			FillDepthTriangle_new(
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
	Example e;
	bool vsync=true;
	if(e.Construct(640, 480, 1, 1, false, vsync)) e.Start();

	return 0;
}