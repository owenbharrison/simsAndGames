#pragma once
#ifndef MESH_STRUCT_H
#define MESH_STRUCT_H

#include <vector>

#include <string>
#include <fstream>
#include <sstream>

struct IndexTriangle {
	int a=0, b=0, c=0;
};

struct Mesh {
	std::vector<vf3d> vertexes;
	std::vector<IndexTriangle> index_tris;

	vf3d scale{1, 1, 1}, rotation, translation;
	cmn::mat4 model;

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
		std::vector<vf3d> transformed;
		for(const auto& v:vertexes) {
			float w=1;
			transformed.push_back(matMulVec(model, v, w));
		}
		tris.clear();
		for(const auto& it:index_tris) {
			cmn::Triangle t{transformed[it.a], transformed[it.b], transformed[it.c]};
			t.col=col;
			tris.push_back(t);
		}
	}

	void colorNormals() {
		for(auto& t:tris) {
			vf3d norm=t.getNorm(), rgb=.5f+.5f*norm;
			float lum=rgb.dot({.299f, .587f, .114f});
			t.col=olc::PixelF(rgb.x/lum, rgb.y/lum, rgb.z/lum);
		}
	}

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

	bool contains(const vf3d& pt) const {
		vf3d dir=vf3d(
			.5f-cmn::randFloat(),
			.5f-cmn::randFloat(),
			.5f-cmn::randFloat()
		).norm();
		int num=0;
		for(const auto& t:tris) {
			float dist=t.intersectSeg(pt, pt+dir);
			if(dist>0) num++;
		}
		return num%2;
	}

	static bool loadFromOBJ(Mesh& m, const std::string& filename) {
		m={};
		
		std::ifstream file(filename);
		if(file.fail()) return false;

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
				for(std::string vtn; line_str>>vtn;) {
					std::stringstream vtn_str(vtn);
					int v_ix;
					if(vtn_str>>v_ix) v_ixs.push_back(v_ix-1);
				}

				//triangulate
				for(int i=2; i<v_ixs.size(); i++) {
					m.index_tris.push_back({
						v_ixs[0],
						v_ixs[i-1],
						v_ixs[i]
						});
				}
			}
		}

		m.updateMatrix();
		m.updateTriangles(olc::WHITE);

		file.close();

		return true;
	}
};
#endif//MESH_STRUCT_H