#pragma once
#ifndef CAMERA_CLASS_H
#define CAMERA_CLASS_H

//for memcpy
#include <string>

struct Camera {
protected:
	int width=0, height=0;

	void copyFrom(const Camera&), clear();

public:
	olc::Sprite* curr_spr=nullptr, * prev_spr=nullptr;

	vf3d pos, pseudo_up;
	vf3d rgt, up, fwd;

	float focal_length_x=1;
	float focal_length_y=1;

	bool guess_valid=false;
	float guess_x=0, guess_y=0;

	Camera() {}

	Camera(
		int w, int h,
		const vf3d& p, const vf3d& u, const vf3d& d
	) {
		width=w, height=h;

		curr_spr=new olc::Sprite(width, height);
		prev_spr=new olc::Sprite(width, height);

		pos=p;
		pseudo_up=u;

		fwd=d;

		updateBasisVectors();
	}

	Camera(const Camera& c) {
		copyFrom(c);
	}

	~Camera() {
		clear();
	}

	Camera& operator=(const Camera& c) {
		if(&c!=this) {
			clear();

			copyFrom(c);
		}

		return *this;
	}

	int getWidth() const { return width; }
	int getHeight() const { return height; }

	//cast ray thru pixel
	vf3d getDir(float x, float y) const {
		//get direction
		float cx=.5f*width;
		float cy=.5f*height;
		vf3d n=vf3d(
			(x-cx)/focal_length_x,
			(cy-y)/focal_length_y,//flip y
			1
		).norm();

		//apply basis vectors
		return n.x*rgt+n.y*up+n.z*fwd;
	}

	void updateSprite(olc::Sprite* spr) {
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				float u=(.5f+i)/width;
				float v=(.5f+j)/height;
				curr_spr->SetPixel(i, j, spr->Sample(u, v));
			}
		}
	}

	//update coordinate system (from RHR)
	virtual void updateBasisVectors() {
		rgt=fwd.cross(pseudo_up).norm();
		up=rgt.cross(fwd);
	}

	void updateGuess() {
		const vf3d rgb_weights(.299f, .587f, .114f);

		guess_valid=false;

		//luminance weighted average
		float x_sum=0, y_sum=0;
		float w_sum=0;
		for(int x=0; x<width; x++) {
			for(int y=0; y<height; y++) {
				olc::Pixel curr=curr_spr->GetPixel(x, y);
				float curr_lum=rgb_weights.dot(vf3d(
					curr.r, curr.g, curr.b
				))/255;

				olc::Pixel prev=prev_spr->GetPixel(x, y);
				float prev_lum=rgb_weights.dot(vf3d(
					prev.r, prev.g, prev.b
				))/255;

				float lum_diff=std::abs(curr_lum-prev_lum);
				x_sum+=lum_diff*x;
				y_sum+=lum_diff*y;
				w_sum+=lum_diff;
			}
		}
		if(w_sum<1e-6) return;

		//find position of movement
		guess_valid=true;
		guess_x=x_sum/w_sum;
		guess_y=y_sum/w_sum;
	}
};

void Camera::copyFrom(const Camera& c) {
	width=c.width;
	height=c.height;

	curr_spr=new olc::Sprite(width, height);
	std::memcpy(curr_spr->GetData(), c.curr_spr->GetData(), sizeof(olc::Pixel)*width*height);

	prev_spr=new olc::Sprite(width, height);
	std::memcpy(prev_spr->GetData(), c.prev_spr->GetData(), sizeof(olc::Pixel)*width*height);

	pos=c.pos;
	pseudo_up=c.pseudo_up;

	rgt=c.rgt;
	up=c.up;
	fwd=c.fwd;

	focal_length_x=c.focal_length_x;
	focal_length_y=c.focal_length_y;

	guess_valid=c.guess_valid;
	guess_x=c.guess_x;
	guess_y=c.guess_y;
}

void Camera::clear() {
	delete curr_spr;
	curr_spr=nullptr;
	delete prev_spr;
	prev_spr=nullptr;
}
#endif