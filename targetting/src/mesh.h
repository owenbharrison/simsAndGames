#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include "quat.h"

bool rayIntersectAABB(const vf3d& orig, const vf3d& dir, const AABB3& box) {
	const float epsilon=1e-6f;
	float tmin=-INFINITY;
	float tmax=INFINITY;

	//x axis
	if(std::abs(dir.x)>epsilon) {
		float inv_d=1/dir.x;
		float t1=inv_d*(box.min.x-orig.x);
		float t2=inv_d*(box.max.x-orig.x);
		if(t1>t2) std::swap(t1, t2);
		tmin=std::max(tmin, t1);
		tmax=std::min(tmax, t2);
	} else if(orig.x<box.min.x||orig.x>box.max.x) return false;

	//y axis
	if(std::abs(dir.y)>epsilon) {
		float inv_d=1/dir.y;
		float t3=inv_d*(box.min.y-orig.y);
		float t4=inv_d*(box.max.y-orig.y);
		if(t3>t4) std::swap(t3, t4);
		tmin=std::max(tmin, t3);
		tmax=std::min(tmax, t4);
	} else if(orig.y<box.min.y||orig.y>box.max.y) return false;

	//z axis
	if(std::abs(dir.z)>epsilon) {
		float inv_d=1/dir.z;
		float t5=inv_d*(box.min.z-orig.z);
		float t6=inv_d*(box.max.z-orig.z);
		if(t5>t6) std::swap(t5, t6);
		tmin=std::max(tmin, t5);
		tmax=std::min(tmax, t6);
	} else if(orig.z<box.min.z||orig.z>box.max.z) return false;

	if(tmax<0||tmin>tmax) return false;

	return true;
}

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
	std::vector<Triangle> tris;
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
			Triangle t{
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
			vf3d norm=t.getNorm();
			t.col.r=128+127*norm.x;
			t.col.g=128+127*norm.y;
			t.col.b=128+127*norm.z;
		}
	}

	AABB3 getAABB() const {
		AABB3 box;
		for(const auto& t:tris) {
			for(int i=0; i<3; i++) {
				box.fitToEnclose(t.p[i]);
			}
		}
		return box;
	}

	float intersectRay(const vf3d& orig, const vf3d& dir) const {
		if(!rayIntersectAABB(orig, dir, getAABB())) return -1;

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