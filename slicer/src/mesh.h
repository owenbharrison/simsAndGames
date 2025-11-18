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

	vf3d rotation;
	vf3d scale{1, 1, 1};
	vf3d offset;
	Mat4 mat_model, mat_inv_model;

	std::vector<cmn::Triangle> tris;

	void updateMatrixes() {
		//combine all transforms
		Mat4 mat_rot_x=Mat4::makeRotX(rotation.x);
		Mat4 mat_rot_y=Mat4::makeRotY(rotation.y);
		Mat4 mat_rot_z=Mat4::makeRotZ(rotation.z);
		Mat4 mat_rot=mat_rot_x*mat_rot_y*mat_rot_z;
		Mat4 mat_scale=Mat4::makeScale(scale.x, scale.y, scale.z);
		Mat4 mat_trans=Mat4::makeTrans(offset.x, offset.y, offset.z);
		mat_model=mat_scale*mat_rot*mat_trans;
		mat_inv_model=Mat4::inverse(mat_model);
	}

	void updateTriangles(const olc::Pixel& col=olc::WHITE) {
		//transform verts
		std::vector<vf3d> new_verts;
		new_verts.reserve(verts.size());
		for(const auto& v:verts) {
			new_verts.push_back(v*mat_model);
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
		if(!getLocalAABB().contains(pos*mat_inv_model)) return false;

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