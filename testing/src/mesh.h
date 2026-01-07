#pragma once
#ifndef MESH_STRUCT_H
#define MESH_STRUCT_H

#include "cmn/math/v3d.h"

#include <vector>

#include <string>

#include <fstream>

//for hash
#include <functional>

#include <unordered_map>

#include <sstream>

struct Mesh {
	struct v2d_t { float u=0, v=0; };

	struct Vertex {
		cmn::vf3d pos, norm;
		v2d_t tex;
	};
	std::vector<Vertex> verts;

	struct IndexTriangle {
		int a, b, c;
	};
	std::vector<IndexTriangle> tris;

	sg_buffer vbuf{SG_INVALID_ID};
	sg_buffer ibuf{SG_INVALID_ID};

	void updateVertexBuffer() {
		//free old
		if(vbuf.id!=SG_INVALID_ID) sg_destroy_buffer(vbuf);

		//send data to "gpu"
		sg_buffer_desc vbuf_desc{};
		vbuf_desc.usage.index_buffer=false;
		vbuf_desc.usage.vertex_buffer=true;
		vbuf_desc.data.ptr=verts.data();
		vbuf_desc.data.size=sizeof(Vertex)*verts.size();
		vbuf=sg_make_buffer(vbuf_desc);
	}

	void updateIndexBuffer() {
		//free old
		if(ibuf.id!=SG_INVALID_ID) sg_destroy_buffer(ibuf);

		//alloc temp
		const int num_tris=tris.size();
		const int num_indexes=3*num_tris;
		std::uint32_t* ibuf_data=new std::uint32_t[num_indexes];

		//transfer to temp
		for(int i=0; i<num_tris; i++) {
			const auto& t=tris[i];
			ibuf_data[3*i]=t.a;
			ibuf_data[1+3*i]=t.b;
			ibuf_data[2+3*i]=t.c;
		}

		//send temp to "gpu"
		sg_buffer_desc ibuf_desc{};
		ibuf_desc.usage.vertex_buffer=false;
		ibuf_desc.usage.index_buffer=true;
		ibuf_desc.data.ptr=ibuf_data;
		ibuf_desc.data.size=sizeof(std::uint32_t)*num_indexes;
		ibuf=sg_make_buffer(ibuf_desc);

		//free temp
		delete[] ibuf_data;
	}

	static Mesh makeCube() {
		Mesh m;

		m.verts={
			//back
			{{-1, -1, -1}, {0, 0, -1}, {1, 1}},
			{{1, -1, -1}, {0, 0, -1}, {0, 1}},
			{{-1, 1, -1}, {0, 0, -1}, {1, 0}},
			{{1, 1, -1}, {0, 0, -1}, {0, 0}},
			//front
			{{-1, -1, 1}, {0, 0, 1}, {0, 1}},
			{{1, -1, 1}, {0, 0, 1}, {1, 1}},
			{{-1, 1, 1}, {0, 0, 1}, {0, 0}},
			{{1, 1, 1}, {0, 0, 1}, {1, 0}},
			//left
			{{-1, -1, -1}, {-1, 0, 0}, {0, 1}},
			{{-1, 1, -1}, {-1, 0, 0}, {0, 0}},
			{{-1, -1, 1}, {-1, 0, 0}, {1, 1}},
			{{-1, 1, 1}, {-1, 0, 0}, {1, 0}},
			//right
			{{1, -1, -1}, {1, 0, 0}, {1, 1}},
			{{1, 1, -1}, {1, 0, 0}, {1, 0}},
			{{1, -1, 1}, {1, 0, 0}, {0, 1}},
			{{1, 1, 1}, {1, 0, 0}, {0, 0}},
			//bottom
			{{-1, -1, -1}, {0, -1, 0}, {0, 1}},
			{{-1, -1, 1}, {0, -1, 0}, {0, 0}},
			{{1, -1, -1}, {0, -1, 0}, {1, 1}},
			{{1, -1, 1}, {0, -1, 0}, {1, 0}},
			//top
			{{-1, 1, -1}, {0, 1, 0}, {0, 0}},
			{{-1, 1, 1}, {0, 1, 0}, {0, 1}},
			{{1, 1, -1}, {0, 1, 0}, {1, 0}},
			{{1, 1, 1}, {0, 1, 0}, {1, 1}}
		};

		m.tris={
			{0, 2, 1}, {1, 2, 3},//back
			{4, 5, 6}, {5, 7, 6},//front
			{8, 10, 9}, {9, 10, 11},//left
			{12, 13, 14}, {13, 15, 14},//right
			{16, 18, 17}, {17, 18, 19},//bottom
			{20, 21, 22}, {21, 23, 22}//top
		};

		m.updateVertexBuffer();
		m.updateIndexBuffer();

		return m;
	}

