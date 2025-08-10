#include "common/3d/engine_3d.h"
using cmn::vf3d;

#include "common/utils.h"

#include "solver.h"

#include <vector>

struct Billboard {
	vf3d pos;
	float sz=1;
	olc::Pixel col=olc::WHITE;
};

#include "thread_pool.h"

struct Example : cmn::Engine3D {
	Example() {
		sAppName="3D Particles Test";
	}

	//camera positioning
	float cam_yaw=2.5f;
	float cam_pitch=-.6f;
	vf3d light_pos;

	//simulation
	float update_timer=0;
	const float time_step=1/60.f;
	const int num_sub_steps=3;
	const float sub_time_step=time_step/num_sub_steps;

	const vf3d gravity{0, -9.8f, 0};
	Solver* solver=nullptr;

	//graphics
	std::vector<Billboard> billboards;
	bool show_grid=false;
	olc::Sprite* circle_spr=nullptr;

	//multithreaded rendering
	const int mtr_tile_size=64;
	int mtr_num_x=0, mtr_num_y=0;
	std::vector<cmn::Triangle>* mtr_bins=nullptr;

	std::vector<olc::vi2d> mtr_jobs;

	ThreadPool* mtr_pool=nullptr;

	bool user_create() override {
		srand(time(0));

		//set cam pos
		cam_pos={11.71f, 5.36f, -9.58f};
		light_pos=cam_pos;

		//initialize solver
		vf3d sz(15, 10, 20);
		solver=new Solver(5000, {-sz/2, sz/2});

		{//circle sprite for rendering
			int sz=512;
			circle_spr=new olc::Sprite(sz, sz);
			SetDrawTarget(circle_spr);
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2, olc::WHITE);
			SetDrawTarget(nullptr);
		}

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

