#pragma once
#ifndef ARMATURE_STRUCT_H
#define ARMATURE_STRUCT_H

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <exception>

#include "bone.h"

struct Armature {
	std::vector<Bone> bones;

	//https://www.youtube.com/watch?v=ZzMnu3v_MOw
	std::vector<Mat4> calculateAnimationPose(std::vector<Mat4> mat_in) const {
		std::vector<Mat4> mat_local(bones.size());
		for(int i=0; i<bones.size(); i++) {
			mat_local[i]=mat_in[i]*bones[i].mat_local;
		}

		std::vector<Mat4> mat_model(bones.size());
		mat_model[0]=mat_local[0];
		for(int i=1; i<bones.size(); i++) {
			int p=bones[i].parent;
			mat_model[i]=mat_local[i]*mat_model[p];
		}

		std::vector<Mat4> mat_out(bones.size());
		for(int i=0; i<bones.size(); i++) {
			mat_out[i]=bones[i].mat_inv_model*mat_model[i];
		}

		return mat_out;
	}

	static Armature loadFromARM(const std::string& filename) {
		std::ifstream file(filename);
		if(file.fail()) throw std::runtime_error("invalid filename: "+filename);

		Armature arm;

		for(std::string line; std::getline(file, line);) {
			std::stringstream line_str(line);

			Bone b;
			line_str>>b.parent;
			for(int i=0; i<16; i++) line_str>>b.mat_local.v[i%4][i/4];
			for(int i=0; i<16; i++) line_str>>b.mat_inv_model.v[i%4][i/4];
			arm.bones.push_back(b);
		}

		file.close();

		return arm;
	}
};
#endif