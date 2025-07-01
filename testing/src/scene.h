#pragma once
#ifndef SCENE_CLASS_H
#define SCENE_CLASS_H

#include "mesh.h"
#include "quat.h"

#include <vector>

struct Scene {
	std::vector<Mesh> meshes;

	static Scene load(const fs::path& scn_path) {
		std::ifstream file(scn_path);
		if(file.fail()) throw std::runtime_error("invalid filename");

		//get header
		std::string line;
		std::getline(file, line);
		int num=std::stoi(line);
		if(num<0) throw std::runtime_error("invalid meshnum");

		const fs::path base_dir=scn_path.parent_path();

		Scene scn;

		char junk;
		std::stringstream line_str;
		for(int i=0; i<num; i++) {
			//m <filename>
			std::getline(file, line);
			line_str.str(line), line_str.clear();
			line_str>>junk;
			line_str>>line;
			Mesh m=Mesh::loadFromOBJ(base_dir/line);

			//r <w> <x> <y> <z>
			std::getline(file, line);
			line_str.str(line), line_str.clear();
			line_str>>junk;
			line_str>>m.rotation.w>>m.rotation.x>>m.rotation.y>>m.rotation.z;

			//s <x> <y> <z>
			std::getline(file, line);
			line_str.str(line), line_str.clear();
			line_str>>junk;
			line_str>>m.scale.x>>m.scale.y>>m.scale.z;

			//t <x> <y> <z>
			std::getline(file, line);
			line_str.str(line), line_str.clear();
			line_str>>junk;
			line_str>>m.translation.x>>m.translation.y>>m.translation.z;

			//add it
			scn.meshes.push_back(m);
		}

		//update and color meshes
		for(auto& m:scn.meshes) {
			m.updateTransforms();
			m.applyTransforms();
			m.colorNormals();
		}

		return scn;
	}
};
#endif