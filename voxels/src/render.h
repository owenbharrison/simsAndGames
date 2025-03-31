#pragma once
#ifndef RENDER_CLASS_H
#define RENDER_CLASS_H

class Render {
	int width=0, height=0;

	//positioning
	vf3d cam_pos, cam_dir{0, 0, 1};
	vf3d light_pos;

	Mat4 mat_proj;
	Mat4 mat_view;

	void copyFrom(const Render&), clear();

	void updateTransforms();

	void DrawTriangle(int, int, float,
	int, int, float,
	int, int, float,
	unsigned char, unsigned char, unsigned char);

public:
	//buffers
	unsigned char* pixels=nullptr;
	float* depth_buffer=nullptr;

	Render() {}

	Render(int w, int h, float fov) {
		//sizing
		width=w;
		height=h;

		//allocate buffers
		pixels=new unsigned char[3*width*height];
		depth_buffer=new float[width*height];
		resetBuffers();

		//setup projection matrix
		float asp=float(height)/width;
		mat_proj=Mat4::makeProj(fov, asp, .1f, 1000.f);
		updateTransforms();
	}

	//1
	Render(const Render& r) {
		copyFrom(r);
	}

	//2
	~Render() {
		clear();
	}

	//3
	Render& operator=(const Render& r) {
		if(&r==this) return *this;

		clear();

		copyFrom(r);

		return *this;
	}

	int getWidth() const {
		return width;
	}

	int getHeight() const {
		return height;
	}

	int ix(int i, int j) const {
		return i+width*j;
	}

	void updatePos(vf3d cp, vf3d cd, vf3d lp) {
		cam_pos=cp;
		cam_dir=cd.norm();
		light_pos=lp;

		updateTransforms();
	}

	void resetBuffers() {
		//for each pixel
		for(int i=0; i<width*height; i++) {
			//reset pixels
			pixels[3*i]=0;
			pixels[1+3*i]=0;
			pixels[2+3*i]=0;

			//reset depth buffer
			depth_buffer[i]=0;
		}
	}

	//cull, project, clip, transform, clip, raster
	void render(const std::list<Triangle>& tris) {
		//project triangles
		std::list<Triangle> tris_to_clip;
		for(const auto& tri:tris) {
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
						tri_proj.p[j].x*=width/2;
						tri_proj.p[j].y*=height/2;
					}
					tri_proj.col=clipped[i].col;

					tris_to_clip.emplace_back(tri_proj);
				}
			}
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
						ctr=vf3d(width, 0, 0);
						norm=vf3d(-1, 0, 0);
						break;
					case 2://top
						ctr=vf3d(0, 0, 0);
						norm=vf3d(0, 1, 0);
						break;
					case 3://bottom
						ctr=vf3d(0, height, 0);
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
			DrawTriangle(
				t.p[0].x, t.p[0].y, t.t[0].w,
				t.p[1].x, t.p[1].y, t.t[1].w,
				t.p[2].x, t.p[2].y, t.t[2].w,
				t.col.r, t.col.g, t.col.b
			);
		}
	}
};

void Render::copyFrom(const Render& r) {
	width=r.width;
	height=r.height;

	pixels=new unsigned char[3*width*height];
	memcpy(pixels, r.pixels, sizeof(unsigned char)*3*width*height);

	depth_buffer=new float[width*height];
	memcpy(depth_buffer, r.depth_buffer, sizeof(float)*width*height);
}

void Render::clear() {
	delete[] pixels;
	delete[] depth_buffer;
}

void Render::updateTransforms() {
	const vf3d up(0, 1, 0);
	vf3d target=cam_pos+cam_dir;
	Mat4 mat_cam=Mat4::makePointAt(cam_pos, target, up);
	mat_view=Mat4::quickInverse(mat_cam);
}

void Render::DrawTriangle(
int x1, int y1, float w1,
int x2, int y2, float w2,
int x3, int y3, float w3,
unsigned char r, unsigned char g, unsigned char b
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
				int k=ix(i, j);
				if(tex_w>depth_buffer[k]) {
					pixels[3*k]=r;
					pixels[1+3*k]=g;
					pixels[2+3*k]=b;
					depth_buffer[k]=tex_w;
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
			int k=ix(i, j);
			if(tex_w>depth_buffer[k]) {
				pixels[3*k]=r;
				pixels[1+3*k]=g;
				pixels[2+3*k]=b;
				depth_buffer[k]=tex_w;
			}
			t+=t_step;
		}
	}
}
#endif