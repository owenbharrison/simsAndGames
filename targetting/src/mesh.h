#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

struct IndexTriangle {
	int a=0, b=0, c=0;
};

struct Mesh {
	std::vector<vf3d> vertices;
	std::vector<IndexTriangle> index_tris;
	vf3d rotation;
	vf3d scale{1, 1, 1};
	vf3d translation;
	Mat4 mat_world;//local->world
	Mat4 mat_local;//world->local
	std::vector<Triangle> tris;

	void updateMatrices() {
		//combine all transforms
		Mat4 mat_rot_x=Mat4::makeRotX(rotation.x);
		Mat4 mat_rot_y=Mat4::makeRotY(rotation.y);
		Mat4 mat_rot_z=Mat4::makeRotZ(rotation.z);
		Mat4 mat_rot=mat_rot_x*mat_rot_y*mat_rot_z;
		Mat4 mat_scale=Mat4::makeScale(scale.x, scale.y, scale.z);
		Mat4 mat_trans=Mat4::makeTrans(translation.x, translation.y, translation.z);
		mat_world=mat_rot*mat_scale*mat_trans;
		//matrix could be singular
		mat_local=Mat4::identity();
		try {
			Mat4 local=Mat4::inverse(mat_world);
			mat_local=local;
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
		}
	}

	void updateTris() {
		std::vector<vf3d> new_verts;
		new_verts.reserve(vertices.size());
		for(const auto& v:vertices) {
			new_verts.push_back(v*mat_world);
		}

		tris.clear();
		tris.reserve(index_tris.size());
		for(const auto& t:index_tris) {
			tris.push_back({
				new_verts[t.a],
				new_verts[t.b],
				new_verts[t.c]
				});
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
				m.vertices.push_back(v);
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

		file.close();

		m.updateMatrices();
		m.updateTris();

		return m;
	}
};
#endif