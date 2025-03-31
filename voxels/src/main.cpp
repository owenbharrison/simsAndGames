#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "math/vf3d.h"

#include "math/mat4.h"

#include "gfx/mesh.h"

#include "stopwatch.h"

#include "math/v2d.h"

//vector-matrix multiplication
vf3d operator*(const vf3d& v, const Mat4& m) {
	vf3d r;
	r.x=v.x*m.v[0][0]+v.y*m.v[1][0]+v.z*m.v[2][0]+v.w*m.v[3][0];
	r.y=v.x*m.v[0][1]+v.y*m.v[1][1]+v.z*m.v[2][1]+v.w*m.v[3][1];
	r.z=v.x*m.v[0][2]+v.y*m.v[1][2]+v.z*m.v[2][2]+v.w*m.v[3][2];
	r.w=v.x*m.v[0][3]+v.y*m.v[1][3]+v.z*m.v[2][3]+v.w*m.v[3][3];
	return r;
}

vf3d reflect(const vf3d& in, const vf3d& norm) {
	return in-2*norm.dot(in)*norm;
}

float map(float x, float a, float b, float c, float d) {
	float t=(x-a)/(b-a);
	return c+t*(d-c);
}

#include "render.h"

struct VoxelGame : olc::PixelGameEngine {
	VoxelGame() {
		sAppName="Voxel Game";
	}

	Mesh model;

	float* depth_buffer=nullptr;
	float* u_buffer=nullptr;
	float* v_buffer=nullptr;

	//camera matrix
	Mat4 mat_proj;

	//camera positioning
	vf3d cam_pos{0, 0, -5};
	float cam_yaw=0;
	float cam_pitch=0;

	//graphics stuff
	vf3d light_pos=cam_pos;
	bool show_outlines=false;
	bool to_time=false;
	enum struct Debug {
		NONE=0,
		DEPTH,
		UV
	};
	Debug debug_view=Debug::NONE;

	olc::Sprite* texture=nullptr;
	olc::Sprite* white_texture=nullptr;

	Render cam1;
	olc::Sprite* cam1_spr=nullptr;

	bool OnUserCreate() override {
		srand(time(0));

		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";
		ConsoleCaptureStdOut(true);
		
		//make projection matrix
		float fov=90.f;
		float asp=float(ScreenHeight())/ScreenWidth();
		mat_proj=Mat4::makeProj(fov, asp, .1f, 1000.f);

		depth_buffer=new float[ScreenWidth()*ScreenHeight()];
		u_buffer=new float[ScreenWidth()*ScreenHeight()];
		v_buffer=new float[ScreenWidth()*ScreenHeight()];

		//load model, rescale, load texture
		model=Mesh::loadFromOBJ("assets/models/cube.txt");
		model.normalize(2);
		texture=new olc::Sprite("assets/textures/mario.png");

		white_texture=new olc::Sprite(1, 1);
		white_texture->SetPixel(0, 0, olc::WHITE);

		//set triangle colors
		for(auto& t:model.triangles) {
			vf3d norm=t.getNorm();
			t.col.r=128+127*norm.x;
			t.col.g=128+127*norm.y;
			t.col.b=128+127*norm.z;
		}

		//setup render
		{
			int width=64;
			int height=48;
			cam1=Render(width, height, 90.f);
			cam1.updatePos({4, 4, 4}, {-1, -1, -1}, {4, 4, 4});
			cam1.render(model.triangles);
		
			cam1_spr=new olc::Sprite(cam1.getWidth(), cam1.getHeight());
			for(int i=0; i<cam1.getWidth(); i++) {
				for(int j=0; j<cam1.getHeight(); j++) {
					int k=cam1.ix(i, j);
					cam1_spr->SetPixel(i, j, olc::Pixel(
						cam1.pixels[3*k],
						cam1.pixels[1+3*k],
						cam1.pixels[2+3*k],
						255
					));
				}
			}
		}

		return true;
	}

