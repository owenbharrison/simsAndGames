#define OLC_PGE_APPLICATION
#include "common/3d/engine_3d.h"

#define MULTITHREADING
#ifdef __EMSCRIPTEN__
#undef MULTITHREADING
#endif

using cmn::vf3d;

#include "mesh.h"

#include "particle.h"

#include "common/utils.h"

#ifdef MULTITHREADING
#include "thread_pool.h"
#endif

struct FXUI : cmn::Engine3D {
	FXUI() {
		sAppName="3D Particle Effects";
	}

	//camera positioning
	float cam_yaw=1.07f;
	float cam_pitch=-.56f;
	vf3d light_pos;

	//ui stuff
	vf3d mouse_dir;

	float smoke_timer=0;

	bool update_physics=true;

	//scene stuff
	Mesh terrain;

	std::list<Particle> particles;
	vf3d buoyancy{0, 4.9f, 0};

	//graphics stuff
	std::vector<olc::Sprite*> texture_atlas;
	olc::Sprite* fire_gradient=nullptr;

	//multithreaded rendering
#ifdef MULTITHREADING
	const int tile_size=32;
	int tiles_x=0, tiles_y=0;
	std::vector<cmn::Triangle>* tile_bins=nullptr;
	
	std::vector<olc::vi2d> tile_jobs;
	
	ThreadPool* raster_pool=nullptr;
#endif

	bool user_create() override {
		cam_pos={-.47f, 9.19f, -8.36f};

		//try load terrain model
		try {
			terrain=Mesh::loadFromOBJ("assets/models/terrain.txt");
			terrain.colorNormals();
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

		//load textures
		texture_atlas.resize(Particle::NUM_TYPES);
		texture_atlas[Particle::SMOKE]=new olc::Sprite("assets/img/smoke.png");
		//texture_atlas[Particle::LIGHTNING]=new olc::Sprite("assets/img/something.png");

		fire_gradient=new olc::Sprite("assets/img/fire_gradient.png");

#ifdef MULTITHREADING
		//allocate tiles
		tiles_x=1+ScreenWidth()/tile_size;
		tiles_y=1+ScreenHeight()/tile_size;
		tile_bins=new std::vector<cmn::Triangle>[tiles_x*tiles_y];

		//allocate jobs
		for(int i=0; i<tiles_x; i++) {
			for(int j=0; j<tiles_y; j++) {
				tile_jobs.push_back({i, j});
			}
		}

		//make thread pool
		raster_pool=new ThreadPool(std::thread::hardware_concurrency());
#endif

		return true;
	}

	bool user_destroy() override {
		for(auto& t:texture_atlas) delete t;
		delete fire_gradient;

#ifdef MULTITHREADING
		delete[] tile_bins;
		delete raster_pool;
#endif

		return true;
	}

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

		//debug toggle
		if(GetKey(olc::Key::ENTER).bPressed) update_physics^=true;

		//update mouse ray
		{
			//unprojection matrix
			cmn::Mat4 inv_vp=cmn::Mat4::inverse(mat_view*mat_proj);

			//get ray thru screen mouse pos
			float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
			float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
			vf3d clip(ndc_x, ndc_y, 1);
			vf3d world=clip*inv_vp;
			world/=world.w;
			mouse_dir=(world-cam_pos).norm();
		}

		//add particles at mouse
		if(smoke_timer<0) {
			smoke_timer+=1/60.f;

			if(GetMouse(olc::Mouse::LEFT).bHeld) {
				//cast ray thru "scene"
				float dist=terrain.intersectRay(cam_pos, mouse_dir);
				if(dist>0) {
					vf3d pt=cam_pos+dist*mouse_dir;

					vf3d dir(cmn::randFloat(-1, 1), cmn::randFloat(-1, 1), cmn::randFloat(-1, 1));
					float speed=cmn::randFloat(1, 2);
					vf3d vel=speed*dir.norm();
					float rot=cmn::randFloat(2*cmn::Pi);
					float rot_vel=cmn::randFloat(-1.5f, 1.5f);
					float size=cmn::randFloat(.5f, 1.5f);
					float size_vel=cmn::randFloat(.5f, .9f);
					float lifespan=cmn::randFloat(2, 4);
					particles.push_back(Particle(Particle::SMOKE, pt, vel, rot, rot_vel, size, size_vel, lifespan));
				}
			}
		}
		smoke_timer-=dt;

		if(update_physics) {
			//update particles
			for(auto& p:particles) {
				//drag
				p.vel*=1-.95f*dt;
				p.rot_vel*=1-.55f*dt;
				p.size_vel*=1-.45f*dt;

				//kinematics
				p.accelerate(buoyancy);
				p.update(dt);
			}

			//remove dead particles
			for(auto it=particles.begin(); it!=particles.end();) {
				if(it->isDead()) {
					it=particles.erase(it);
				} else it++;
			}
		}

		return true;
	}

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});
		
		//add terrain mesh
		tris_to_project.insert(tris_to_project.end(),
			terrain.triangles.begin(), terrain.triangles.end()
		);

		//sort particles by camera dist for transparency
		particles.sort([&] (const Particle& a, const Particle& b) {
			return (a.pos-cam_pos).mag2()>(b.pos-cam_pos).mag2();
		});

		//add particles as billboards
		for(const auto& p:particles) {
			//basis vectors to point at camera
			vf3d norm=(p.pos-cam_pos).norm();
			vf3d up(0, 1, 0);
			vf3d rgt=norm.cross(up).norm();
			up=rgt.cross(norm);

			//rotate basis vectors w/ rotation matrix
			float c=std::cos(p.rot), s=std::sin(p.rot);
			vf3d new_rgt=c*rgt+s*up;
			vf3d new_up=-s*rgt+c*up;

			//vertex positioning
			vf3d tl=p.pos-p.size/2*new_rgt+p.size/2*new_up;
			vf3d tr=p.pos+p.size/2*new_rgt+p.size/2*new_up;
			vf3d bl=p.pos-p.size/2*new_rgt-p.size/2*new_up;
			vf3d br=p.pos+p.size/2*new_rgt-p.size/2*new_up;

			//texture coords
			cmn::v2d tl_t{0, 0};
			cmn::v2d tr_t{1, 0};
			cmn::v2d bl_t{0, 1};
			cmn::v2d br_t{1, 1};

			//tessellation
			float life=1-p.age/p.lifespan;
			olc::Pixel col=fire_gradient->Sample(1-life, 0);
			col.a=255*life;
			cmn::Triangle t1{tl, br, tr, tl_t, br_t, tr_t}; t1.col=col, t1.id=Particle::SMOKE;
			tris_to_project.push_back(t1);
			cmn::Triangle t2{tl, bl, br, tl_t, bl_t, br_t}; t2.col=col, t2.id=Particle::SMOKE;
			tris_to_project.push_back(t2);
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
		olc::Sprite* spr, olc::Pixel tint, int id,
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
						Draw(i, j, tint*spr->Sample(tex_u/tex_w, tex_v/tex_w));
						depth=tex_w;
						id_buffer[bufferIX(i, j)]=id;
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
					Draw(i, j, tint*spr->Sample(tex_u/tex_w, tex_v/tex_w));
					depth=tex_w;
					id_buffer[bufferIX(i, j)]=id;
				}
			}
		}
	}
