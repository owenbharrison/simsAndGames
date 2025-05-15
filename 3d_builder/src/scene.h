#pragma once
#ifndef SCENE_CLASS_H
#define SCENE_CLASS_H

#include "mesh.h"
#include "quat.h"

#include <vector>

struct Scene {
	std::vector<Mesh> meshes;

	//dont want to sort meshes by id...
	int lowestUniqueID() const {
		for(int id=0; ; id++) {
			bool unique=true;
			for(const auto& m:meshes) {
				if(m.id==id) {
					unique=false;
					break;
				}
			}
			if(unique) return id;
		}
	}

	bool save(const fs::path& scn_path) const {
		std::ofstream file(scn_path);
		if(file.fail()) return false;

		fs::path base_dir=scn_path.parent_path();

		file<<meshes.size()<<'\n';

		//print all meshes line by line
		for(const auto& m:meshes) {
			auto rel_path=fs::relative(m.filename, base_dir);
			const auto& r=m.rotation;
			const auto& s=m.scale;
			const auto& t=m.translation;
			file<<"m "<<rel_path<<"\n"
				"\tr "<<r.w<<' '<<r.x<<' '<<r.y<<' '<<r.z<<"\n"
				"\ts "<<s.x<<' '<<s.y<<' '<<s.z<<"\n"
				"\tt "<<t.x<<' '<<t.y<<' '<<t.z<<'\n';
		}

		return true;
	}

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
			m.id=scn.lowestUniqueID();
			m.applyTransforms();
			m.colorNormals();
		}

		return scn;
	}
};
#endif