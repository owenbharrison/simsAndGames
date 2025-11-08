#pragma once
#ifndef FOLLOWING_CAMERA_CLASS_H
#define FOLLOWING_CAMERA_CLASS_H

//for trig
#include <cmath>

//y p => x y z
//0 0 => 0 0 1
static vf3d polar3D(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

#include "camera.h"

struct FollowingCamera : public Camera {
protected:
	void copyFrom(const FollowingCamera&), clear();

public:
	//how is this thing mounted?
	vf3d mount_rgt, mount_up, mount_fwd;

	//these are w.r.t. mount_fwd
	float yaw=0, pitch=0;

	float min_yaw=0, max_yaw=0;
	float min_pitch=0, max_pitch=0;

	FollowingCamera() {}

	FollowingCamera(
		int w, int h,
		const vf3d& p, const vf3d& u, const vf3d& d,
		float ny, float xy, float np, float xp
	) : Camera(w, h, p, u, d) {
		mount_fwd=d;

		min_yaw=ny, max_yaw=xy;
		min_pitch=np, max_pitch=xp;

		updateBasisVectors();
	}

	//ro3: 1
	FollowingCamera(const FollowingCamera& fc) {
		copyFrom(fc);
	}

	//ro3: 2
	~FollowingCamera() {
		clear();
	}

	//ro3: 3
	FollowingCamera& operator=(const FollowingCamera& fc) {
		if(&fc!=this) {
			clear();

			copyFrom(fc);
		}

		return *this;
	}

	//clamp turning to bounds
	void clampAngles() {
		if(yaw<min_yaw) yaw=min_yaw;
		if(yaw>max_yaw) yaw=max_yaw;
		if(pitch<min_pitch) pitch=min_pitch;
		if(pitch>max_pitch) pitch=max_pitch;
	}

	void updateBasisVectors() override {
		//update coordinate system (from RHR)
		mount_rgt=mount_fwd.cross(pseudo_up).norm();
		mount_up=mount_rgt.cross(mount_fwd);

		//get dir given yaw & pitch
		vf3d yp=polar3D(yaw, pitch);

		//transform that dir with coordinate system
		fwd=yp.x*mount_rgt+yp.y*mount_up+yp.z*mount_fwd;

		//update base coordinate system
		Camera::updateBasisVectors();
	}
};

void FollowingCamera::copyFrom(const FollowingCamera& fc) {
	Camera::copyFrom(fc);

	mount_rgt=fc.mount_rgt;
	mount_up=fc.mount_up;
	mount_fwd=fc.mount_fwd;

	yaw=fc.yaw, pitch=fc.pitch;

	min_yaw=fc.min_yaw, max_yaw=fc.max_yaw;
	min_pitch=fc.min_pitch, max_pitch=fc.max_pitch;
}

void FollowingCamera::clear() {
	Camera::clear();
}
#endif