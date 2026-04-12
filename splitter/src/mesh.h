#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include "cmn/math/v2d.h"

#include <vector>

//for polar
#include "cmn/utils.h"

//for memcpy
#include <string>

//for swap
#include <algorithm>

#include <unordered_map>

//for uint32_t & uint64_t
#include <cstdint>

#include <list>

#include <queue>

struct IndexTriangle {
	int ix[3]{-1, -1, -1};
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

float cross(const cmn::vf2d& a, const cmn::vf2d& b) {
	return a.x*b.y-a.y*b.x;
};

class Mesh {
	cmn::vf2d sc{0, 1};

	std::vector<Mesh> separate() const;

public:
	std::vector<cmn::vf2d> verts;
	std::vector<IndexTriangle> tris;
	cmn::vf2d pos;
	float rot=0;
	float r=1, g=1, b=1;

	//precompute sin & cos for rotation
	void updateMatrix() {
		sc.x=std::sin(rot);
		sc.y=std::cos(rot);
	}

	cmn::vf2d rotVec(const cmn::vf2d& p) const {
		return {p.x*sc.y-p.y*sc.x, p.x*sc.x+p.y*sc.y};
	}

	cmn::vf2d unrotVec(const cmn::vf2d& p) const {
		return {p.x*sc.y+p.y*sc.x, -p.x*sc.x+p.y*sc.y};
	}

	cmn::vf2d loc2wld(const cmn::vf2d& l) const {
		return pos+rotVec(l);
	}

	cmn::vf2d wld2loc(const cmn::vf2d& w) const {
		return unrotVec(w-pos);
	}

