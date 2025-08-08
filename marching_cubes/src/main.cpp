#include "common/3d/engine_3d.h"
using olc::vf2d;
using cmn::vf3d;
using cmn::Mat4;

constexpr float Pi=3.1415927f;

#include "perlin_noise.h"

#include "lookup.h"

float random01() {
	static constexpr float rand_max=RAND_MAX;
	return rand()/rand_max;
}

struct Example : cmn::Engine3D {
	Example() {
		sAppName="3d builder";
	}

	//camera positioning
	float cam_yaw=Pi/4;
	float cam_pitch=-.1f;
	vf3d light_pos;

	int width=0, height=0, depth=0;
	float* values=nullptr;
	int ix(int i, int j, int k) const {
		return i+width*(j+height*k);
	}

	PerlinNoise noise_gen;
	vf3d noise_offset;

	float total_dt=0;

	bool show_grid=true;

	float surf=.5f;

	bool user_create() override {
		srand(time(0));

		//w, h, d = [5, 10]
		width=5+rand()%6;
		height=5+rand()%6;
		depth=5+rand()%6;

		cam_pos=vf3d(-2, .5f*height, -2);
		light_pos=vf3d(.5f*width, 5+height, .5f*depth);

		noise_gen=PerlinNoise(time(0));

		values=new float[width*height*depth];

		return true;
	}

	//mostly camera controls
	bool user_update(float dt) override {
		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;

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

		{//size changes
			int inc=1;
			bool edit=false;
			if(GetKey(olc::Key::CTRL).bHeld) inc=-1;
			
			if(GetKey(olc::Key::X).bPressed) edit=true, width+=inc;
			if(width<2) width=2;
			if(width>50) width=50;

 			if(GetKey(olc::Key::Y).bPressed) edit=true, height+=inc;
			if(height<2) height=2;
			if(height>50) height=50;

			if(GetKey(olc::Key::Z).bPressed) edit=true, depth+=inc;
			if(depth<2) depth=2;
			if(depth>50) depth=50;

			//reallocate
			if(edit) values=new float[width*height*depth];
		}

		//debug toggles
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;

		//move along time
		if(GetKey(olc::Key::ENTER).bHeld) {
			//find smooth direction change
			float nx=noise_gen.noise(total_dt, 2*total_dt, 3*total_dt);
			float ny=noise_gen.noise(100+3*total_dt, 100+total_dt, 100+2*total_dt);
			float nz=noise_gen.noise(200+2*total_dt, 200+3*total_dt, 200+total_dt);
			
			vf3d dir(nx, ny, nz);

			//step along change
			noise_offset+=dir*dt;
			total_dt+=dt;
		}

		//fill grid
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				for(int k=0; k<depth; k++) {
					values[ix(i, j, k)]=noise_gen.noise(
						noise_offset.x+.1f*i,
						noise_offset.y+.1f*j,
						noise_offset.z+.1f*k
					);
				}
			}
		}

		return true;
	}

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});
		
		for(int i=0; i<width-1; i++) {
			float min_x=i, max_x=1+i;
			for(int j=0; j<height-1; j++) {
				float min_y=j, max_y=1+j;
				for(int k=0; k<depth-1; k++) {
					float min_z=k, max_z=1+k;
					//get cube corners
					vf3d c[8]{
						{min_x, min_y, min_z},
						{max_x, min_y, min_z},
						{min_x, max_y, min_z},
						{max_x, max_y, min_z},
						{min_x, min_y, max_z},
						{max_x, min_y, max_z},
						{min_x, max_y, max_z},
						{max_x, max_y, max_z}
					};
					//get corner values
					float v[8]{
						values[ix(i, j, k)],
						values[ix(i+1, j, k)],
						values[ix(i, j+1, k)],
						values[ix(i+1, j+1, k)],
						values[ix(i, j, k+1)],
						values[ix(i+1, j, k+1)],
						values[ix(i, j+1, k+1)],
						values[ix(i+1, j+1, k+1)]
					};
					//get cube state
					int state=
						128*(v[7]<surf)+
						64*(v[6]<surf)+
						32*(v[5]<surf)+
						16*(v[4]<surf)+
						8*(v[3]<surf)+
						4*(v[2]<surf)+
						2*(v[1]<surf)+
						1*(v[0]<surf);
					//triangulate
					const int* tris=triangulation_table[state];
					for(int l=0; l<12; l+=3) {
						if(tris[l]==-1) break;
						//get edges
						const int* e0=edge_indexes[tris[l]];
						const int* e1=edge_indexes[tris[l+1]];
						const int* e2=edge_indexes[tris[l+2]];
						//get interpolators
						float t0=(surf-v[e0[0]])/(v[e0[1]]-v[e0[0]]);
						float t1=(surf-v[e1[0]])/(v[e1[1]]-v[e1[0]]);
						float t2=(surf-v[e2[0]])/(v[e2[1]]-v[e2[0]]);
						tris_to_project.push_back({
							c[e0[0]]+t0*(c[e0[1]]-c[e0[0]]),
							c[e1[0]]+t1*(c[e1[1]]-c[e1[0]]),
							c[e2[0]]+t2*(c[e2[1]]-c[e2[0]])
						});
					}
				}
			}
		}

		if(show_grid) {
			//show x lines
			const float min_x=0, max_x=width-1;
			for(int j=0; j<height; j++) {
				float y=j;
				for(int k=0; k<depth; k++) {
					float z=k;
					cmn::Line l{vf3d(min_x, y, z), vf3d(max_x, y, z)};
					l.col=olc::RED;
					lines_to_project.push_back(l);
				}
			}

			//show y lines
			const float min_y=0, max_y=height-1;
			for(int k=0; k<depth; k++) {
				float z=k;
				for(int i=0; i<width; i++) {
					float x=i;
					cmn::Line l{vf3d(x, min_y, z), vf3d(x, max_y, z)};
					l.col=olc::BLUE;
					lines_to_project.push_back(l);
				}
			}

			//show x lines
			const float min_z=0, max_z=depth-1;
			for(int i=0; i<width; i++) {
				float x=i;
				for(int j=0; j<height; j++) {
					float y=j;
					cmn::Line l{vf3d(x, y, min_z), vf3d(x, y, max_z)};
					l.col=olc::GREEN;
					lines_to_project.push_back(l);
				}
			}
		}

		return true;
	}

	bool user_render() override {
		Clear(olc::BLACK);

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
	Example e;
	bool vsync=true;
	if(e.Construct(540, 360, 1, 1, false, vsync)) e.Start();
	
	return 0;
}