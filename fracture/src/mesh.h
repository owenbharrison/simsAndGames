#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

typedef v3d_generic<int> IndexTriangle;

struct IndexEdge {
	int a=0, b=0;

	//sorted edges
	static IndexEdge make(int a, int b) {
		if(a>b) std::swap(a, b);
		return {a, b};
	}
};

#include <unordered_map>

struct Mesh {
	std::vector<vf3d> verts;
	std::vector<IndexTriangle> index_tris;
	std::vector<Triangle> tris;
	int id=-1;

	void triangulate() {
		tris.clear();
		for(const auto& it:index_tris) {
			tris.push_back({
				verts[it.x],
				verts[it.y],
				verts[it.z]
			});
		}
	}

	void color(const olc::Pixel& col) {
		for(auto& t:tris) t.col=col;
	}

	void colorByNormals() {
		for(auto& t:tris) {
			vf3d norm=t.getNorm(), rgb=.5f+.5f*norm;
			float l=std::max(rgb.x, std::max(rgb.y, rgb.z));
			t.col=olc::PixelF(rgb.x/l, rgb.y/l, rgb.z/l);
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
		std::unordered_map<int, int> me2pos, me2neg;
		{
			for(int i=0; i<verts.size(); i++) {
				bool side=norm.dot(verts[i]-ctr)>0;
				if(side) {
					me2pos[i]=me2pos.size();
					pos.verts.push_back(verts[i]);
				} else {
					me2neg[i]=me2neg.size();
					neg.verts.push_back(verts[i]);
				}
				sides.push_back(side);
			}

			//plane doesnt intersect(all on one side)
			if(me2pos.empty()||me2neg.empty()) return false;
		}

		{
			int pos_ix[3], neg_ix[3];
			for(const auto& it:index_tris) {
				int pos_ct=0, neg_ct=0;
				for(int i=0; i<3; i++) {
					const auto& ix=it[i];
					if(sides[ix]) pos_ix[pos_ct++]=ix;
					else neg_ix[neg_ct++]=ix;
				}
				switch(pos_ct) {
					case 0: //send tri to neg
						neg.index_tris.push_back({
							me2neg[neg_ix[0]],
							me2neg[neg_ix[1]],
							me2neg[neg_ix[2]]
							});
						break;
					case 3: //send tri to pos
						pos.index_tris.push_back({
							me2pos[pos_ix[0]],
							me2pos[pos_ix[1]],
							me2pos[pos_ix[2]]
							});
						break;
				}
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