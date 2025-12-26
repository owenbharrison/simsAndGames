#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include "quat.h"

#include "return_code.h"

struct IndexTriangle {
	int a=0, b=0, c=0;
};

struct Mesh {
	std::vector<vf3d> vertexes;
	std::vector<IndexTriangle> index_tris;

	Quat rotation;
	vf3d scale{1, 1, 1};
	vf3d translation;
	mat4 model;

	std::vector<cmn::Triangle> tris;
	int id=-1;

	//combine all transforms
	void updateTransforms() {
		mat4 scl=mat4::makeScale(scale);
		mat4 rot=rotation.getMatrix();
		mat4 trans=mat4::makeTranslation(translation);
		model=mat4::mul(trans, mat4::mul(rot, scl));
	}

	void applyTransforms() {
		std::vector<vf3d> new_verts;
		new_verts.reserve(vertexes.size());
		for(const auto& v:vertexes) {
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
			};
			t.id=id;
			tris.push_back(t);
		}
	}

	void colorNormals() {
		for(auto& t:tris) {
			vf3d norm=t.getNorm(), rgb=.5f+.5f*norm;
			float l=std::max(rgb.x, std::max(rgb.y, rgb.z));
			t.col=olc::PixelF(rgb.x/l, rgb.y/l, rgb.z/l);
		}
	}

	//world space aabb
	cmn::AABB3 getAABB() const {
		cmn::AABB3 box;
		for(const auto& v:vertexes) {
			float w=1;
			box.fitToEnclose(matMulVec(model, v, w));
		}
		return box;
	}

	float intersectRay(const vf3d& orig, const vf3d& dir) const {
		float record=-1;
		for(const auto& t:tris) {
			float dist=t.intersectSeg(orig, orig+dir);
			if(dist>0) {
				if(record<0||dist<record) {
					record=dist;
				}
			}
		}

		return record;
	}

	static ReturnCode loadFromOBJ(Mesh& m, const std::string& filename) {
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
				m.vertexes.push_back(v);
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

		return {true, "success"};
	}
};
#endif