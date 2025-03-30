#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "math/vf3d.h"
#include "math/mat4.h"
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

//matrix-matrix multiplication
Mat4 operator*(const Mat4& a, const Mat4& b) {
	Mat4 m;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
			float sum=0;
			for(int k=0; k<4; k++) sum+=a.v[i][k]*b.v[k][j];
			m.v[i][j]=sum;
		}
	}
	return m;
}

#include "gfx/mesh.h"

#include "stopwatch.h"

vf3d reflect(const vf3d& in, const vf3d& norm) {
	return in-2*norm.dot(in)*norm;
}

struct VoxelGame : olc::PixelGameEngine {
	VoxelGame() {
		sAppName="Voxel Game";
	}

	Mesh model;
	Mat4 mat_proj;

	vf3d cam_pos{0, 0, -5};
	float cam_yaw=0;
	float cam_pitch=0;

	vf3d light_pos;
	bool show_outlines=false;

	olc::Sprite* texture=nullptr;

	bool OnUserCreate() override {
		mat_proj=Mat4::makeProj(90.f, float(ScreenHeight())/ScreenWidth(), .1f, 1000.f);

		light_pos=cam_pos;

		model=Mesh::loadFromOBJ("assets/models/cube.txt");
		model.normalize(2);
		texture=new olc::Sprite("assets/textures/mario.png");

		return true;
	}

	int clipAgainstPlane(const Triangle& tri, const vf3d& ctr, const vf3d& norm, Triangle& a, Triangle& b) {
		const vf3d* in_pts[3];
		const v2d* in_tex[3];
		int in_ct=0;
		const vf3d* out_pts[3];
		const v2d* out_tex[3];
		int out_ct=0;

		//classify each point based on
		for(int i=0; i<3; i++) {
			//what side of plane it is on
			if(norm.dot(tri.p[i]-ctr)>0) {
				in_pts[in_ct]=&tri.p[i];
				in_tex[in_ct]=&tri.t[i];
				in_ct++;
			}
			else {
				out_pts[out_ct]=&tri.p[i];
				out_tex[out_ct]=&tri.t[i];
				out_ct++;
			}
		}

		switch(in_ct) {
			default:
				//tri behind plane
				return 0;
			case 1:
			{
				//form tri
				a.p[0]=*in_pts[0];
				a.t[0]=*in_tex[0];

				float t=0;
				a.p[1]=segIntersectPlane(*in_pts[0], *out_pts[0], ctr, norm, &t);
				a.t[1].u=in_tex[0]->u+t*(out_tex[0]->u-in_tex[0]->u);
				a.t[1].v=in_tex[0]->v+t*(out_tex[0]->v-in_tex[0]->v);
				a.t[1].w=in_tex[0]->w+t*(out_tex[0]->w-in_tex[0]->w);

				t=0;
				a.p[2]=segIntersectPlane(*in_pts[0], *out_pts[1], ctr, norm, &t);
				a.t[2].u=in_tex[0]->u+t*(out_tex[1]->u-in_tex[0]->u);
				a.t[2].v=in_tex[0]->v+t*(out_tex[1]->v-in_tex[0]->v);
				a.t[2].w=in_tex[0]->w+t*(out_tex[1]->w-in_tex[0]->w);
				
				a.col=tri.col;
				return 1;
			}
			case 2:
			{
				//form quad
				a.p[0]=*in_pts[0];
				a.t[0]=*in_tex[0];

				a.p[1]=*in_pts[1];
				a.t[1]=*in_tex[1];

				float t=0;
				a.p[2]=segIntersectPlane(*in_pts[1], *out_pts[0], ctr, norm, &t);
				a.t[2].u=in_tex[1]->u+t*(out_tex[0]->u-in_tex[1]->u);
				a.t[2].v=in_tex[1]->v+t*(out_tex[0]->v-in_tex[1]->v);
				a.t[2].w=in_tex[1]->w+t*(out_tex[0]->w-in_tex[1]->w);

				a.col=tri.col;

				b.p[0]=a.p[0];
				b.t[0]=a.t[0];
				
				b.p[1]=a.p[2];
				b.t[1]=a.t[2];

				t=0;
				b.p[2]=segIntersectPlane(*out_pts[0], a.p[0], ctr, norm, &t);
				b.t[2].u=out_tex[0]->u+t*(a.t[0].u-out_tex[0]->u);
				b.t[2].v=out_tex[0]->v+t*(a.t[0].v-out_tex[0]->v);
				b.t[2].w=out_tex[0]->w+t*(a.t[0].w-out_tex[0]->w);

				b.col=tri.col;
				return 2;
			}
			case 3:
				//tri infront of plane
				a=tri;
				return 1;
		}
	}

	void DrawTriangleDecal(const vf2d& a, const vf2d& b, const vf2d& c, olc::Pixel col=olc::WHITE) {
		DrawLineDecal(a, b, col);
		DrawLineDecal(b, c, col);
		DrawLineDecal(c, a, col);
	}

