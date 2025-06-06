#pragma once
#ifndef ENGINE_3D_CLASS_H
#define ENGINE_3D_CLASS_H

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#include "v3d.h"

#include "mat4.h"

#include "v2d.h"

#include "triangle.h"
#include "line.h"

#include <stack>

namespace cmn {
	class Engine3D : public olc::PixelGameEngine {
		bool inRangeX(int x) const { return x>=0&&x<ScreenWidth(); }
		bool inRangeY(int y) const { return y>=0&&y<ScreenHeight(); }

		bool OnUserCreate() override;

		bool OnUserDestroy() override;

		//geom helpers
		void makeQuad(vf3d p, float w, float h, Triangle& a, Triangle& b);

		bool OnUserUpdate(float dt) override;

	public:
		//camera positioning
		vf3d cam_pos, cam_dir{0, 0, 1};
		float cam_fov_deg=90;

		//camera matrices
		Mat4 mat_proj, mat_view;

		//lighting info
		vf3d light_pos;

		//graphics pipeline stages
		std::vector<Triangle> tris_to_project;
		std::vector<Line> lines_to_project;
		std::vector<Triangle> tris_to_draw;
		std::vector<Line> lines_to_draw;

		//z buffering
		float* depth_buffer=nullptr;

		//object buffering?
		int* id_buffer=nullptr;

		virtual bool user_create()=0;

		virtual bool user_destroy() { return true; }

		virtual bool user_update(float dt) { return true; }

		virtual bool user_geometry()=0;

		virtual bool user_render()=0;

		void addAABB(const AABB3& box, const olc::Pixel& col) {
			//corner vertexes
			vf3d v0=box.min, v7=box.max;
			vf3d v1(v7.x, v0.y, v0.z);
			vf3d v2(v0.x, v7.y, v0.z);
			vf3d v3(v7.x, v7.y, v0.z);
			vf3d v4(v0.x, v0.y, v7.z);
			vf3d v5(v7.x, v0.y, v7.z);
			vf3d v6(v0.x, v7.y, v7.z);
			//bottom
			Line l1{v0, v1}; l1.col=col;
			lines_to_project.push_back(l1);
			Line l2{v1, v3}; l2.col=col;
			lines_to_project.push_back(l2);
			Line l3{v3, v2}; l3.col=col;
			lines_to_project.push_back(l3);
			Line l4{v2, v0}; l4.col=col;
			lines_to_project.push_back(l4);
			//sides
			Line l5{v0, v4}; l5.col=col;
			lines_to_project.push_back(l5);
			Line l6{v1, v5}; l6.col=col;
			lines_to_project.push_back(l6);
			Line l7{v2, v6}; l7.col=col;
			lines_to_project.push_back(l7);
			Line l8{v3, v7}; l8.col=col;
			lines_to_project.push_back(l8);
			//top
			Line l9{v4, v5}; l9.col=col;
			lines_to_project.push_back(l9);
			Line l10{v5, v7}; l10.col=col;
			lines_to_project.push_back(l10);
			Line l11{v7, v6}; l11.col=col;
			lines_to_project.push_back(l11);
			Line l12{v6, v4}; l12.col=col;
			lines_to_project.push_back(l12);
		}

		void projectAndClip() {
			//recalculate matrices
			{
				const vf3d up(0, 1, 0);
				vf3d target=cam_pos+cam_dir;
				Mat4 mat_cam=Mat4::makePointAt(cam_pos, target, up);
				mat_view=Mat4::quickInverse(mat_cam);
			}

			//clip triangles against near plane
			std::vector<Triangle> tris_to_clip;
			for(const auto& tri:tris_to_project) {
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
					tri_view.id=tri.id;

					//clip
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
						tri_proj.id=clipped[i].id;

						tris_to_clip.push_back(tri_proj);
					}
				}
			}

