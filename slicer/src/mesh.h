#pragma once
#ifndef MESH_CLASS_H
#define MESH_CLASS_H

#include "cmn/geom/aabb3.h"

#include "cmn/math/mat4.h"

#include <vector>

#include "cmn/utils.h"

#include <string>

#include <fstream>

#include <sstream>

struct IndexTriangle {
	int a=0, b=0, c=0;
};

bool segIntersectTri(
	const cmn::vf3d& s0, const cmn::vf3d& s1,
	const cmn::vf3d& t0, const cmn::vf3d& t1, const cmn::vf3d& t2,
	float* tptr=nullptr, float* uptr=nullptr, float* vptr=nullptr
) {
	/*
	segment: p=s0+t(s1-s0)
	triangle: p=t0+u(t1-t0)+v(t2-t0)
	set equal: t(s1-s0)+u(t0-t1)+v(t0-t2)=t0-s0
	system of 3 eqs(x,y,z): solve like matrix!
	*/

	//column vectors
	cmn::vf3d a=s1-s0;
	cmn::vf3d b=t0-t1;
	cmn::vf3d c=t0-t2;
	cmn::vf3d d=t0-s0;

	//triple product to find det
	cmn::vf3d bxc=cross(b, c);
	float det=dot(a, bxc);

	//parallel
	if(std::abs(det)<1e-6f) return false;

	//cramer's rule <3
	float recip=1/det;
	float t=recip*dot(d, bxc);
	float u=recip*dot(a, cross(d, c));
	float v=recip*dot(a, cross(b, d));

	if(tptr) *tptr=t;
	if(uptr) *uptr=u;
	if(tptr) *vptr=v;

	//within unit segment
	if(t<0||t>1) return false;

	//within unit uv triangle
	if(u<0||u>1) return false;
	if(v<0||v>1) return false;
	if(u+v>1) return false;

	return true;
}

struct Mesh {
	std::vector<cmn::vf3d> verts;
	std::vector<IndexTriangle> tris;

	cmn::vf3d scale{1, 1, 1}, rot, trans;
	cmn::mat4 model, inv_model;

	//combine all transforms
	void updateMatrixes() {
		cmn::mat4 m_scale=cmn::mat4::makeScale(scale);
		cmn::mat4 rot_x=cmn::mat4::makeRotX(rot.x);
		cmn::mat4 rot_y=cmn::mat4::makeRotY(rot.y);
		cmn::mat4 rot_z=cmn::mat4::makeRotZ(rot.z);
		cmn::mat4 m_rot=cmn::mat4::mul(rot_z, cmn::mat4::mul(rot_y, rot_x));
		cmn::mat4 m_trans=cmn::mat4::makeTranslation(trans);
		model=cmn::mat4::mul(m_trans, cmn::mat4::mul(m_rot, m_scale));
		inv_model=cmn::mat4::inverse(model);
	}

	cmn::AABBf3 getLocalAABB() const {
		const cmn::vf3d inf(1e300, 1e300, 1e300);
		cmn::AABBf3 box{inf, -inf};
		for(const auto& v:verts) {
			box.fitToEnclose(v);
		}
		return box;
	}

	//polyhedra raycasting algorithm
	bool contains(cmn::vf3d pos) const {
		//localize start pt
		float w=1;
		pos=matMulVec(inv_model, pos, w);
		
		//random direction
		auto dir=normalize(.5f-cmn::vf3d(
			cmn::randFloat(),
			cmn::randFloat(),
			cmn::randFloat()
		));

		//for every tri
		int num=0;
		for(const auto& tri:tris) {
			float t, u, v;
			segIntersectTri(
				pos, pos+dir,
				verts[tri.a],
				verts[tri.b],
				verts[tri.c],
				&t, &u, &v
			);
			if(t>0&&u>0&&u<1&&v>0&&v<1&&u+v<1) num++;
		}

		//odd? inside!
		return num%2;
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

		m.updateMatrixes();

		return true;
	}
};
#endif