	std::vector<Mesh> split(cmn::vf2d ctr, cmn::vf2d norm) const {
		//localize plane
		ctr=wld2loc(ctr);
		norm=unrotVec(norm);

		//classify pts & make lookup
		std::vector<int> i2pn;
		auto i2p=[] (int i) { return 1+i; };
		auto i2n=[] (int i) { return -1-i; };
		auto p2i=[] (int p) { return p-1; };
		auto n2i=[] (int n) { return -1-n; };
		Mesh mesh_pos, mesh_neg;
		for(const auto& v:verts) {
			if(norm.dot(v-ctr)>0) {
				i2pn.push_back(i2p(mesh_pos.verts.size()));
				mesh_pos.verts.push_back(v);
			} else {
				i2pn.push_back(i2n(mesh_neg.verts.size()));
				mesh_neg.verts.push_back(v);
			}
		}

		//all on one side
		if(mesh_pos.verts.empty()||mesh_neg.verts.empty()) return {};

		//store intersections for reuse
		std::unordered_map<IndexEdge, SplitIndex, IndexEdgeHash> cache;
		auto getEdgeIndex=[&] (int i0, int i1) {
			IndexEdge e(i0, i1);

			auto it=cache.find(e);
			if(it!=cache.end()) return it->second;

			const cmn::vf2d& a=verts[i0], ab=verts[i1]-a;
			float t=norm.dot(ctr-a)/norm.dot(ab);
			cmn::vf2d ix=a+t*ab;

			mesh_pos.verts.push_back(ix);
			mesh_neg.verts.push_back(ix);

			SplitIndex is{mesh_pos.verts.size()-1, mesh_neg.verts.size()-1};
			cache[e]=is;
			return is;
		};

		int ix_pos[3], ix_neg[3];
		for(const auto& t:tris) {
			int ct_pos=0, ct_neg=0;
			for(int i=0; i<3; i++) {
				const auto& ix=t.ix[i];
				if(i2pn[ix]>0) ix_pos[ct_pos++]=ix;
				else ix_neg[ct_neg++]=ix;
			}
			switch(ct_pos) {
				case 0://send entire tri to neg
					mesh_neg.tris.push_back({
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
					mesh_pos.tris.push_back({p0, p0n0.pos_ix, p0n1.pos_ix});
					mesh_neg.tris.push_back({p0n0.neg_ix, n0, n1});
					mesh_neg.tris.push_back({p0n0.neg_ix, n1, p0n1.neg_ix});
					break;
				}
				case 2: {//pos quad, neg tri
					auto p0n0=getEdgeIndex(ix_pos[0], ix_neg[0]);
					auto p1n0=getEdgeIndex(ix_pos[1], ix_neg[0]);
					int p0=p2i(i2pn[ix_pos[0]]);
					int p1=p2i(i2pn[ix_pos[1]]);
					int n0=n2i(i2pn[ix_neg[0]]);
					mesh_pos.tris.push_back({p0, p1, p1n0.pos_ix});
					mesh_pos.tris.push_back({p0, p1n0.pos_ix, p0n0.pos_ix});
					mesh_neg.tris.push_back({p1n0.neg_ix, n0, p0n0.neg_ix});
					break;
				}
				case 3://send entire tri to pos
					mesh_pos.tris.push_back({
						p2i(i2pn[ix_pos[0]]),
						p2i(i2pn[ix_pos[1]]),
						p2i(i2pn[ix_pos[2]]),
						});
					break;
			}
		}

		//seperate & conglomerate
		std::vector<Mesh> meshes;
		auto meshes_pos=mesh_pos.separate();
		auto meshes_neg=mesh_neg.separate();
		meshes.insert(
			meshes.end(),
			meshes_pos.begin(),
			meshes_pos.end()
		);
		meshes.insert(
			meshes.end(),
			meshes_neg.begin(),
			meshes_neg.end()
		);

		//apply transforms
		for(int i=0; i<meshes.size(); i++) {
			auto& m=meshes[i];
			m.pos=pos;
			m.rot=rot;
			m.sc=sc;
			m.r=cmn::randFloat();
			m.g=cmn::randFloat();
			m.b=cmn::randFloat();
		}

		return meshes;
	}

	static Mesh makeRect(float w, float h, cmn::vf2d p, float r, float g, float b) {
		Mesh m;
		//push order(CW) matters.
		m.verts.push_back({-w/2, -h/2});
		m.verts.push_back({w/2, -h/2});
		m.verts.push_back({-w/2, h/2});
		m.verts.push_back({w/2, h/2});
		m.tris={{0, 1, 3}, {0, 3, 2}};
		m.pos=p;
		m.updateMatrix();
		m.r=r, m.g=g, m.b=b;
		return m;
	}

	static Mesh makeCircle(float rad, int n, cmn::vf2d p, float r, float g, float b) {
		Mesh m;
		for(int i=0; i<n; i++) {
			float angle=2*cmn::Pi*i/n;
			cmn::vf2d dir=cmn::polar<cmn::vf2d>(1, angle);
			m.verts.push_back(rad*dir);
		}
		for(int i=2; i<n; i++) {
			m.tris.push_back({0, i-1, i});
		}
		m.pos=p;
		m.updateMatrix();
		m.r=r, m.g=g, m.b=b;
		return m;
	}

	static Mesh makeTorus(float big_r, float small_r, int n, cmn::vf2d p, float r, float g, float b) {
		Mesh m;
		for(int i=0; i<n; i++) {
			float angle=2*cmn::Pi*i/n;
			cmn::vf2d dir=cmn::polar<cmn::vf2d>(1, angle);
			m.verts.push_back(small_r*dir);
			m.verts.push_back(big_r*dir);
		}
		for(int i=0; i<n; i++) {
			int a=2*i, b=1+a;
			int c=2*((i+1)%n), d=1+c;
			m.tris.push_back({a, b, c});
			m.tris.push_back({b, d, c});
		}
		m.pos=p;
		m.updateMatrix();
		m.r=r, m.g=g, m.b=b;
		return m;
	}
};

//separate by loose parts.
std::vector<Mesh> Mesh::separate() const {
	//build adjacency list
	std::vector<std::vector<int>> adj(verts.size());
	for(const auto& t:tris) {
		adj[t.ix[0]].push_back(t.ix[1]); adj[t.ix[1]].push_back(t.ix[0]);
		adj[t.ix[1]].push_back(t.ix[2]); adj[t.ix[2]].push_back(t.ix[1]);
		adj[t.ix[2]].push_back(t.ix[0]); adj[t.ix[0]].push_back(t.ix[2]);
	}

	std::vector<Mesh> meshes;
	//which component does ix belong to?
	std::vector<int> ix2part(verts.size(), -1);
	int curr_part=0;

	for(int i=0; i<(int)verts.size(); i++) {
		//skip if filled
		if(ix2part[i]!=-1) continue;

		//floodfill
		std::vector<int> part;
		std::queue<int> q; q.push(i);
		ix2part[i]=curr_part;
		while(!q.empty()) {
			int nxt=q.front(); q.pop();
			part.push_back(nxt);
			for(int nbr:adj[nxt]) {
				if(ix2part[nbr]==-1) {
					ix2part[nbr]=curr_part;
					q.push(nbr);
				}
			}
		}

		//add verts & init lookup
		Mesh sub;
		std::vector<int> remap(verts.size(), -1);
		for(int j=0; j<part.size(); j++) {
			int v=part[j];
			remap[v]=j;
			sub.verts.push_back(verts[v]);
		}

		//add remapped tris
		for(const auto& t:tris) {
			if(
				ix2part[t.ix[0]]==curr_part&&
				ix2part[t.ix[1]]==curr_part&&
				ix2part[t.ix[2]]==curr_part
				) {
				sub.tris.push_back({
					remap[t.ix[0]],
					remap[t.ix[1]],
					remap[t.ix[2]]
					});
			}
		}

		//add the sucker
		meshes.push_back(sub);
		curr_part++;
	}

	return meshes;
}
#endif