			//clip lines against near plane
			std::vector<Line> lines_to_clip;
			for(const auto& line:lines_to_project) {
				//transform triangles given camera positioning
				Line line_view;
				for(int i=0; i<2; i++) {
					line_view.p[i]=line.p[i]*mat_view;
					line_view.t[i]=line.t[i];
				}
				line_view.col=line.col;
				line_view.id=line.id;

				//clip
				Line clipped;
				if(line_view.clipAgainstPlane(vf3d(0, 0, .1f), vf3d(0, 0, 1), clipped)) {
					Line line_proj;
					//for each vert
					for(int j=0; j<2; j++) {
						//project
						line_proj.p[j]=clipped.p[j]*mat_proj;

						//copy over texture stuff
						line_proj.t[j]=clipped.t[j];

						//w normalization?
						float w=line_proj.p[j].w;
						line_proj.t[j].u/=w;
						line_proj.t[j].v/=w;
						line_proj.t[j].w=1/w;

						//scale into view
						line_proj.p[j]/=w;

						//x/y inverted so put them back
						line_proj.p[j].x*=-1;
						line_proj.p[j].y*=-1;

						//offset into visible normalized space
						line_proj.p[j]+=vf3d(1, 1, 0);
						line_proj.p[j].x*=ScreenWidth()/2;
						line_proj.p[j].y*=ScreenHeight()/2;
					}
					line_proj.col=clipped.col;
					line_proj.id=clipped.id;

					lines_to_clip.push_back(line_proj);
				}
			}

			//left,right,top,bottom
			const vf3d ctrs[4]{
				vf3d(0, 0, 0),
				vf3d(ScreenWidth(), 0, 0),
				vf3d(0, 0, 0),
				vf3d(0, ScreenHeight(), 0)
			};
			const vf3d norms[4]{
				vf3d(1, 0, 0),
				vf3d(-1, 0, 0),
				vf3d(0, 1, 0),
				vf3d(0, -1, 0)
			};

			//clip tris against edges of screen
			std::stack<Triangle> tri_queue;
			tris_to_draw.clear();
			for(const auto& t:tris_to_clip) {
				tri_queue.push(t);
				int num_new=1;
				Triangle clipped[2];
				for(int i=0; i<4; i++) {
					//iteratively clip all tris
					while(num_new>0) {
						Triangle test=tri_queue.top();
						tri_queue.pop();
						num_new--;

						int num_clip=test.clipAgainstPlane(ctrs[i], norms[i], clipped[0], clipped[1]);
						for(int j=0; j<num_clip; j++) tri_queue.push(clipped[j]);
					}
					num_new=tri_queue.size();
				}

				//add to conglomerate
				while(!tri_queue.empty()) {
					tris_to_draw.push_back(tri_queue.top());
					tri_queue.pop();
				}
			}

