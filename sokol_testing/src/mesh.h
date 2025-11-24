#pragma once
#ifndef MESH_STRUCT_H
#define MESH_STRUCT_H

#include "math/v3d.h"
#include "math/mat4.h"

#include <fstream>
#include <vector>
#include <sstream>
#include <unordered_map>
//for hash
#include <functional>

#include "return_code.h"

struct v2d_t { float u=0, v=0; };

struct Vertex {
	vf3d pos, norm;
	v2d_t tex;
};

struct IndexTriangle {
	int a, b, c;
};

//come on you guys!
//there it is right there in front of you the whole time.
//you're dereferencing a null pointer!
//open your eyes.
float rayIntersectTri(
	const vf3d& orig, const vf3d& dir,
	const vf3d& t0, const vf3d& t1, const vf3d& t2,
	float* uptr=nullptr, float* vptr=nullptr
) {
	/*ray equation
	p=o+td
	triangle equation
	t0+u(t1-t0)+v(t2-t0)
	set equal
	o+td=t0+u(t1-t0)+v(t2-t0)
	rearrange
	td+u(t0-t1)+v(t0-t2)=t0-o
	solve like matrix!
	*/

	static const float epsilon=1e-6f;

	//column vectors
	vf3d a=dir;
	vf3d b=t0-t1;
	vf3d c=t0-t2;
	vf3d d=t0-orig;
	vf3d bxc=b.cross(c);
	float det=a.dot(bxc);
	//parallel
	if(std::abs(det)<epsilon) return -1;

	vf3d f=c.cross(a)/det;
	float u=f.dot(d);
	if(uptr) *uptr=u;

	vf3d g=a.cross(b)/det;
	float v=g.dot(d);
	if(vptr) *vptr=v;

	//within unit uv triangle
	if(u<0||u>1) return -1;
	if(v<0||v>1) return -1;
	if(u+v>1) return -1;

	//get t
	vf3d e=bxc/det;
	float t=e.dot(d);

	//behind ray
	if(t<0) return -1;

	return t;
}

struct Mesh {
	std::vector<Vertex> verts;
	std::vector<IndexTriangle> index_tris;
	sg_buffer vbuf;
	sg_buffer ibuf;

	vf3d rotation, scale{1, 1, 1}, translation;
	mat4 m_model, m_inv_model;

	float intersectRay(const vf3d& orig_world, const vf3d& dir_world) const {
		//localize ray
		float w=1;//want translation
		vf3d orig_local=matMulVec(m_inv_model, orig_world, w);
		w=0;//no translation
		vf3d dir_local=matMulVec(m_inv_model, dir_world, w);
		//renormalization is not necessary here, because
		//  rayIntersect is basically segment intersection,
		//  but it feels wrong not to pass a unit vector.
		dir_local=dir_local.norm();

		//find closest tri
		float record=-1;
		for(const auto& it:index_tris) {
			//valid intersection?
			float dist=rayIntersectTri(
				orig_local, dir_local,
				verts[it.a].pos,
				verts[it.b].pos,
				verts[it.c].pos
			);
			if(dist<0) continue;

			//"sort" while iterating
			if(record<0||dist<record) record=dist;
		}

		//i hate fp== comparisons.
		if(record<0) return -1;

		//get point from local -> world & get dist to orig
		//i need to do this because of the non-uniform scale
		vf3d p_local=orig_local+record*dir_local;
		w=1;//want translation
		vf3d p_world=matMulVec(m_model, p_local, w);
		return (p_world-orig_world).mag();
	}

	void updateVertexBuffer() {
		//free old
		if(vbuf.id!=SG_INVALID_ID) sg_destroy_buffer(vbuf);

		struct BufferVertex {
			float x, y, z;
			float nx, ny, nz;
			std::int16_t u, v;
		};

		//alloc temp
		const int num_verts=verts.size();
		BufferVertex* vbuf_data=new BufferVertex[num_verts];

		//transfer to temp
		for(int i=0; i<num_verts; i++) {
			const auto& v=verts[i];
			vbuf_data[i]={
				v.pos.x, v.pos.y, v.pos.z,
				v.norm.x, v.norm.y, v.norm.z,
				std::int16_t(32767*v.tex.u), std::int16_t(32767*v.tex.v)
			};
		}

		//send temp to "gpu"
		sg_buffer_desc vbuf_desc; zeroMem(vbuf_desc);
		vbuf_desc.data.ptr=vbuf_data;
		vbuf_desc.data.size=sizeof(BufferVertex)*num_verts;
		vbuf=sg_make_buffer(vbuf_desc);

		//free temp
		delete[] vbuf_data;
	}

