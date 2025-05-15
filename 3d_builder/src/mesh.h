#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include "quat.h"

#include <fstream>
#include <sstream>
#include <filesystem>
namespace fs=std::filesystem;

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
	int uv_a=-1, uv_b=-1, uv_c=-1;
};

struct Mesh {
	fs::path filename;
	std::vector<vf3d> verts;
	std::vector<v2d> tex_verts;
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
		new_verts.reserve(verts.size());
		for(const auto& v:verts) {
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
			if(it.uv_a>=0) t.t[0]=tex_verts[it.uv_a];
			if(it.uv_b>=0) t.t[1]=tex_verts[it.uv_b];
			if(it.uv_c>=0) t.t[2]=tex_verts[it.uv_c];
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

	static Mesh loadFromOBJ(const fs::path& filename) {
		Mesh m;
		m.filename=filename;

		std::ifstream file(m.filename);
		if(file.fail()) throw std::runtime_error("invalid filename");

		//parse file line by line
		std::string line;
		while(std::getline(file, line)) {
			std::stringstream line_str(line);
			std::string type; line_str>>type;
			if(type=="v") {
				vf3d v;
				line_str>>v.x>>v.y>>v.z;
				m.verts.push_back(v);
			} else if(type=="vt") {
				v2d vt;
				line_str>>vt.u>>vt.v;
				m.tex_verts.push_back(vt);
			} else if(type=="f") {
				//this works with n-gons.
				std::vector<int> v_ixs;
				std::vector<int> vt_ixs;

				//parse v/t/n until fail
				bool use_tex=true;
				for(std::string vtn; line_str>>vtn;) {
					std::stringstream vtn_str(vtn);

					int v_ix;
					vtn_str>>v_ix;
					v_ixs.push_back(v_ix-1);

					//load texture info
					char junk;
					if(vtn_str>>junk) {
						int vt_ix;
						vtn_str>>vt_ix;
						vt_ixs.push_back(vt_ix-1);
					} else use_tex=false;
				}

				//triangulate face
				for(int i=2; i<v_ixs.size(); i++) {
					IndexTriangle it{
						v_ixs[0],
						v_ixs[i-1],
						v_ixs[i]
					};
					if(use_tex) {
						it.uv_a=vt_ixs[0];
						it.uv_b=vt_ixs[i-1];
						it.uv_c=vt_ixs[i];
					}
					m.index_tris.push_back(it);
				}
			}
		}

		m.updateTransforms();
		m.applyTransforms();

		return m;
	}

	//save mesh with applied transforms
	bool saveToOBJ(const fs::path& filename) const {
		std::ofstream file(filename);
		if(file.fail()) return false;

		//apply transforms & print
		for(const auto& v:verts) {
			vf3d nv=v*mat_world;
			file<<"v "<<nv.x<<' '<<nv.y<<' '<<nv.z<<'\n';
		}

		//print tex verts
		for(const auto& vt:tex_verts) {
			file<<"vt "<<vt.u<<' '<<vt.v<<'\n';
		}

		//print faces v/t/n
		for(const auto& it:index_tris) {
			file<<"f "<<
				(1+it.a)<<'/'<<(1+it.uv_a)<<' '<<
				(1+it.b)<<'/'<<(1+it.uv_b)<<' '<<
				(1+it.c)<<'/'<<(1+it.uv_c)<<'\n';
		}

		return true;
	}
};
#endif