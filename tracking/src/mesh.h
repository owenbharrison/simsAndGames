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

	Mat4 mat_world;

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
	void updateTransforms() {
		Mat4 mat_rot_x=Mat4::makeRotX(rotation.x);
		Mat4 mat_rot_y=Mat4::makeRotY(rotation.y);
		Mat4 mat_rot_z=Mat4::makeRotZ(rotation.z);
		Mat4 mat_rot=mat_rot_x*mat_rot_y*mat_rot_z;
		Mat4 mat_scale=Mat4::makeScale(scale.x, scale.y, scale.z);
		Mat4 mat_trans=Mat4::makeTrans(translation.x, translation.y, translation.z);
		mat_world=mat_scale*mat_rot*mat_trans;
	}

	//get transforms from new coordinate system
	void updateTransforms(const vf3d& rgt, const vf3d& up, const vf3d& fwd) {
		Mat4 mat_rot;
		mat_rot(0, 0)=rgt.x, mat_rot(0, 1)=rgt.y, mat_rot(0, 2)=rgt.z;
		mat_rot(1, 0)=up.x, mat_rot(1, 1)=up.y, mat_rot(1, 2)=up.z;
		mat_rot(2, 0)=-fwd.x, mat_rot(2, 1)=-fwd.y, mat_rot(2, 2)=-fwd.z;
		mat_rot(3, 3)=1;
		Mat4 mat_scale=Mat4::makeScale(scale.x, scale.y, scale.z);
		Mat4 mat_trans=Mat4::makeTrans(translation.x, translation.y, translation.z);
		mat_world=mat_scale*mat_rot*mat_trans;
	}

	void updateTriangles(const olc::Pixel& col=olc::WHITE) {
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

		m.updateTransforms();
		m.updateTriangles();

		file.close();

		return true;
	}
};
#endif