	void TexturedTriangle(
		int x1, int y1, float u1, float v1, float w1,
		int x2, int y2, float u2, float v2, float w2,
		int x3, int y3, float u3, float v3, float w3,
		olc::Pixel col//olc::Sprite* s
	) {
		//sort by y
		if(y2<y1) {
			std::swap(x1, x2);
			std::swap(y1, y2);
			std::swap(u1, u2);
			std::swap(v1, v2);
			std::swap(w1, w2);
		}
		if(y3<y1) {
			std::swap(x1, x3);
			std::swap(y1, y3);
			std::swap(u1, u3);
			std::swap(v1, v3);
			std::swap(w1, w3);
		}
		if(y3<y2) {
			std::swap(x2, x3);
			std::swap(y2, y3);
			std::swap(u2, u3);
			std::swap(v2, v3);
			std::swap(w2, w3);
		}

		//calculate slopes
		int dx1=x2-x1;
		int dy1=y2-y1;
		float du1=u2-u1;
		float dv1=v2-v1;
		float dw1=w2-w1;

		int dx2=x3-x1;
		int dy2=y3-y1;
		float du2=u3-u1;
		float dv2=v3-v1;
		float dw2=w3-w1;

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
				float t_step=1.f/(bx-ax);
				float t=0;
				for(int i=ax; i<bx; i++) {
					tex_u=tex_su+t*(tex_eu-tex_su);
					tex_v=tex_sv+t*(tex_ev-tex_sv);
					tex_w=tex_sw+t*(tex_ew-tex_sw);
					float& depth=depth_buffer[i+ScreenWidth()*j];
					if(tex_w>depth) {
						Draw(i, j, col);//s->Sample(tex_u/tex_w, tex_v/tex_w));
						depth=tex_w;
						u_buffer[i+ScreenWidth()*j]=tex_u;
						v_buffer[i+ScreenWidth()*j]=tex_v;
					}
					t+=t_step;
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
			float t=0;
			for(int i=ax; i<bx; i++) {
				tex_u=tex_su+t*(tex_eu-tex_su);
				tex_v=tex_sv+t*(tex_ev-tex_sv);
				tex_w=tex_sw+t*(tex_ew-tex_sw);
				float& depth=depth_buffer[i+ScreenWidth()*j];
				if(tex_w>depth) {
					Draw(i, j, col);//s->Sample(tex_u/tex_w, tex_v/tex_w));
					depth=tex_w;
					u_buffer[i+ScreenWidth()*j]=tex_u;
					v_buffer[i+ScreenWidth()*j]=tex_v;
				}
				t+=t_step;
			}
		}
	}

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") {
			ConsoleClear();

