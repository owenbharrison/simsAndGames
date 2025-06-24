#pragma once
#ifndef SPRITE_SHEET_CLASS_H
#define SPRITE_SHEET_CLASS_H

#include <unordered_map>
#include <string>
#include <vector>

struct SpriteSheet {
	olc::Sprite* sprite=nullptr;
	int num_x=0, num_y=0;
	std::unordered_map<std::string, std::vector<int>> animations;

	SpriteSheet() {}

	SpriteSheet(const std::string& filename, int x, int y) {
		sprite=new olc::Sprite(filename);
		num_x=x;
		num_y=y;
	}

	~SpriteSheet() {
		delete sprite;
	}

	void getCurrRect(const std::string& state, float anim, olc::vf2d& a, olc::vf2d& b) {
		//get corresponding list of poses
		const auto& poses=animations[state];
		//wrap animation step and get pose
		int pose=poses[int(anim)%poses.size()];
		//find texture region x
		int pose_x=pose%num_x;
		float min_u=pose_x/float(num_x);
		float max_u=min_u+1.f/num_x;
		//find texture region y
		int pose_y=pose/num_x;
		float min_v=pose_y/float(num_y);
		float max_v=min_v+1.f/num_y;
		//send texture rect
		a={min_u, min_v};
		b={max_u, max_v};
	}
};
#endif