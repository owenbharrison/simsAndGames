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
	std::vector<IndexTriangle> tris;
	sg_buffer vbuf{SG_INVALID_ID};
	sg_buffer ibuf{SG_INVALID_ID};

	vf3d rotation, scale{1, 1, 1}, translation;
	mat4 model, inv_model;

	float intersectRay(const vf3d& orig_world, const vf3d& dir_world) const {
		//localize ray
		float w=1;//want translation
		vf3d orig_local=matMulVec(inv_model, orig_world, w);
		w=0;//no translation
		vf3d dir_local=matMulVec(inv_model, dir_world, w);
		//renormalization is not necessary here, because
		//  rayIntersect is basically segment intersection,
		//  but it feels wrong not to pass a unit vector.
		dir_local=dir_local.norm();

		//find closest tri
		float record=-1;
		for(const auto& t:tris) {
			//valid intersection?
			float dist=rayIntersectTri(
				orig_local, dir_local,
				verts[t.a].pos,
				verts[t.b].pos,
				verts[t.c].pos
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
		vf3d p_world=matMulVec(model, p_local, w);
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
		sg_buffer_desc vbuf_desc{};
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
		ibuf_desc.usage.index_buffer=true;
		ibuf_desc.data.ptr=ibuf_data;
		ibuf_desc.data.size=sizeof(std::uint32_t)*num_indexes;
		ibuf=sg_make_buffer(ibuf_desc);

		//free temp
		delete[] ibuf_data;
	}

	void updateMatrixes() {
		//xyz euler angles?
		mat4 rot_x=mat4::makeRotX(rotation.x);
		mat4 rot_y=mat4::makeRotY(rotation.y);
		mat4 rot_z=mat4::makeRotZ(rotation.z);
		mat4 rot=mat4::mul(rot_z, mat4::mul(rot_y, rot_x));

		mat4 scl=mat4::makeScale(scale);

		mat4 trans=mat4::makeTranslation(translation);

		//combine & invert
		model=mat4::mul(trans, mat4::mul(rot, scl));
		inv_model=mat4::inverse(model);
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
		m.updateMatrixes();

		return m;
	}

#pragma region SOLIDS OF REVOLUTION
	static Mesh makeTorus(float rad_xz, int num_xz, float rad_y, int num_y) {
		Mesh m;

		//note: loop order matters here.
		for(int i=0; i<num_xz; i++) {
			float u=i/(num_xz-1.f);
			float theta=2*Pi*u;

			//offset from big radius
			float dx=std::cos(theta);
			float ox=rad_xz*dx;
			float dz=std::sin(theta);
			float oz=rad_xz*dz;

			for(int j=0; j<num_y; j++) {
				float v=j/(num_y-1.f);
				float phi=2*Pi*v;

				//scale xz by little radius
				float dr=std::sin(phi);
				float nx=dx*dr;
				float ny=std::cos(phi);
				float nz=dz*dr;
				float x=ox+rad_y*nx;
				float z=oz+rad_y*nz;
				float y=rad_y*ny;

				m.verts.push_back({{x, y, z}, {nx, ny, nz}, {u, v}});
			}
		}

		//tessellate grid
		auto ix=[&] (int i, int j) { return j+num_y*i; };
		for(int i=1; i<num_xz; i++) {
			for(int j=1; j<num_y; j++) {
				const auto& a=ix(i-1, j-1);
				const auto& b=ix(i, j-1);
				const auto& c=ix(i-1, j);
				const auto& d=ix(i, j);
				m.tris.push_back({a, b, c});
				m.tris.push_back({b, d, c});
			}
		}

		m.updateVertexBuffer();
		m.updateIndexBuffer();
		m.updateMatrixes();

		return m;
	}

	static Mesh makeUVSphere(float rad, int num_xz, int num_y) {
		Mesh m;

		//note: loop order matters here.
		for(int i=0; i<num_xz; i++) {
			float u=i/(num_xz-1.f);
			float theta=2*Pi*u;

			float dx=std::cos(theta);
			float dz=std::sin(theta);

			for(int j=0; j<num_y; j++) {
				float v=j/(num_y-1.f);
				float phi=Pi*v;

				float dr=std::sin(phi);
				float nx=dx*dr;
				float ny=std::cos(phi);
				float nz=dz*dr;

				float x=rad*nx;
				float y=rad*ny;
				float z=rad*nz;

				m.verts.push_back({{x, y, z}, {nx, ny, nz}, {u, v}});
			}
		}

		//tessellate grid
		auto ix=[&] (int i, int j) { return j+num_y*i; };
		for(int i=1; i<num_xz; i++) {
			for(int j=1; j<num_y; j++) {
				const auto& a=ix(i-1, j-1);
				const auto& b=ix(i, j-1);
				const auto& c=ix(i-1, j);
				const auto& d=ix(i, j);
				m.tris.push_back({a, b, c});
				m.tris.push_back({b, d, c});
			}
		}

		m.updateVertexBuffer();
		m.updateIndexBuffer();
		m.updateMatrixes();

		return m;
	}

	static Mesh makeCylinder(float rad, int num, float hgt) {
		Mesh m;
		
		//toppole, topedge, topside, btmside, btmedge, btmpole

		float y_top=hgt/2, y_btm=-y_top;
		float v_top=std::atan(2*rad/hgt)/Pi, v_btm=1-v_top;

		//note: push_back order matters here.
		for(int i=0; i<num; i++) {
			float u=i/(num-1.f);
			float theta=2*Pi*u;

			float nx=std::cos(theta);
			float nz=std::sin(theta);

			float x=rad*nx;
			float z=rad*nz;

			m.verts.push_back({{0, y_top, 0}, {0, 1, 0}, {u, 0}});
			m.verts.push_back({{x, y_top, z}, {0, 1, 0}, {u, v_top}});
			m.verts.push_back({{x, y_top, z}, {nx, 0, nz}, {u, v_top}});
			m.verts.push_back({{x, y_btm, z}, {nx, 0, nz}, {u, v_btm}});
			m.verts.push_back({{x, y_btm, z}, {0, -1, 0}, {u, v_btm}});
			m.verts.push_back({{0, y_btm, 0}, {0, -1, 0}, {u, 1}});
		}

		auto ix_tp=[] (int i) { return 6*i; };
		auto ix_te=[] (int i) { return 1+6*i; };
		auto ix_ts=[] (int i) { return 2+6*i; };
		auto ix_bs=[] (int i) { return 3+6*i; };
		auto ix_be=[] (int i) { return 4+6*i; };
		auto ix_bp=[] (int i) { return 5+6*i; };

		//lid
		for(int i=0; i<num; i++) {
			m.tris.push_back({ix_tp(i), ix_te((i+1)%num), ix_te(i)});
		}

		//sides
		for(int i=0; i<num; i++) {
			int ts=ix_ts(i);
			int bs=ix_bs(i);
			int ts_next=ix_ts((i+1)%num);
			int bs_next=ix_bs((i+1)%num);
			m.tris.push_back({ts, ts_next, bs});
			m.tris.push_back({bs, ts_next, bs_next});
		}

		//base
		for(int i=0; i<num; i++) {
			m.tris.push_back({ix_bp(i), ix_be((i+num-1)%num), ix_be(i)});
		}

		m.updateVertexBuffer();
		m.updateIndexBuffer();
		m.updateMatrixes();

		return m;
	}

	static Mesh makeCone(float rad, int num, float hgt) {
		Mesh m;

		//topside, btmside, btmedge, btmpole
		
		float s=std::sqrt(rad*rad+hgt*hgt);
		float hs=hgt/s;
		float rs=rad/s;
		float ny=rad/s;

		float y_top=hgt/2, y_btm=-y_top;
		float v_btm=s/(s+rad);

		//note: push_back order matters here.
		for(int i=0; i<num; i++) {
			float u=i/(num-1.f);
			float theta=2*Pi*u;

			float dx=std::cos(theta);
			float dz=std::sin(theta);

			float nx=hs*dx;
			float nz=hs*dz;

			float x=rad*dx;
			float z=rad*dz;

			m.verts.push_back({{0, y_top, 0}, {nx, ny, nz}, {u, 0}});
			m.verts.push_back({{x, y_btm, z}, {nx, ny, nz}, {u, v_btm}});
			m.verts.push_back({{x, y_btm, z}, {0, -1, 0}, {u, v_btm}});
			m.verts.push_back({{0, y_btm, 0}, {0, -1, 0}, {u, 1}});
		}

		auto ix_ts=[](int i) { return 4*i; };
		auto ix_bs=[](int i) { return 1+4*i; };
		auto ix_be=[](int i) { return 2+4*i; };
		auto ix_bp=[](int i) { return 3+4*i; };

		//side
		for(int i=0; i<num; i++) {
			m.tris.push_back({ix_ts(i), ix_bs((i+1)%num), ix_bs(i)});
		}

		//base
		for(int i=0; i<num; i++) {
			m.tris.push_back({ix_bp(i), ix_be((i+num-1)%num), ix_be(i)});
		}

		m.updateVertexBuffer();
		m.updateIndexBuffer();
		m.updateMatrixes();

		return m;
	}
#pragma endregion

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

					if(v==-1) return {false, "invalid face vertex index"};
					if(n==-1) return {false, "invalid face normal index"};

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
		m.updateMatrixes();

		return {true, "success"};
	}
};
#endif