			return true;
		}

		if(cmd=="time") {
			to_time=true;

			return true;
		}

		if(cmd=="import") {
			std::string filename;
			line_str>>filename;
			if(filename.empty()) {
				std::cout<<"no filename. try using:\n  import <filename>\n";

				return false;
			}

			model=Mesh::loadFromOBJ(filename);
			model.normalize(2);

			return true;
		}

		if(cmd=="keybinds") {
			std::cout<<"useful keybinds:\n"
				"  SPACE    move up\n"
				"  SHIFT    move down\n"
				"  WASD     move camera\n"
				"  ARROWS   look camera\n"
				"  ENTER    set light pos\n"
				"  O        toggle outlines\n"
				"  B        depth debug view\n"
				"  U        uv debug view\n"
				"  N        no debug\n"
				"  ESC      toggle integrated console\n";

			return true;
		}

		if(cmd=="help") {
			std::cout<<"useful commands:\n"
				"  clear      clears the console\n"
				"  time       times immediate next update and render loop\n"
				"  import     import model from file\n"
				"  keybinds   which keys to press for this program?\n";

			return true;
		}

		std::cout<<"unknown command. type help for list of commands.\n";

		return false;
	}

	bool OnUserUpdate(float dt) override {
		Stopwatch geom_watch, render_watch;

#pragma region USER INPUT
		//open and close the integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		//only allow input when console NOT open
		if(!IsConsoleShowing()) {
			//look up, down
			if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
			if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
			if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
			if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;

			//look left, right
			if(GetKey(olc::Key::LEFT).bHeld) cam_yaw+=dt;
			if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw-=dt;

			//move up, down
			if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*dt;
			if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*dt;

			//move forward, backward
			vf3d fb_dir(std::sinf(cam_yaw), 0, std::cosf(cam_yaw));
			if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*dt*fb_dir;
			if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*dt*fb_dir;

			//move left, right
			vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
			if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*dt*lr_dir;
			if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*dt*lr_dir;

			//set light to camera
			if(GetKey(olc::Key::ENTER).bHeld) light_pos=cam_pos;

			//ui toggles
			if(GetKey(olc::Key::O).bPressed) show_outlines^=true;

			if(GetKey(olc::Key::B).bPressed) debug_view=Debug::DEPTH;
			if(GetKey(olc::Key::U).bPressed) debug_view=Debug::UV;
			if(GetKey(olc::Key::N).bPressed) debug_view=Debug::NONE;
		}
#pragma endregion

#pragma region GEOMETRY AND CLIPPING
		if(to_time) geom_watch.start();

		const vf3d up(0, 1, 0);
		vf3d look_dir(
			std::sinf(cam_yaw)*std::cosf(cam_pitch),
			std::sinf(cam_pitch),
			std::cosf(cam_yaw)*std::cosf(cam_pitch)
		);
		vf3d target=cam_pos+look_dir;

		Mat4 mat_cam=Mat4::makePointAt(cam_pos, target, up);
		Mat4 mat_view=Mat4::quickInverse(mat_cam);

		std::list<Triangle> tris_to_clip;
		for(const auto& tri:model.triangles) {
			vf3d norm=tri.getNorm();

			//is triangle pointing towards me? culling
			if(norm.dot(tri.p[0]-cam_pos)<0) {
				//lighting
				vf3d light_dir=(light_pos-tri.getCtr()).norm();
				float dp=std::max(.5f, norm.dot(light_dir));

				//transform triangles given camera positioning
				Triangle tri_view;
				for(int i=0; i<3; i++) {
					tri_view.p[i]=tri.p[i]*mat_view;
					tri_view.t[i]=tri.t[i];
				}
				tri_view.col=tri.col*dp;

				//clip against near plane
				Triangle clipped[2];
				int num=tri_view.clipAgainstPlane(vf3d(0, 0, .1f), vf3d(0, 0, 1), clipped[0], clipped[1]);
				for(int i=0; i<num; i++) {
					Triangle tri_proj;
					//for each vert
					for(int j=0; j<3; j++) {
						//project
						tri_proj.p[j]=clipped[i].p[j]*mat_proj;

						//copy over texture stuff
						tri_proj.t[j]=clipped[i].t[j];

						//w normalization?
						float w=tri_proj.p[j].w;
						tri_proj.t[j].u/=w;
						tri_proj.t[j].v/=w;
						tri_proj.t[j].w=1/w;

						//scale into view
						tri_proj.p[j]/=w;

						//x/y inverted so put them back
						tri_proj.p[j].x*=-1;
						tri_proj.p[j].y*=-1;

						//offset into visible normalized space
						tri_proj.p[j]+=vf3d(1, 1, 0);
						tri_proj.p[j].x*=ScreenWidth()/2;
						tri_proj.p[j].y*=ScreenHeight()/2;
					}
					tri_proj.col=clipped[i].col;

					tris_to_clip.emplace_back(tri_proj);
				}
			}
		}

		/*painters algorithm?
		tris_to_clip.sort([] (const Triangle& a, const Triangle& b) {
			float a_z=a.p[0].z+a.p[1].z+a.p[2].z;
			float b_z=b.p[0].z+b.p[1].z+b.p[2].z;
			return a_z>b_z;
		});*/

		if(to_time) {
			geom_watch.stop();
			auto dur=geom_watch.getMicros();
			std::cout<<"geom: "<<dur<<"us ("<<(dur/1000.f)<<" ms)\n";
		}
#pragma endregion

#pragma region RENDER
		if(to_time) render_watch.start();

		//dark grey background
		Clear(olc::Pixel(50, 50, 50));

		//reset buffers
		for(int i=0; i<ScreenWidth()*ScreenHeight(); i++) {
			depth_buffer[i]=0;
			u_buffer[i]=0;
			v_buffer[i]=0;
		}

		//screen space clipping!
		std::list<Triangle> tri_queue;
		std::list<Triangle> tris_to_draw;
		for(const auto& t:tris_to_clip) {
			Triangle clipped[2];
			tri_queue={t};
			int num_new=1;
			for(int i=0; i<4; i++) {
				//choose plane on screen edge to clip with
				vf3d ctr, norm;
				switch(i) {
					case 0://left
						ctr=vf3d(0, 0, 0);
						norm=vf3d(1, 0, 0);
						break;
					case 1://right
						ctr=vf3d(ScreenWidth(), 0, 0);
						norm=vf3d(-1, 0, 0);
						break;
					case 2://top
						ctr=vf3d(0, 0, 0);
						norm=vf3d(0, 1, 0);
						break;
					case 3://bottom
						ctr=vf3d(0, ScreenHeight(), 0);
						norm=vf3d(0, -1, 0);
						break;
				}

				//iteratively clip all tris
				while(num_new>0) {
					Triangle test=tri_queue.front();
					tri_queue.pop_front();
					num_new--;

					int num_clip=test.clipAgainstPlane(ctr, norm, clipped[0], clipped[1]);
					for(int j=0; j<num_clip; j++) tri_queue.emplace_back(clipped[j]);
				}
				num_new=tri_queue.size();
			}

			//add to conglomerate
			tris_to_draw.insert(tris_to_draw.end(), tri_queue.begin(), tri_queue.end());
		}

		//rasterize all triangles
		for(const auto& t:tris_to_draw) {
			TexturedTriangle(
				t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
				t.col//model.textured?texture:white_texture
			);
		}
		switch(debug_view) {
			case Debug::DEPTH: {
				//find min and max values
				float min_depth=INFINITY, max_depth=-min_depth;
				for(int i=0; i<ScreenWidth()*ScreenHeight(); i++) {
					float depth=depth_buffer[i];
					if(depth<min_depth) min_depth=depth;
					if(depth>max_depth) max_depth=depth;
				}

				//show all depth values
				for(int i=0; i<ScreenWidth(); i++) {
					for(int j=0; j<ScreenHeight(); j++) {
						float depth=depth_buffer[i+ScreenWidth()*j];
						float shade=map(depth, min_depth, max_depth, 0, 1);
						Draw(i, j, olc::PixelF(shade, shade, shade));
					}
				}

				//debug view string
				DrawString({0, 0}, "showing depth", olc::WHITE);

				break;
			}
			case Debug::UV: {
				//show all uv as red and green values
				for(int i=0; i<ScreenWidth(); i++) {
					for(int j=0; j<ScreenHeight(); j++) {
						olc::Pixel col=olc::PixelF(
							u_buffer[i+ScreenWidth()*j],
							v_buffer[i+ScreenWidth()*j],
							0
						);
						Draw(i, j, col);
					}
				}

				//debug view string
				DrawString({0, 0}, "showing uv", olc::WHITE);
				
				break;
			}
		}

		//show white triangle outlines
		if(show_outlines) {
			for(const auto& t:tris_to_draw) {
				DrawTriangle(
					vf2d(t.p[0].x, t.p[0].y),
					vf2d(t.p[1].x, t.p[1].y),
					vf2d(t.p[2].x, t.p[2].y),
					olc::WHITE
				);
			}
		}

		DrawSprite(GetMouseX(), GetMouseY(), cam1_spr);

		if(to_time) {
			//end timing cycle
			to_time=false;

			render_watch.stop();
			auto dur=render_watch.getMicros();
			std::cout<<"render: "<<dur<<"us ("<<(dur/1000.f)<<" ms)\n";
		}
#pragma endregion

		return true;
	}
};

int main() {
	VoxelGame vg;
	bool vsync=true;
	if(vg.Construct(640, 480, 1, 1, false, vsync)) vg.Start();

	return 0;
}