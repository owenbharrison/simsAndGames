#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

struct IndexTriangle {
	int x=0, y=0, z=0;
	olc::Pixel col=olc::WHITE;

	int& operator[](int i) {
		if(i==1) return y;
		if(i==2) return z;
		return x;
	}

	const int& operator[](int i) const {
		if(i==1) return y;
		if(i==2) return z;
		return x;
	}
};

struct IndexEdge {
	int a=0, b=0;

	//sorted edges
	IndexEdge(int a_, int b_) {
		a=a_, b=b_;
		if(a>b) std::swap(a, b);
	}
};

#include <unordered_map>
#include <utility>

struct EdgeHash {
	std::size_t operator()(const IndexEdge& e) const {
		std::hash<int> hasher;
		return hasher(e.a)^(hasher(e.b)<<1);
	}
};

struct EdgeEqual {
	bool operator()(const IndexEdge& a, const IndexEdge& b) const {
		return a.a==b.a&&a.b==b.b;
	}
};

struct Mesh {
	std::vector<vf3d> verts;
	std::vector<IndexTriangle> index_tris;
	std::vector<Triangle> tris;
	int id=-1;

	void triangulate() {
		tris.clear();
		for(const auto& it:index_tris) {
			Triangle t{
				verts[it.x],
				verts[it.y],
				verts[it.z]
			}; t.col=it.col;
			tris.push_back(t);
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

	bool splitByPlane(const vf3d& ctr, const vf3d& norm, Mesh& pos, Mesh& neg) const {
		//classify and populate verts and initialize lookup
		std::vector<bool> sides;
		sides.reserve(verts.size());
		std::unordered_map<int, int> me_pos, me_neg;
		for(int i=0; i<verts.size(); i++) {
			bool side=norm.dot(verts[i]-ctr)>0;
			if(side) {
				me_pos[i]=me_pos.size();
				pos.verts.push_back(verts[i]);
			} else {
				me_neg[i]=me_neg.size();
				neg.verts.push_back(verts[i]);
			}
			sides.push_back(side);
		}

		//plane doesnt intersect(all on one side)
		if(me_pos.empty()||me_neg.empty()) return false;

		//clip each tri
		std::unordered_map<IndexEdge, std::pair<int, int>, EdgeHash, EdgeEqual> edge_pn;
		int pos_pts[3], neg_pts[3];
		for(const auto& it:index_tris) {
			vf3d it_ba=verts[it[1]]-verts[it[0]];
			vf3d it_ca=verts[it[2]]-verts[it[0]];
			vf3d orig_surf=it_ba.cross(it_ca);
			//wind pts
			int pos_ct=0, neg_ct=0;
			for(int i=0; i<3; i++) {
				const auto& ix=it[i];
				if(sides[ix]) pos_pts[pos_ct++]=ix;
				else neg_pts[neg_ct++]=ix;
			}
			switch(pos_ct) {
				case 0://send tri to neg
					neg.index_tris.push_back({
						me_neg[neg_pts[0]],
						me_neg[neg_pts[1]],
						me_neg[neg_pts[2]]
						});
					break;
				case 1: {//clip tri
					std::pair<int, int> pn1;
					IndexEdge e1(pos_pts[0], neg_pts[0]);
					auto pn1it=edge_pn.find(e1);
					//reuse
					if(pn1it!=edge_pn.end()) pn1=pn1it->second;
					else {//make
						vf3d pt=segIntersectPlane(
							verts[pos_pts[0]], verts[neg_pts[0]],
							ctr, norm
						);
						pn1={pos.verts.size(), neg.verts.size()}, edge_pn[e1]=pn1;
						pos.verts.push_back(pt), neg.verts.push_back(pt);
					}

					std::pair<int, int> pn2;
					IndexEdge e2(pos_pts[0], neg_pts[1]);
					auto pn2it=edge_pn.find(e2);
					//reuse
					if(pn2it!=edge_pn.end()) pn2=pn2it->second;
					else {//make
						vf3d pt=segIntersectPlane(
							verts[pos_pts[0]], verts[neg_pts[1]],
							ctr, norm
						);
						pn2={pos.verts.size(), neg.verts.size()}, edge_pn[e2]=pn2;
						pos.verts.push_back(pt), neg.verts.push_back(pt);
					}

					IndexTriangle newp1{me_pos[pos_pts[0]], pn1.first, pn2.first, olc::YELLOW};
					IndexTriangle newn1{pn1.second, me_neg[neg_pts[0]], me_neg[neg_pts[1]], olc::CYAN};
					IndexTriangle newn2{pn1.second, me_neg[neg_pts[1]], pn2.second, olc::GREEN};

					vf3d cl_ba=verts[neg_pts[0]]-verts[pos_pts[0]];
					vf3d cl_ca=verts[neg_pts[1]]-verts[pos_pts[0]];
					vf3d cl_surf=cl_ba.cross(cl_ca);
					if(orig_surf.dot(cl_surf)<0) {
						std::swap(newp1.x, newp1.y);
						std::swap(newn1.x, newn1.y);
						std::swap(newn2.x, newn2.y);
					}

					pos.index_tris.push_back(newp1);
					neg.index_tris.push_back(newn1);
					neg.index_tris.push_back(newn2);

					break;
				}
				case 2: {//clip tri
					std::pair<int, int> pn1;
					IndexEdge e1(pos_pts[0], neg_pts[0]);
					auto pn1it=edge_pn.find(e1);
					//reuse
					if(pn1it!=edge_pn.end()) pn1=pn1it->second;
					else {//make
						vf3d pt=segIntersectPlane(
							verts[pos_pts[0]], verts[neg_pts[0]],
							ctr, norm
						);
						pn1={pos.verts.size(), neg.verts.size()}, edge_pn[e1]=pn1;
						pos.verts.push_back(pt), neg.verts.push_back(pt);
					}

					std::pair<int, int> pn2;
					IndexEdge e2(pos_pts[1], neg_pts[0]);
					auto pn2it=edge_pn.find(e2);
					//reuse
					if(pn2it!=edge_pn.end()) pn2=pn2it->second;
					else {//make
						vf3d pt=segIntersectPlane(
							verts[pos_pts[1]], verts[neg_pts[0]],
							ctr, norm
						);
						pn2={pos.verts.size(), neg.verts.size()}, edge_pn[e2]=pn2;
						pos.verts.push_back(pt), neg.verts.push_back(pt);
					}
					
					IndexTriangle newp1{me_pos[pos_pts[0]], pn1.first, pn2.first, olc::MAGENTA};
					IndexTriangle newp2{me_pos[pos_pts[0]], pn2.first, me_pos[pos_pts[1]], olc::RED};
					IndexTriangle newn1{pn1.second, me_neg[neg_pts[0]], pn2.second, olc::BLUE};

					vf3d cl_ba=verts[neg_pts[0]]-verts[pos_pts[0]];
					vf3d cl_ca=verts[pos_pts[1]]-verts[pos_pts[0]];
					vf3d cl_surf=cl_ba.cross(cl_ca);
					if(orig_surf.dot(cl_surf)<0) {
						std::swap(newp1.x, newp1.y);
						std::swap(newp2.x, newp2.y);
						std::swap(newn1.x, newn1.y);
					}

					pos.index_tris.push_back(newp1);
					pos.index_tris.push_back(newp2);
					neg.index_tris.push_back(newn1);

					break;
				}
				case 3://send tri to pos
					pos.index_tris.push_back({
						me_pos[pos_pts[0]],
						me_pos[pos_pts[1]],
						me_pos[pos_pts[2]]
						});
					break;
			}
		}

		pos.triangulate();
		neg.triangulate();

		return true;
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
					m.index_tris.push_back({
						v_ixs[0],
						v_ixs[i-1],
						v_ixs[i]
						});
				}
			}
		}

		m.triangulate();

		file.close();

		return m;
	}
};
#endif