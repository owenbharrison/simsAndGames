//implement BVH to optimize ray intersections

#pragma once
#ifndef MESH_STRUCT_H
#define MESH_STRUCT_H

#include <vector>
#include <fstream>
#include <sstream>

#include <exception>

struct Mesh {
	std::vector<cmn::Triangle> triangles;

	cmn::AABB3 getAABB() const {
		cmn::AABB3 a;
		for(const auto& t:triangles) {
			for(int i=0; i<3; i++) {
				a.fitToEnclose(t.p[i]);
			}
		}
		return a;
	}

	void fitToBounds(const cmn::AABB3& box) {
		vf3d box_ctr=box.getCenter();

		cmn::AABB3 me=getAABB();
		vf3d me_ctr=me.getCenter();

		//which is the constraining dimension?
		vf3d num=(box.max-box.min)/(me.max-me.min);
		float scl=std::min(num.x, std::min(num.y, num.z));

		//scale about box center.
		for(auto& t:triangles) {
			for(int i=0; i<3; i++) {
				t.p[i]=box_ctr+scl*(t.p[i]-me_ctr);
			}
		}
	}

	//set triangle colors to their normals
	void colorNormals() {
		for(auto& t:triangles) {
			vf3d norm=t.getNorm(), rgb=.5f+.5f*norm;
			float l=std::max(rgb.x, std::max(rgb.y, rgb.z));
			t.col=olc::PixelF(rgb.x/l, rgb.y/l, rgb.z/l);
		}
	}

	static Mesh loadFromOBJ(const std::string& filename) {
		Mesh m;

		std::ifstream file(filename);
		if(file.fail()) throw std::runtime_error("invalid filename");

		std::vector<vf3d> verts;
		std::vector<cmn::v2d> texs;

		std::string line;
		while(std::getline(file, line)) {
			std::stringstream line_str(line);
			std::string type; line_str>>type;
			if(type=="v") {
				vf3d v;
				line_str>>v.x>>v.y>>v.z;
				verts.push_back({v});
			} else if(type=="vt") {
				cmn::v2d t;
				line_str>>t.u>>t.v;
				texs.push_back(t);
			} else if(type=="f") {
				std::vector<int> v_ixs;
				std::vector<int> vt_ixs;

				//parse v/t/n until fail
				int num=0;
				for(std::string vtn; line_str>>vtn; num++) {
					std::stringstream vtn_str(vtn);
					int v_ix;
					if(vtn_str>>v_ix) v_ixs.push_back(v_ix-1);
					char junk; vtn_str>>junk;
					int vt_ix;
					if(vtn_str>>vt_ix) vt_ixs.push_back(vt_ix-1);
				}

				//triangulate
				bool to_texture=vt_ixs.size();
				for(int i=2; i<num; i++) {
					cmn::Triangle t{
						verts[v_ixs[0]],
						verts[v_ixs[i-1]],
						verts[v_ixs[i]]
					};
					//whether to add texture info...
					if(to_texture) {
						t.t[0]=texs[vt_ixs[0]];
						t.t[1]=texs[vt_ixs[i-1]];
						t.t[2]=texs[vt_ixs[i]];
					}
					m.triangles.push_back(t);
				}
			}
		}

		file.close();
		return m;
	}
};
#endif//MESH_STRUCT_H