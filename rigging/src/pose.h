#pragma once
#ifndef POSE_STRUCT_H
#define POSE_STRUCT_H

#include <vector>
#include <string>
#include <fstream>
#include <sstream>

struct Pose {
	std::vector<Mat4> mat_pose;

	static Pose loadFromPOSE(const std::string& filename) {

		std::ifstream file(filename);
		if(file.fail()) throw std::runtime_error("invalid filename: "+filename);

		Pose pose;

		//load bone list
		for(std::string line; std::getline(file, line);) {
			std::stringstream line_str(line);

			//parse bone matrix
			Mat4 m;
			for(int i=0; i<16; i++) line_str>>m(i%4, i/4);

			pose.mat_pose.push_back(m);
		}

		return pose;
	}
};
#endif