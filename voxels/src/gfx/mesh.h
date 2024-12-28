#pragma once
#ifndef MESH_STRUCT_H
#define MESH_STRUCT_H

#include <list>
#include <vector>
#include <fstream>
#include <sstream>

#include "triangle.h"

struct Mesh {
	std::list<Triangle> triangles;

	static Mesh loadFromOBJ(const std::string& filename) {
		Mesh m;

		std::ifstream file(filename);
		if(file.fail()) return m;

		std::vector<vf3d> verts;

		std::string line;
		while(std::getline(file, line)) {
			std::stringstream line_str(line);
			std::string type; line_str>>type;
			if(type=="v") {
				vf3d v;
				line_str>>v.x>>v.y>>v.z;
				verts.push_back({v});
			} else if(type=="f") {
				std::vector<int> indexes;

				//parse v/t/n until fail
				int num=0;
				for(std::string vtn; line_str>>vtn; num++) {
					std::stringstream vtn_str(vtn);
					int v_ix; vtn_str>>v_ix;
					indexes.emplace_back(v_ix-1);
				}

				//triangulate
				for(int i=2; i<num; i++) {
					m.triangles.push_back({
						verts[indexes[0]],
						verts[indexes[i-1]],
						verts[indexes[i]],
					});
				}
			}
		}

		file.close();
		return m;
	}

	AABB3 getAABB() const {
		AABB3 a;
		for(const auto& t:triangles) {
			for(int i=0; i<3; i++) {
				a.fitToEnclose(t.p[i]);
			}
		}
		return a;
	}

	void normalize(float rad=1) {
		AABB3 box=getAABB();

		//center abour origin
		vf3d ctr=box.getCenter();
		for(auto& t:triangles) {
			for(int i=0; i<3; i++) t.p[i]-=ctr;
		}

		//divide by maximum dimension
		vf3d size=box.max-box.min;
		float max_dim=std::max(size.x, std::max(size.y, size.z))/2;
		for(auto& t:triangles) {
			for(int i=0; i<3; i++) t.p[i]/=max_dim;
		}

		//scale up to radius
		//divide by maximum dimension
		for(auto& t:triangles) {
			for(int i=0; i<3; i++) t.p[i]*=rad;
		}
	}
};
#endif//MESH_STRUCT_H