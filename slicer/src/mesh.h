#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include <vector>

#include "return_code.h"

#include <string>
#include <fstream>
#include <sstream>

struct IndexTriangle {
	int x=0, y=0, z=0;
};

struct Mesh {
	std::vector<vf3d> verts;
	std::vector<IndexTriangle> index_tris;

	vf3d scale{1, 1, 1}, rotation, translation;
	mat4 model, inv_model;

	std::vector<cmn::Triangle> tris;

	//combine all transforms
	void updateMatrixes() {
		mat4 scl=mat4::makeScale(scale);
		mat4 rot_x=mat4::makeRotX(rotation.x);
		mat4 rot_y=mat4::makeRotY(rotation.y);
		mat4 rot_z=mat4::makeRotZ(rotation.z);
		mat4 rot=mat4::mul(rot_z, mat4::mul(rot_y, rot_x));
		mat4 trans=mat4::makeTranslation(translation);
		model=mat4::mul(trans, mat4::mul(rot, scl));
		inv_model=mat4::inverse(model);
	}

	void updateTriangles(const olc::Pixel& col=olc::WHITE) {
		//transform verts
		std::vector<vf3d> new_verts;
		new_verts.reserve(verts.size());
		for(const auto& v:verts) {
			float w=1;
			new_verts.push_back(matMulVec(model, v, w));
		}

		//triangulate
		tris.clear();
		tris.reserve(index_tris.size());
		for(const auto& it:index_tris) {
			cmn::Triangle t{
				new_verts[it.x],
				new_verts[it.y],
				new_verts[it.z]
			}; t.col=col;
			tris.push_back(t);
		}
	}

	cmn::AABB3 getLocalAABB() const {
		cmn::AABB3 box;
		for(const auto& v:verts) {
			box.fitToEnclose(v);
		}
		return box;
	}

	//polyhedra raycasting algorithm
	bool contains(const vf3d& pos) const {
		float w=1;
		if(!getLocalAABB().contains(matMulVec(inv_model, pos, w))) return false;

		//random direction
		vf3d dir(cmn::randFloat(), cmn::randFloat(), cmn::randFloat());
		dir=(.5f-dir).norm();

		//for every tri
		int num=0;
		for(const auto& t:tris) {
			float dist=t.intersectSeg(pos, pos+dir);
			if(dist>0) num++;
		}

		//odd? inside!
		return num%2;
	}

	[[nodiscard]] static ReturnCode loadFromOBJ(Mesh& m, const std::string& filename) {
		m={};

		std::ifstream file(filename);
		if(file.fail()) return {false, "invalid filename"};

		//parse file line by line
		std::string line;
		while(std::getline(file, line)) {
			std::stringstream line_str(line);
			std::string type; line_str>>type;
			if(type=="v") {
				vf3d v;
				line_str>>v.x>>v.y>>v.z;
				m.verts.push_back(v);
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

		m.updateMatrixes();
		m.updateTriangles();

		file.close();

		return {true, "success"};
	}
};
#endif