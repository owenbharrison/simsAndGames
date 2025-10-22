#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include <vector>
#include <exception>
#include <utility>
#include <unordered_map>

struct IndexTriangle {
	int x=0, y=0, z=0;

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

struct Mesh {
	std::vector<vf3d> verts;
	std::vector<IndexTriangle> index_tris;

	vf3d rotation;
	vf3d scale{1, 1, 1};
	vf3d offset;
	cmn::Mat4 mat_rot, mat_inv_rot;
	cmn::Mat4 mat_total, mat_inv_total;

	std::vector<cmn::Triangle> tris;

	void updateTransforms() {
		//combine all transforms
		cmn::Mat4 mat_rot_x=cmn::Mat4::makeRotX(rotation.x);
		cmn::Mat4 mat_rot_y=cmn::Mat4::makeRotY(rotation.y);
		cmn::Mat4 mat_rot_z=cmn::Mat4::makeRotZ(rotation.z);
		mat_rot=mat_rot_x*mat_rot_y*mat_rot_z;
		try {//try find inverse of rotation matrix
			mat_inv_rot=cmn::Mat4::inverse(mat_rot);
		} catch(const std::exception& e) {
			mat_inv_rot=cmn::Mat4::makeIdentity();
		}
		cmn::Mat4 mat_scale=cmn::Mat4::makeScale(scale.x, scale.y, scale.z);
		cmn::Mat4 mat_trans=cmn::Mat4::makeTrans(offset.x, offset.y, offset.z);
		mat_total=mat_scale*mat_rot*mat_trans;
		try {//try find inverse of total matrix
			mat_inv_total=cmn::Mat4::inverse(mat_total);
		} catch(const std::exception& e) {
			mat_inv_total=cmn::Mat4::makeIdentity();
		}
	}

	void updateTriangles(const olc::Pixel& col=olc::WHITE) {
		//transform verts
		std::vector<vf3d> new_verts;
		new_verts.reserve(verts.size());
		for(const auto& v:verts) {
			new_verts.push_back(v*mat_total);
		}

		//triangulate
		tris.clear();
		tris.reserve(index_tris.size());
		for(const auto& it:index_tris) {
			cmn::Triangle t{
				new_verts[it.x],
				new_verts[it.y],
				new_verts[it.z]
			}; t.col=col;
			tris.push_back(t);
		}
	}

	//color triangles to their normals
	void colorNormally() {
		for(auto& t:tris) {
			vf3d norm=t.getNorm(), rgb=.5f+.5f*norm;
			float l=std::max(rgb.x, std::max(rgb.y, rgb.z));
			t.col=olc::PixelF(rgb.x/l, rgb.y/l, rgb.z/l);
		}
	}

	cmn::AABB3 getLocalAABB() const {
		cmn::AABB3 box;
		for(const auto& v:verts) {
			box.fitToEnclose(v);
		}
		return box;
	}

	//polyhedra raycasting algorithm
	bool contains(const vf3d& pos) const {
		if(!getLocalAABB().contains(pos*mat_inv_total)) return false;
		
		//random direction
		vf3d dir(cmn::randFloat(), cmn::randFloat(), cmn::randFloat());
		dir=(.5f-dir).norm();

		//for every tri
		int num=0;
		for(const auto& t:tris) {
			float dist=t.intersectSeg(pos, pos+dir);
			if(dist>0) num++;
		}

		//odd? inside!
		return num%2;
	}

	bool splitByPlane(const vf3d& ctr, const vf3d& norm, Mesh& pos, Mesh& neg) const {
		struct IndexEdge {
			int a=0, b=0;

			//sorted edges
			IndexEdge(int a_, int b_) {
				a=a_, b=b_;
				if(a>b) std::swap(a, b);
			}
		};

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
		
		//copy over transforms
		pos.rotation=rotation;
		neg.rotation=rotation;
		pos.scale=scale;
		neg.scale=scale;
		pos.offset=offset;
		neg.offset=offset;
		pos.updateTransforms();
		neg.updateTransforms();

		//localize plane
		vf3d local_ctr=ctr*mat_inv_total;
		vf3d local_norm=norm*mat_inv_rot;

		//classify and populate verts
		std::vector<bool> sides;
		sides.reserve(verts.size());
		//my index to pos & neg indexes
		std::unordered_map<int, int> me_pos, me_neg;
		for(int i=0; i<verts.size(); i++) {
			bool side=local_norm.dot(verts[i]-local_ctr)>0;
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
							local_ctr, local_norm
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
							local_ctr, local_norm
						);
						pn2={pos.verts.size(), neg.verts.size()}, edge_pn[e2]=pn2;
						pos.verts.push_back(pt), neg.verts.push_back(pt);
					}

					IndexTriangle newp1{me_pos[pos_pts[0]], pn1.first, pn2.first};
					IndexTriangle newn1{pn1.second, me_neg[neg_pts[0]], me_neg[neg_pts[1]]};
					IndexTriangle newn2{pn1.second, me_neg[neg_pts[1]], pn2.second};

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
							local_ctr, local_norm
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
							local_ctr, local_norm
						);
						pn2={pos.verts.size(), neg.verts.size()}, edge_pn[e2]=pn2;
						pos.verts.push_back(pt), neg.verts.push_back(pt);
					}

					IndexTriangle newp1{me_pos[pos_pts[0]], pn1.first, pn2.first};
					IndexTriangle newp2{me_pos[pos_pts[0]], pn2.first, me_pos[pos_pts[1]]};
					IndexTriangle newn1{pn1.second, me_neg[neg_pts[0]], pn2.second};

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

		pos.updateTriangles();
		neg.updateTriangles();

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

		m.updateTransforms();
		m.updateTriangles();

		file.close();

		return m;
	}
};
#endif