#pragma endregion

	bool user_render() override {
		Clear(olc::Pixel(70, 70, 70));

		//render 3d stuff
		resetBuffers();

		SetPixelMode(olc::Pixel::ALPHA);
#ifdef MULTITHREADING
		//clear tile bins
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
			raster_pool->enqueue([&] {
				int nx=std::clamp(tile_size*j.x, 0, ScreenWidth());
				int ny=std::clamp(tile_size*j.y, 0, ScreenHeight());
				int mx=std::clamp(tile_size+nx, 0, ScreenWidth());
				int my=std::clamp(tile_size+ny, 0, ScreenHeight());
				for(const auto& t:tile_bins[j.x+tiles_x*j.y]) {
					if(t.id==-1) {
						FillDepthTriangleWithin(
							t.p[0].x, t.p[0].y, t.t[0].w,
							t.p[1].x, t.p[1].y, t.t[1].w,
							t.p[2].x, t.p[2].y, t.t[2].w,
							t.col, t.id,
							nx, ny, mx, my
						);
					} else {
						FillTexturedDepthTriangleWithin(
							t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
							t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
							t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
							texture_atlas[t.id], t.col, t.id,
							nx, ny, mx, my
						);
					}
				}
				tiles_remaining--;
			});
		}

		//wait for raster to finish
		while(tiles_remaining);
#else
		for(const auto& t:tris_to_draw) {
			if(t.id==-1) {
				FillDepthTriangle(
					t.p[0].x, t.p[0].y, t.t[0].w,
					t.p[1].x, t.p[1].y, t.t[1].w,
					t.p[2].x, t.p[2].y, t.t[2].w,
					t.col, t.id
				);
			} else {
				FillTexturedDepthTriangle(
					t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
					t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
					t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
					texture_atlas[t.id], t.col, t.id
				);
			}
		}
#endif
		SetPixelMode(olc::Pixel::NORMAL);

		//counts of each particle type
		{
			int counts[Particle::NUM_TYPES];
			std::memset(counts, 0, sizeof(int)*Particle::NUM_TYPES);
			for(const auto& p:particles) {
				counts[p.type]++;
			}

			//display particle counts
			int y=0;
			for(int t=0; t<Particle::NUM_TYPES; t++) {
				const auto& c=counts[t];
				if(c==0) continue;

				std::string name="unknown";
				switch(t) {
					case Particle::SMOKE: name="SMOKE"; break;
					case Particle::LIGHTNING: name="LIGHTNING"; break;
				}

				DrawString(0, y, name+": "+std::to_string(c));
				y+=8;
			}
		}

		return true;
	}
};

int main() {
	FXUI f;
	bool vsync=true;
	if(f.Construct(640, 480, 1, 1, false, vsync)) f.Start();

	return 0;
}