		return true;
	}

	bool user_destroy() override {
		delete solver;

		delete circle_spr;

		delete[] mtr_bins;
		delete mtr_pool;

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
			std::cos(cam_yaw)*std::cos(cam_pitch),
			std::sin(cam_pitch),
			std::sin(cam_yaw)*std::cos(cam_pitch)
		);

		//move up, down
		if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*dt;
		if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::cos(cam_yaw), 0, std::sin(cam_yaw));
		if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*dt*fb_dir;
		if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*dt*lr_dir;
		if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*dt*lr_dir;

		//set light pos
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//spawn a bunch of particles in random position
		if(GetKey(olc::Key::B).bHeld) {
			const float addition_rad=3;

			//random center position inside solver bounds
			auto box=solver->getBounds();
			vf3d t(cmn::random(), cmn::random(), cmn::random());
			vf3d ctr=box.min+t*(box.max-box.min);
			for(int i=0; i<100; i++) {
				//random pos in addition sphere
				float dist=cmn::random(0, addition_rad);
				vf3d dir(cmn::random(), cmn::random(), cmn::random());
				dir=(.5f-dir).norm();
				vf3d pos=ctr+dist*dir;

				//random size & color
				float rad=cmn::random(.25f, .75f);
				Particle p(pos, rad);
				p.col=olc::Pixel(rand()%256, rand()%256, rand()%256);

				//try to add it
				solver->addParticle(p);
			}
		}

		//debug toggles
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;

		//ensure similar update across multiple framerates
		update_timer+=dt;
		while(update_timer>time_step) {
			for(int i=0; i<num_sub_steps; i++) {
				solver->solveCollisions();

				solver->accelerate(gravity);

				solver->updateKinematics(sub_time_step);
			}

			update_timer-=time_step;
		}

		return true;
	}

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});

		//add solver bounds
		addAABB(solver->getBounds(), olc::BLACK);

		//add particles as billboards
		billboards.clear();
		for(int i=0; i<solver->getNumParticles(); i++) {
			const auto& p=solver->particles[i];

			billboards.push_back({p.pos, 2*p.rad, p.col});
		}

		//sort billboards back to front for transparency
		std::sort(billboards.begin(), billboards.end(), [&] (const Billboard& a, const Billboard& b) {
			return (cam_pos-a.pos).mag2()<(cam_pos-b.pos).mag2();
		});

		//triangulate billboards
		cmn::Triangle t1, t2;
		for(int i=0; i<billboards.size(); i++) {
			const auto& b=billboards[i];

			//billboarded to point at camera	
			vf3d norm=(b.pos-cam_pos).norm();
			vf3d up(0, 1, 0);
			vf3d rgt=norm.cross(up).norm();
			up=rgt.cross(norm);

			//vertex positioning
			vf3d tl=b.pos-b.sz/2*rgt+b.sz/2*up;
			vf3d tr=b.pos+b.sz/2*rgt+b.sz/2*up;
			vf3d bl=b.pos-b.sz/2*rgt-b.sz/2*up;
			vf3d br=b.pos+b.sz/2*rgt-b.sz/2*up;

			//texture coords
			cmn::v2d tl_t{0, 0};
			cmn::v2d tr_t{1, 0};
			cmn::v2d bl_t{0, 1};
			cmn::v2d br_t{1, 1};

			//tessellation
			t1={tl, br, tr, tl_t, br_t, tr_t};
			t2={tl, bl, br, tl_t, bl_t, br_t};
			t1.id=t2.id=i;
			tris_to_project.push_back(t1);
			tris_to_project.push_back(t2);
		}

		if(show_grid) {
			const auto box=solver->getBounds();
			const float sz=solver->getCellSize();

			//along z
			for(int i=0; i<solver->getNumCellX(); i++) {
				for(int j=0; j<solver->getNumCellY(); j++) {
					float x=box.min.x+sz*i;
					float y=box.min.y+sz*j;
					cmn::Line l{vf3d(x, y, box.min.z), vf3d(x, y, box.max.z)};
					l.col=olc::GREEN;
					lines_to_project.push_back(l);
				}
			}

			//along x
			for(int j=0; j<solver->getNumCellY(); j++) {
				for(int k=0; k<solver->getNumCellZ(); k++) {
					float y=box.min.y+sz*j;
					float z=box.min.z+sz*k;
					cmn::Line l{vf3d(box.min.x, y, z), vf3d(box.max.x, y, z)};
					l.col=olc::RED;
					lines_to_project.push_back(l);
				}
			}

			//along y
			for(int k=0; k<solver->getNumCellZ(); k++) {
				for(int i=0; i<solver->getNumCellX(); i++) {
					float z=box.min.z+sz*k;
					float x=box.min.x+sz*i;
					cmn::Line l{vf3d(x, box.min.y, z), vf3d(x, box.max.y, z)};
					l.col=olc::BLUE;
					lines_to_project.push_back(l);
				}
			}
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

	void FillTexturedDepthTriangleWithin(
		int x1, int y1, float u1, float v1, float w1,
		int x2, int y2, float u2, float v2, float w2,
		int x3, int y3, float u3, float v3, float w3,
		const olc::Sprite* spr, olc::Pixel tint, int id,
		int nx, int ny, int mx, int my
	) {
		//sort by y
		if(y2<y1) {
			std::swap(x1, x2), std::swap(y1, y2);
			std::swap(u1, u2), std::swap(v1, v2);
			std::swap(w1, w2);
		}
		if(y3<y1) {
			std::swap(x1, x3), std::swap(y1, y3);
			std::swap(u1, u3), std::swap(v1, v3);
			std::swap(w1, w3);
		}
		if(y3<y2) {
			std::swap(x2, x3), std::swap(y2, y3);
			std::swap(u2, u3), std::swap(v2, v3);
			std::swap(w2, w3);
		}

		//calculate slopes
		int dx1=x2-x1, dy1=y2-y1;
		float du1=u2-u1, dv1=v2-v1;
		float dw1=w2-w1;

		int dx2=x3-x1, dy2=y3-y1;
		float du2=u3-u1, dv2=v3-v1;
		float dw2=w3-w1;

		float t_step, t;

		float tex_u, tex_v, tex_w;

		float dax_step=0, dbx_step=0,
			du1_step=0, dv1_step=0,
			du2_step=0, dv2_step=0,
			dw1_step=0, dw2_step=0;

		if(dy1) dax_step=dx1/std::fabsf(dy1);
		if(dy2) dbx_step=dx2/std::fabsf(dy2);

		if(dy1) du1_step=du1/std::fabsf(dy1);
		if(dy1) dv1_step=dv1/std::fabsf(dy1);
		if(dy1) dw1_step=dw1/std::fabsf(dy1);
		if(dy2) du2_step=du2/std::fabsf(dy2);
		if(dy2) dv2_step=dv2/std::fabsf(dy2);
		if(dy2) dw2_step=dw2/std::fabsf(dy2);

		//start scanline filling triangles
		if(dy1) {
			for(int j=y1; j<=y2; j++) {
				if(j<ny||j>=my) continue;
				int ax=x1+dax_step*(j-y1);
				int bx=x1+dbx_step*(j-y1);
				float tex_su=u1+du1_step*(j-y1);
				float tex_sv=v1+dv1_step*(j-y1);
				float tex_sw=w1+dw1_step*(j-y1);
				float tex_eu=u1+du2_step*(j-y1);
				float tex_ev=v1+dv2_step*(j-y1);
				float tex_ew=w1+dw2_step*(j-y1);
				//sort along x
				if(ax>bx) {
					std::swap(ax, bx);
					std::swap(tex_su, tex_eu);
					std::swap(tex_sv, tex_ev);
					std::swap(tex_sw, tex_ew);
				}
				t_step=1.f/(bx-ax);
				for(int i=ax; i<bx; i++) {
					if(i<nx||i>=mx) continue;
					t=t_step*(i-ax);
					tex_u=tex_su+t*(tex_eu-tex_su);
					tex_v=tex_sv+t*(tex_ev-tex_sv);
					tex_w=tex_sw+t*(tex_ew-tex_sw);
					float& depth=depth_buffer[bufferIX(i, j)];
					if(tex_w>depth) {
						olc::Pixel col=spr->Sample(tex_u/tex_w, tex_v/tex_w);
						if(col.a!=0) {
							Draw(i, j, tint*col);
							depth=tex_w;
							id_buffer[bufferIX(i, j)]=id;
						}
					}
				}
			}
		}

		//recalculate slopes
		dx1=x3-x2;
		dy1=y3-y2;
		du1=u3-u2;
		dv1=v3-v2;
		dw1=w3-w2;

		if(dy1) dax_step=dx1/std::fabsf(dy1);

		du1_step=0, dv1_step=0;
		if(dy1) du1_step=du1/std::fabsf(dy1);
		if(dy1) dv1_step=dv1/std::fabsf(dy1);
		if(dy1) dw1_step=dw1/std::fabsf(dy1);

		for(int j=y2; j<=y3; j++) {
			if(j<ny||j>=my) continue;
			int ax=x2+dax_step*(j-y2);
			int bx=x1+dbx_step*(j-y1);
			float tex_su=u2+du1_step*(j-y2);
			float tex_sv=v2+dv1_step*(j-y2);
			float tex_sw=w2+dw1_step*(j-y2);
			float tex_eu=u1+du2_step*(j-y1);
			float tex_ev=v1+dv2_step*(j-y1);
			float tex_ew=w1+dw2_step*(j-y1);
			//sort along x
			if(ax>bx) {
				std::swap(ax, bx);
				std::swap(tex_su, tex_eu);
				std::swap(tex_sv, tex_ev);
				std::swap(tex_sw, tex_ew);
			}
			float t_step=1.f/(bx-ax);
			for(int i=ax; i<bx; i++) {
				if(i<nx||i>=mx) continue;
				t=t_step*(i-ax);
				tex_u=tex_su+t*(tex_eu-tex_su);
				tex_v=tex_sv+t*(tex_ev-tex_sv);
				tex_w=tex_sw+t*(tex_ew-tex_sw);
				float& depth=depth_buffer[bufferIX(i, j)];
				if(tex_w>depth) {
					olc::Pixel col=spr->Sample(tex_u/tex_w, tex_v/tex_w);
					if(col.a!=0) {
						Draw(i, j, tint*col);
						depth=tex_w;
						id_buffer[bufferIX(i, j)]=id;
					}
				}
			}
		}
	}
#pragma endregion

	bool user_render() override {
		Clear(olc::Pixel(100, 100, 100));

		//render 3d stuff
		resetBuffers();

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
		SetPixelMode(olc::Pixel::ALPHA);
		std::atomic<int> tiles_remaining=mtr_jobs.size();
		for(const auto& j:mtr_jobs) {
			mtr_pool->enqueue([&] {
				int nx=std::clamp(mtr_tile_size*j.x, 0, ScreenWidth());
				int ny=std::clamp(mtr_tile_size*j.y, 0, ScreenHeight());
				int mx=std::clamp(mtr_tile_size+nx, 0, ScreenWidth());
				int my=std::clamp(mtr_tile_size+ny, 0, ScreenHeight());
				const auto& bin=mtr_bins[j.x+mtr_num_x*j.y];
				for(const auto& t:bin) {
					FillTexturedDepthTriangleWithin(
						t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
						t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
						t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
						circle_spr, billboards[t.id].col, t.id,
						nx, ny, mx, my
					);
				}
				tiles_remaining--;
			});
		}

		//wait for raster to finish
		while(tiles_remaining);
		SetPixelMode(olc::Pixel::NORMAL);

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