	[[nodiscard]] static bool loadFromOBJ(Mesh& m, const std::string& filename) {
		m=Mesh{};

		std::ifstream file(filename);
		if(file.fail()) return false;

		std::vector<cmn::vf3d> vertex_pos;
		std::vector<cmn::vf3d> vertex_norm;
		std::vector<v2d_t> vertex_tex{{0, 0}};

		struct vtn_t {
			int v=0, t=0, n=0;

			bool operator==(const vtn_t& x) const {
				return x.v==v&&x.t==t&&x.n==n;
			}
		};

		struct vtn_t_hash {
			std::size_t operator()(const vtn_t& x) const {
				std::size_t v=std::hash<int>()(x.v);
				std::size_t t=std::hash<int>()(x.t);
				std::size_t n=std::hash<int>()(x.n);
				return v^(t<<1)^(n<<2);
			}
		};

		std::unordered_map<vtn_t, int, vtn_t_hash> vtn2ix;

		for(std::string line_str; std::getline(file, line_str);) {
			std::stringstream line_ss(line_str);

			std::string type; line_ss>>type;
			if(type=="v") {
				cmn::vf3d v;
				line_ss>>v.x>>v.y>>v.z;

				vertex_pos.push_back(v);
			} else if(type=="vn") {
				cmn::vf3d n;
				line_ss>>n.x>>n.y>>n.z;

				//ensure unit
				vertex_norm.push_back(n.norm());
			} else if(type=="vt") {
				v2d_t t;
				line_ss>>t.u>>t.v;

				vertex_tex.push_back(t);
			} else if(type=="f") {
				//parse vtns until fail
				std::vector<vtn_t> vtns;
				for(std::string vtn_str; line_ss>>vtn_str;) {
					std::stringstream vtn_ss(vtn_str);

					int v=-1, t=0, n=-1;

					std::string v_str;
					std::getline(vtn_ss, v_str, '/');
					std::stringstream v_ss(v_str);
					if(!(v_ss>>v)) v=-1;

					std::string t_str;
					if(std::getline(vtn_ss, t_str, '/')) {
						std::stringstream t_ss(t_str);
						if(!(t_ss>>t)) t=0;
					}

					std::string n_str;
					if(std::getline(vtn_ss, n_str, '/')) {
						std::stringstream n_ss(n_str);
						if(!(n_ss>>n)) n=-1;
					}

					if(v==-1) return false;
					if(n==-1) return false;

					//obj 1-based indexing
					vtns.push_back({v-1, t, n-1});
				}

				//add vertexes
				std::vector<int> indexes;
				for(const auto& vtn:vtns) {
					//use vertex if it exists
					int ix=0;
					auto it=vtn2ix.find(vtn);
					if(it!=vtn2ix.end()) ix=it->second;
					//otherwise, make new one.
					else {
						ix=m.verts.size();
						vtn2ix.insert({vtn, ix});
						m.verts.push_back({
							vertex_pos[vtn.v],
							vertex_norm[vtn.n],
							vertex_tex[vtn.t]
							});
					}
					indexes.push_back(ix);
				}

				//triangulate polygon
				for(int i=2; i<indexes.size(); i++) {
					m.tris.push_back({
						indexes[0],
						indexes[i-1],
						indexes[i]
						});
				}
			}
		}

		file.close();

		m.updateVertexBuffer();
		m.updateIndexBuffer();

		return true;
	}
};
#endif