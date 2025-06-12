#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include "quat.h"

struct IndexTriangle {
	int a=0, b=0, c=0;
};

struct Mesh {
	std::vector<vf3d> vertices;
	std::vector<IndexTriangle> index_tris;
	Quat rotation;
	vf3d scale{1, 1, 1};
	vf3d translation;
	Mat4 mat_world;//local->world
	Mat4 mat_local;//world->local
	std::vector<cmn::Triangle> tris;
	int id=-1;

	void updateTransforms() {
		//combine all transforms
		Mat4 mat_rot=Quat::toMat4(rotation);
		Mat4 mat_scale=Mat4::makeScale(scale.x, scale.y, scale.z);
		Mat4 mat_trans=Mat4::makeTrans(translation.x, translation.y, translation.z);
		mat_world=mat_scale*mat_rot*mat_trans;
		//matrix could be singular
		mat_local=Mat4::identity();
		try {
			Mat4 local=Mat4::inverse(mat_world);
			mat_local=local;
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
		}
	}

	void applyTransforms() {
		std::vector<vf3d> new_verts;
		new_verts.reserve(vertices.size());
		for(const auto& v:vertices) {
			new_verts.push_back(v*mat_world);
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

	cmn::AABB3 getAABB() const {
		cmn::AABB3 box;
		for(const auto& t:tris) {
			for(int i=0; i<3; i++) {
				box.fitToEnclose(t.p[i]);
			}
		}
		return box;
	}

	float intersectRay(const vf3d& orig, const vf3d& dir) const {
		if(!rayIntersectBox(orig, dir, getAABB())) return -1;

		//sort by closest
		float record=-1;
		for(const auto& t:tris) {
			float dist=segIntersectTri(orig, orig+dir, t);
			if(dist>0) {
				if(record<0||dist<record) {
					record=dist;
				}
			}
		}

		return record;
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