	void TexturedTriangle(
	int x1, int y1, float u1, float v1, float w1,
	int x2, int y2, float u2, float v2, float w2,
	int x3, int y3, float u3, float v3, float w3,
	olc::Sprite* s
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
			for(int i=y1; i<=y2; i++) {
				int ax=x1+dax_step*(i-y1);
				int bx=x1+dbx_step*(i-y1);
				float tex_su=u1+du1_step*(i-y1);
				float tex_sv=v1+dv1_step*(i-y1);
				float tex_sw=w1+dw1_step*(i-y1);
				float tex_eu=u1+du2_step*(i-y1);
				float tex_ev=v1+dv2_step*(i-y1);
				float tex_ew=w1+dw2_step*(i-y1);
				//sort along x
				if(ax>bx) {
					std::swap(ax, bx);
					std::swap(tex_su, tex_eu);
					std::swap(tex_sv, tex_ev);
					std::swap(tex_sw, tex_ew);
				}
				float t_step=1.f/(bx-ax);
				float t=0;
				for(int j=ax; j<bx; j++) {
					tex_u=tex_su+t*(tex_eu-tex_su);
					tex_v=tex_sv+t*(tex_ev-tex_sv);
					tex_w=tex_sw+t*(tex_ew-tex_sw);
					Draw(j, i, s->Sample(tex_u/tex_w, tex_v/tex_w));
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

		for(int i=y2; i<=y3; i++) {
			int ax=x2+dax_step*(i-y2);
			int bx=x1+dbx_step*(i-y1);
			float tex_su=u2+du1_step*(i-y2);
			float tex_sv=v2+dv1_step*(i-y2);
			float tex_sw=w2+dw1_step*(i-y2);
			float tex_eu=u1+du2_step*(i-y1);
			float tex_ev=v1+dv2_step*(i-y1);
			float tex_ew=w1+dw2_step*(i-y1);
			//sort along x
			if(ax>bx) {
				std::swap(ax, bx);
				std::swap(tex_su, tex_eu);
				std::swap(tex_sv, tex_ev);
				std::swap(tex_sw, tex_ew);
			}
			float t_step=1.f/(bx-ax);
			float t=0;
			for(int j=ax; j<bx; j++) {
				tex_u=tex_su+t*(tex_eu-tex_su);
				tex_v=tex_sv+t*(tex_ev-tex_sv);
				tex_w=tex_sw+t*(tex_ew-tex_sw);
				Draw(j, i, s->Sample(tex_u/tex_w, tex_v/tex_w));
				t+=t_step;
			}
		}
	}

	bool OnUserUpdate(float dt) override {
		if(GetKey(olc::Key::CTRL).bHeld) dt/=10;

		const bool to_time=GetKey(olc::Key::T).bPressed;
		Stopwatch geom_watch, render_watch;

#pragma region MOVEMENT
		//add mouse later

		//look up,down
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

		if(GetKey(olc::Key::ENTER).bHeld) light_pos=cam_pos;

		if(GetKey(olc::Key::O).bPressed) show_outlines^=true;
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

				Triangle tri_view;
				for(int i=0; i<3; i++) {
					tri_view.p[i]=tri.p[i]*mat_view;
					tri_view.t[i]=tri.t[i];
				}
				tri_view.col=tri.col*dp;

				//clip against near plane
				Triangle clipped[2];
				int num=clipAgainstPlane(tri_view, vf3d(0, 0, .1f), vf3d(0, 0, 1), clipped[0], clipped[1]);
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

		//painters algorithm?
		tris_to_clip.sort([](const Triangle& a, const Triangle& b) {
			float a_z=a.p[0].z+a.p[1].z+a.p[2].z;
			float b_z=b.p[0].z+b.p[1].z+b.p[2].z;
			return a_z>b_z;
		});
		if(to_time) {
			geom_watch.stop();
			auto dur=geom_watch.getMicros();
			std::cout<<"geom: "<<dur<<"us ("<<(dur/1000.f)<<" ms)\n";
		}
#pragma endregion

#pragma region RENDER
		if(to_time) render_watch.start();
		Clear(olc::Pixel(50, 50, 50));

		//screen space clipping!
		//its easier?
		std::list<Triangle> tri_queue;
		for(const auto& t:tris_to_clip) {
			Triangle clipped[2];
			tri_queue={t};
			int num_new=1;
			for(int i=0; i<4; i++) {
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

				while(num_new>0) {
					Triangle test=tri_queue.front();
					tri_queue.pop_front();
					num_new--;

					int num_clip=clipAgainstPlane(test, ctr, norm, clipped[0], clipped[1]);
					for(int j=0; j<num_clip; j++) tri_queue.emplace_back(clipped[j]);
				}

				num_new=tri_queue.size();
			}

			//finally!
			for(const auto& t:tri_queue) {
				TexturedTriangle(
				t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
				texture
				);
				if(show_outlines) {
					DrawTriangle(
						vf2d(t.p[0].x, t.p[0].y),
						vf2d(t.p[1].x, t.p[1].y),
						vf2d(t.p[2].x, t.p[2].y),
						olc::WHITE
					);
				}
			}
		}
		if(to_time) {
			render_watch.stop();
			auto dur=render_watch.getMicros();
			std::cout<<"render: "<<dur<<"us ("<<(dur/1000.f)<<" ms)\n";
		}
#pragma endregion

		return true;
	}
};

int main() {
	srand(time(0));

	VoxelGame vg;
	if(vg.Construct(320, 240, 2, 2, false, false)) vg.Start();

	return 0;
}