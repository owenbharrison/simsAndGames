#pragma once
#ifndef CAMERA_STRUCT_H
#define CAMERA_STRUCT_H
struct Camera {
	int width=0, height=0;
	olc::Sprite* curr_spr=nullptr, * prev_spr=nullptr;
	vf3d pos, dir;
	vf2d focal_length{1, 1};

	Camera() {}

	Camera(int w, int h, const vf3d& p, const vf3d& d) {
		width=w;
		height=h;
		curr_spr=new olc::Sprite(w, h);
		prev_spr=new olc::Sprite(w, h);
		pos=p;
		dir=d;
	}

	void updateSpr(olc::Sprite* spr) {
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				float u=float(i)/width;
				float v=float(j)/height;
				curr_spr->SetPixel(i, j, spr->Sample(u, v));
			}
		}
	}

	//cast ray thru pixel
	vf3d getDir(int x, int y) const {
		//get direction
		float cx=.5f*width;
		float cy=.5f*height;
		vf3d n=vf3d(
			(x-cx)/focal_length.x,
			(cy-y)/focal_length.y,//flip y?
			1
		).norm();

		//apply basis vectors
		vf3d up(0, 1, 0);
		vf3d rgt=dir.cross(up).norm();
		up=rgt.cross(dir);
		return n.x*rgt+n.y*up+n.z*dir;
	}
};
#endif