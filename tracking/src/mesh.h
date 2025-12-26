#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include <vector>

struct IndexTriangle {
	int a=0, b=0, c=0;
};

struct Mesh {
	std::vector<vf3d> vertices;
	std::vector<IndexTriangle> index_tris;

	vf3d rotation;
	vf3d scale{1, 1, 1};
	vf3d translation;
	mat4 model;

	std::vector<cmn::Triangle> tris;

	float intersectRay(const vf3d& orig, const vf3d& dir) const {
		//sort by closest
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

	//get transforms from new coordinate system
	void updateMatrix(const vf3d& rgt, const vf3d& up, const vf3d& fwd) {
		mat4 scl=mat4::makeScale(scale);
		mat4 rot;
		rot(0, 0)=rgt.x, rot(0, 1)=rgt.y, rot(0, 2)=rgt.z;
		rot(1, 0)=up.x, rot(1, 1)=up.y, rot(1, 2)=up.z;
		rot(2, 0)=-fwd.x, rot(2, 1)=-fwd.y, rot(2, 2)=-fwd.z;
		rot(3, 3)=1;
		mat4 trans=mat4::makeTranslation(translation);
		model=mat4::mul(trans, mat4::mul(rot, scl));
	}

	void updateTriangles(const olc::Pixel& col=olc::WHITE) {
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
			};
			t.col=col;
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

	static bool loadFromOBJ(Mesh& m, const std::string& filename) {
		m=Mesh{};
		
		std::ifstream file(filename);
		if(file.fail()) return false;

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

		m.updateMatrix();
		m.updateTriangles();

		file.close();

		return true;
	}
};
#endif