#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include "cmn/math/mat4.h"

#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#include <unordered_map>
//for hash
#include <functional>

cmn::vf3d segIntersectPlane(
	const cmn::vf3d& a, const cmn::vf3d& b,
	const cmn::vf3d& ctr, const cmn::vf3d& norm
) {
	cmn::vf3d ab=b-a;
	float t=dot(norm, ctr-a)/dot(norm, ab);
	return a+t*ab;
}

struct IndexTriangle {
	int ix[3];
	float r=1, g=1, b=1;
};

//sorted edges
struct IndexEdge {
	int a, b;

	IndexEdge(int v0, int v1) {
		a=v0, b=v1;
		if(a>b) std::swap(a, b);
	}

	bool operator==(const IndexEdge& e) const {
		return e.a==a&&e.b==b;
	}
};

struct IndexEdgeHash {
	std::uint64_t operator()(const IndexEdge& e) const {
		std::uint64_t a=e.a, b=e.b;
		return (a<<32)|b;
	}
};

struct SplitIndex {
	int pos_ix, neg_ix;
};

struct Mesh {
	std::vector<cmn::vf3d> verts;
	std::vector<IndexTriangle> tris;

	bool splitByPlane(
		const cmn::vf3d& ctr, const cmn::vf3d norm,
		Mesh& ahead, Mesh& behind
	) const {
		//classify pts & make lookup
		std::vector<int> i2pn;
		auto i2p=[] (int i) { return 1+i; };
		auto i2n=[] (int i) { return -1-i; };
		auto p2i=[] (int p) { return p-1; };
		auto n2i=[] (int n) { return -1-n; };
		for(const auto& v:verts) {
			if(norm.dot(v-ctr)>0) {
				i2pn.push_back(i2p(ahead.verts.size()));
				ahead.verts.push_back(v);
			} else {
				i2pn.push_back(i2n(behind.verts.size()));
				behind.verts.push_back(v);
			}
		}

		//all on one side
		if(ahead.verts.empty()||behind.verts.empty()) return false;

		//store intersections for reuse
		std::unordered_map<IndexEdge, SplitIndex, IndexEdgeHash> cache;
		auto getEdgeIndex=[&] (int i0, int i1) {
			IndexEdge e(i0, i1);

			auto it=cache.find(e);
			if(it!=cache.end()) return it->second;

			cmn::vf3d ix=segIntersectPlane(verts[i0], verts[i1], ctr, norm);

			ahead.verts.push_back(ix);
			behind.verts.push_back(ix);

			SplitIndex is;
			is.pos_ix=ahead.verts.size()-1;
			is.neg_ix=behind.verts.size()-1;
			cache[e]=is;
			return is;
		};

		//add tri to corresponding mesh & ensure correct winding
		auto addTriFacing=[] (Mesh& m, IndexTriangle t, const cmn::vf3d& n) {
			cmn::vf3d ab=m.verts[t.ix[1]]-m.verts[t.ix[0]];
			cmn::vf3d ac=m.verts[t.ix[2]]-m.verts[t.ix[0]];
			if(dot(cross(ab, ac), n)<0) std::swap(t.ix[1], t.ix[2]);
			m.tris.push_back(t);
		};

		//split each tri
		int ix_pos[3], ix_neg[3];
		for(const auto& t:tris) {
			cmn::vf3d ab=verts[t.ix[1]]-verts[t.ix[0]];
			cmn::vf3d ac=verts[t.ix[2]]-verts[t.ix[0]];
			cmn::vf3d norm=normalize(cross(ab, ac));
			int ct_pos=0, ct_neg=0;
			for(int i=0; i<3; i++) {
				const auto& ix=t.ix[i];
				if(i2pn[ix]>0) ix_pos[ct_pos++]=ix;
				else ix_neg[ct_neg++]=ix;
			}
			switch(ct_pos) {
				case 0://send entire tri to neg
					behind.tris.push_back({
						n2i(i2pn[ix_neg[0]]),
						n2i(i2pn[ix_neg[1]]),
						n2i(i2pn[ix_neg[2]]),
						});
					break;
				case 1: {//pos tri, neg quad
					auto p0n0=getEdgeIndex(ix_pos[0], ix_neg[0]);
					auto p0n1=getEdgeIndex(ix_pos[0], ix_neg[1]);
					int p0=p2i(i2pn[ix_pos[0]]);
					int n0=n2i(i2pn[ix_neg[0]]);
					int n1=n2i(i2pn[ix_neg[1]]);
					addTriFacing(ahead, {p0, p0n0.pos_ix, p0n1.pos_ix, 1, 0, 0}, norm);//red
					addTriFacing(behind, {p0n0.neg_ix, n0, n1, 0, 1, 0}, norm);//green
					addTriFacing(behind, {p0n0.neg_ix, n1, p0n1.neg_ix, 0, 1, 1}, norm);//cyan
					break;
				}
				case 2: {//pos quad, neg tri
					auto p0n0=getEdgeIndex(ix_pos[0], ix_neg[0]);
					auto p1n0=getEdgeIndex(ix_pos[1], ix_neg[0]);
					int p0=p2i(i2pn[ix_pos[0]]);
					int p1=p2i(i2pn[ix_pos[1]]);
					int n0=n2i(i2pn[ix_neg[0]]);
					addTriFacing(ahead, {p0, p1, p1n0.pos_ix, 1, 1, 0}, norm);//yellow
					addTriFacing(ahead, {p0, p1n0.pos_ix, p0n0.pos_ix, 1, 0, 1}, norm);//magenta
					addTriFacing(behind, {p1n0.neg_ix, n0, p0n0.neg_ix, 0, 0, 1}, norm);//blue
					break;
				}
				case 3://send entire tri to pos
					ahead.tris.push_back({
						p2i(i2pn[ix_pos[0]]),
						p2i(i2pn[ix_pos[1]]),
						p2i(i2pn[ix_pos[2]]),
						});
					break;
			}
		}

		return true;
	}


	static bool loadFromOBJ(Mesh& m, const std::string& filename) {
		std::ifstream file(filename);
		if(file.fail()) return false;

		m={};

		//parse file line by line
		std::string line;
		while(std::getline(file, line)) {
			std::stringstream line_str(line);
			std::string type; line_str>>type;
			if(type=="v") {
				cmn::vf3d v;
				line_str>>v.x>>v.y>>v.z;
				m.verts.push_back(v);
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
					m.tris.push_back({
						v_ixs[0],
						v_ixs[i-1],
						v_ixs[i]
						});
				}
			}
		}

		file.close();

		return true;
	}
};
#endif