	void updateIndexBuffer() {
		//free old
		if(ibuf.id!=SG_INVALID_ID) sg_destroy_buffer(ibuf);

		//alloc temp
		const int num_index_tris=index_tris.size();
		const int num_indexes=3*num_index_tris;
		std::uint32_t* ibuf_data=new std::uint32_t[num_indexes];

		//transfer to temp
		for(int i=0; i<num_index_tris; i++) {
			const auto& it=index_tris[i];
			ibuf_data[3*i]=it.a;
			ibuf_data[1+3*i]=it.b;
			ibuf_data[2+3*i]=it.c;
		}

		//send temp to "gpu"
		sg_buffer_desc ibuf_desc; zeroMem(ibuf_desc);
		ibuf_desc.usage.index_buffer=true;
		ibuf_desc.data.ptr=ibuf_data;
		ibuf_desc.data.size=sizeof(std::uint32_t)*num_indexes;
		ibuf=sg_make_buffer(ibuf_desc);

		//free temp
		delete[] ibuf_data;
	}
	
	void updateMatrixes() {
		//xyz euler angles?
		mat4 m_rot_x=mat4::makeRotX(rotation.x);
		mat4 m_rot_y=mat4::makeRotY(rotation.y);
		mat4 m_rot_z=mat4::makeRotZ(rotation.z);
		mat4 m_rot=mat4::mul(m_rot_z, mat4::mul(m_rot_y, m_rot_x));

		mat4 m_scale=mat4::makeScale(scale);

		mat4 m_trans=mat4::makeTranslation(translation);

		//combine & invert
		m_model=mat4::mul(m_trans, mat4::mul(m_rot, m_scale));
		m_inv_model=mat4::inverse(m_model);
	}

	[[nodiscard]] static ReturnCode loadFromOBJ(Mesh& m, const std::string& filename) {
		m=Mesh{};

		std::ifstream file(filename);
		if(file.fail()) return {false, "invalid filename"};

		std::vector<vf3d> vertex_pos;
		std::vector<vf3d> vertex_norm;
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
				vf3d v;
				line_ss>>v.x>>v.y>>v.z;

				vertex_pos.push_back(v);
			} else if(type=="vn") {
				vf3d n;
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

					int v=-1, t=1, n=-1;

					std::string v_str;
					std::getline(vtn_ss, v_str, '/');
					std::stringstream v_ss(v_str);
					if(!(v_ss>>v)) v=-1;

					std::string t_str;
					if(std::getline(vtn_ss, t_str, '/')) {
						std::stringstream t_ss(t_str);
						if(!(t_ss>>t)) t=1;
					}

					std::string n_str;
					if(std::getline(vtn_ss, n_str, '/')) {
						std::stringstream n_ss(n_str);
						if(!(n_ss>>n)) n=-1;
					}

					if(v==-1) return {false, "invalid face vertex index"};
					if(n==-1) return {false, "invalid face normal index"};

					//obj 1-based indexing
					vtns.push_back({v-1, t-1, n-1});
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
					m.index_tris.push_back({
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
		m.updateMatrixes();

		return {true, "success"};
	}

	static void makeCube(Mesh& m) {
		m={};

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

		m.index_tris={
			{0, 1, 2}, {1, 3, 2},//back
			{4, 6, 5}, {5, 6, 7},//front
			{8, 9, 10}, {9, 11, 10},//left
			{12, 14, 13}, {13, 14, 15},//right
			{16, 17, 18}, {17, 19, 18},//bottom
			{20, 22, 21}, {21, 22, 23}//top
		};

		m.updateIndexBuffer();
		m.updateVertexBuffer();
		m.updateMatrixes();
	}
};
#endif