			//clip lines against edges of screen
			std::stack<Line> line_queue;
			lines_to_draw.clear();
			for(const auto& l:lines_to_clip) {
				Line clipped;
				line_queue.push(l);
				int num_new=1;
				for(int i=0; i<4; i++) {
					//iteratively clip all lines
					while(num_new>0) {
						Line test=line_queue.top();
						line_queue.pop();
						num_new--;

						if(test.clipAgainstPlane(ctrs[i], norms[i], clipped)) {
							line_queue.push(clipped);
						}
					}
					num_new=line_queue.size();
				}

				//add to conglomerate
				while(!line_queue.empty()) {
					lines_to_draw.push_back(line_queue.top());
					line_queue.pop();
				}
			}
		}

		void resetBuffers() {
			for(int i=0; i<ScreenWidth()*ScreenHeight(); i++) {
				//reset depth buffer
				depth_buffer[i]=0;
				//reset id buffer
				id_buffer[i]=-1;
			}
		}

		void FillDepthTriangle(
			int x1, int y1, float w1,
			int x2, int y2, float w2,
			int x3, int y3, float w3,
			olc::Pixel col, int id
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

			float tex_w;

			float dax_step=0, dbx_step=0,
				dw1_step=0, dw2_step=0;

			if(dy1) dax_step=dx1/std::fabsf(dy1);
			if(dy2) dbx_step=dx2/std::fabsf(dy2);

			if(dy1) dw1_step=dw1/std::fabsf(dy1);
			if(dy2) dw2_step=dw2/std::fabsf(dy2);

			//start scanline filling triangles
			if(dy1) {
				for(int j=y1; j<=y2; j++) {
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
					float t=0;
					for(int i=ax; i<bx; i++) {
						tex_w=tex_sw+t*(tex_ew-tex_sw);
						if(inRangeX(i)&&inRangeY(j)) {
							int k=i+ScreenWidth()*j;
							float& depth=depth_buffer[k];
							if(tex_w>depth) {
								Draw(i, j, col);
								depth=tex_w;
								id_buffer[k]=id;
							}
						}
						t+=t_step;
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
				float t=0;
				for(int i=ax; i<bx; i++) {
					tex_w=tex_sw+t*(tex_ew-tex_sw);
					if(inRangeX(i)&&inRangeY(j)) {
						int k=i+ScreenWidth()*j;
						float& depth=depth_buffer[k];
						if(tex_w>depth) {
							Draw(i, j, col);
							depth=tex_w;
							id_buffer[k]=id;
						}
					}
					t+=t_step;
				}
			}
		}

		void DrawDepthLine(
			int x1, int y1, float w1,
			int x2, int y2, float w2,
			olc::Pixel col, int id
		) {
			auto draw=[&] (int i, int j, float t) {
				if(!inRangeX(i)||!inRangeY(j)) return;

				int k=i+ScreenWidth()*j;
				float& depth=depth_buffer[k];
				float w=w1+t*(w2-w1);
				if(w>depth) {
					Draw(i, j, col);
					depth=w;
					id_buffer[k]=id;
				}
			};
			int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;
			dx=x2-x1; dy=y2-y1;
			dx1=abs(dx), dy1=abs(dy);
			px=2*dy1-dx1, py=2*dx1-dy1;
			if(dy1<=dx1) {
				bool flip=false;
				if(dx>=0) x=x1, y=y1, xe=x2;
				else x=x2, y=y2, xe=x1, flip=true;
				draw(x, y, flip);
				float t_step=1.f/dx1;
				for(i=0; x<xe; i++) {
					x++;
					if(px<0) px+=2*dy1;
					else {
						if((dx<0&&dy<0)||(dx>0&&dy>0)) y++;
						else y--;
						px+=2*(dy1-dx1);
					}
					draw(x, y, flip?1-t_step*i:t_step*i);
				}
			} else {
				bool flip=false;
				if(dy>=0) x=x1, y=y1, ye=y2;
				else x=x2, y=y2, ye=y1, flip=true;
				draw(x, y, flip);
				float t_step=1.f/dy1;
				for(i=0; y<ye; i++) {
					y++;
					if(py<=0) py+=2*dx1;
					else {
						if((dx<0&&dy<0)||(dx>0&&dy>0)) x++;
						else x--;
						py+=2*(dx1-dy1);
					}
					draw(x, y, flip?1-t_step*i:t_step*i);
				}
			}
		}

		void FillTexturedDepthTriangle(
			int x1, int y1, float u1, float v1, float w1,
			int x2, int y2, float u2, float v2, float w2,
			int x3, int y3, float u3, float v3, float w3,
			olc::Sprite* spr, olc::Pixel tint, int id
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
						if(inRangeX(i)&&inRangeY(j)) {
							int k=i+ScreenWidth()*j;
							float& depth=depth_buffer[k];
							if(tex_w>depth) {
								Draw(i, j, tint*spr->Sample(tex_u/tex_w, tex_v/tex_w));
								depth=tex_w;
								id_buffer[k]=id;
							}
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
					if(inRangeX(i)&&inRangeY(j)) {
						int k=i+ScreenWidth()*j;
						float& depth=depth_buffer[k];
						if(tex_w>depth) {
							Draw(i, j, tint*spr->Sample(tex_u/tex_w, tex_v/tex_w));
							depth=tex_w;
							id_buffer[k]=id;
						}
					}
					t+=t_step;
				}
			}
		}
	};

	bool Engine3D::OnUserCreate() {
		//create projection matrix
		float asp=float(ScreenHeight())/ScreenWidth();
		mat_proj=Mat4::makeProj(cam_fov_deg, asp, .001f, 1000.f);

		//z buffering
		depth_buffer=new float[ScreenWidth()*ScreenHeight()];

		//object buffering?
		id_buffer=new int[ScreenWidth()*ScreenHeight()];

		if(!user_create()) return false;

		return true;
	}

	bool Engine3D::OnUserDestroy() {
		delete[] depth_buffer;

		delete[] id_buffer;

		if(!user_destroy()) return false;

		return true;
	}

	void Engine3D::makeQuad(vf3d p, float w, float h, Triangle& a, Triangle& b) {
		//billboarded to point at camera
		vf3d norm=(p-cam_pos).norm();
		vf3d up(0, 1, 0);
		vf3d rgt=norm.cross(up).norm();
		up=rgt.cross(norm);

		//vertex positioning
		vf3d tl=p+w/2*rgt+h/2*up;
		vf3d tr=p+w/2*rgt-h/2*up;
		vf3d bl=p-w/2*rgt+h/2*up;
		vf3d br=p-w/2*rgt-h/2*up;

		//tesselation
		a={tl, br, tr};
		b={tl, bl, br};
	}

	bool Engine3D::OnUserUpdate(float dt) {
		if(!user_update(dt)) return false;

		tris_to_project.clear();
		lines_to_project.clear();
		if(!user_geometry()) return false;

		projectAndClip(); 

		if(!user_render()) return false;

		return true;
	}
}
#endif