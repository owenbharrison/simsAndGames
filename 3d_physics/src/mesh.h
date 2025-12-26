#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include "index_triangle.h"

#include <fstream>
#include <sstream>
#include <exception>

struct Mesh {
	std::vector<vf3d> vertices;
	std::vector<IndexTriangle> index_tris;
	
	vf3d scale{1, 1, 1}, rotation, translation;
	mat4 model;

	std::vector<cmn::Triangle> tris;

	//combine all transforms
	void updateMatrix() {
		mat4 scl=mat4::makeScale(scale);
		mat4 rot_x=mat4::makeRotX(rotation.x);
		mat4 rot_y=mat4::makeRotY(rotation.y);
		mat4 rot_z=mat4::makeRotZ(rotation.z);
		mat4 rot=mat4::mul(rot_z, mat4::mul(rot_y, rot_x));
		mat4 trans=mat4::makeTranslation(translation);
		model=mat4::mul(trans, mat4::mul(rot, scl));
	}

	void updateTriangles(const olc::Pixel& col) {
		std::vector<vf3d> new_verts;
		new_verts.reserve(vertices.size());
		for(const auto& v:vertices) {
			float w=1;
			new_verts.push_back(matMulVec(model, v, w));
		}

		tris.clear();
		tris.reserve(index_tris.size());
		for(const auto& it:index_tris) {
			cmn::Triangle t{
				new_verts[it.a],
				new_verts[it.b],
				new_verts[it.c]
			}; t.col=col;
			tris.push_back(t);
		}
	}

	static Mesh loadFromOBJ(const std::string& filename) {
		Mesh m;

		std::ifstream file(filename);
		if(file.fail()) throw std::runtime_error("invalid filename");

		//parse file line by line
		std::string line;
		while(std::getline(file, line)) {
			std::stringstream line_str(line);
			std::string type; line_str>>type;
			if(type=="v") {
				vf3d v;
				line_str>>v.x>>v.y>>v.z;
				m.vertices.push_back(v);
			} else if(type=="f") {
				std::vector<int> v_ixs;

				//parse v/t/n until fail
				int num=0;
				for(std::string vtn; line_str>>vtn; num++) {
					std::stringstream vtn_str(vtn);
					int v_ix;
					if(vtn_str>>v_ix) v_ixs.push_back(v_ix-1);
				}

				//triangulate
				for(int i=2; i<num; i++) {
					m.index_tris.push_back({
						v_ixs[0],
						v_ixs[i-1],
						v_ixs[i]
						});
				}
			}
		}

		file.close();

		return m;
	}
};
#endif