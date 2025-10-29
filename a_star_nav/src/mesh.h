#pragma once
#ifndef MESH_STRUCT_H
#define MESH_STRUCT_H

#include <vector>
#include <fstream>
#include <sstream>

struct IndexTriangle {
	int a=0, b=0, c=0;
};

struct Mesh {
	std::vector<vf3d> vertexes;
	std::vector<IndexTriangle> index_tris;
	vf3d rotation, scale{1, 1, 1}, translation;
	Mat4 mat_world;
	std::vector<cmn::Triangle> tris;

	void updateTransforms() {
		//combine all transforms
		Mat4 mat_rot_x=Mat4::makeRotX(rotation.x);
		Mat4 mat_rot_y=Mat4::makeRotY(rotation.y);
		Mat4 mat_rot_z=Mat4::makeRotZ(rotation.z);
		Mat4 mat_rot=mat_rot_x*mat_rot_y*mat_rot_z;
		Mat4 mat_scale=Mat4::makeScale(scale.x, scale.y, scale.z);
		Mat4 mat_trans=Mat4::makeTrans(translation.x, translation.y, translation.z);
		mat_world=mat_scale*mat_rot*mat_trans;
	}

	void updateTriangles(const olc::Pixel& col) {
		std::vector<vf3d> transformed;
		for(const auto& v:vertexes) {
			transformed.push_back(v*mat_world);
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
			box.fitToEnclose(v*mat_world);
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

		m.updateTransforms();
		m.updateTriangles(olc::WHITE);

		file.close();

		return true;
	}
};
#endif//MESH_STRUCT_H