#pragma once
#ifndef RENDER_CLASS_H
#define RENDER_CLASS_H
class Render {
	float fov_deg=0;
	float fov_rad=0;

	int width=0, height=0;

	//positioning
	vf3d cam_pos, cam_dir{0, 0, 1};
	vf3d light_pos;

	Mat4 mat_proj;
	Mat4 mat_view;

	void copyFrom(const Render&), clear();

	void updateTransforms();

	void DrawTriangle(const Triangle&);

public:
	//buffers
	unsigned char* pixels=nullptr;
	float* depth=nullptr;

	Render() {}

	Render(int w, int h, float f) {
		fov_deg=f;
		fov_rad=fov_deg/180*Pi;

		//sizing
		width=w;
		height=h;

		//allocate buffers
		pixels=new unsigned char[3*width*height];
		depth=new float[width*height];
		resetBuffers();

		//setup projection matrix
		float asp=float(height)/width;
		mat_proj=Mat4::makeProj(fov_deg, asp, .1f, 1000.f);
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

	float getFOVRad() const {
		return fov_rad;
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

	vf3d getCamPos() const {
		return cam_pos;
	}

	vf3d getCamDir() const {
		return cam_dir;
	}

	void updatePos(vf3d cp, vf3d cd, vf3d lp) {
		cam_pos=cp;
		cam_dir=cd;
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
			depth[i]=0;
		}
	}

	//cull, clip, project, clip, raster
	void render(const std::list<Triangle>& tris) {
		//cull, clip, and project
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

					tris_to_clip.push_back(tri_proj);
				}
			}
		}

		//screen space clipping
		std::list<Triangle> tri_queue;
		std::list<Triangle> tris_to_draw;
		for(const auto& tri:tris_to_clip) {
			Triangle clipped[2];
			tri_queue={tri};
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
			DrawTriangle(t);
		}
	}
};

void Render::copyFrom(const Render& r) {
	fov_deg=r.fov_deg;
	fov_rad=r.fov_rad;

	width=r.width;
	height=r.height;

	pixels=new unsigned char[3*width*height];
	memcpy(pixels, r.pixels, sizeof(unsigned char)*3*width*height);

	depth=new float[width*height];
	memcpy(depth, r.depth, sizeof(float)*width*height);

	cam_pos=r.cam_pos;
	cam_dir=r.cam_dir;
	light_pos=r.light_pos;

	mat_proj=r.mat_proj;
	mat_view=r.mat_view;
}

void Render::clear() {
	delete[] pixels;
	delete[] depth;
}

void Render::updateTransforms() {
	const vf3d up(0, 1, 0);
	vf3d target=cam_pos+cam_dir;
	Mat4 mat_cam=Mat4::makePointAt(cam_pos, target, up);
	mat_view=Mat4::quickInverse(mat_cam);
}

void Render::DrawTriangle(const Triangle& tri) {
	int x1=tri.p[0].x, y1=tri.p[0].y;
	int x2=tri.p[1].x, y2=tri.p[1].y;
	int x3=tri.p[2].x, y3=tri.p[2].y;
	float w1=tri.t[0].w;
	float w2=tri.t[1].w;
	float w3=tri.t[2].w;

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
				if(tex_w>depth[k]) {
					pixels[3*k]=tri.col.r;
					pixels[1+3*k]=tri.col.g;
					pixels[2+3*k]=tri.col.b;
					depth[k]=tex_w;
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
			if(tex_w>depth[k]) {
				pixels[3*k]=tri.col.r;
				pixels[1+3*k]=tri.col.g;
				pixels[2+3*k]=tri.col.b;
				depth[k]=tex_w;
			}
			t+=t_step;
		}
	}
}
#endif