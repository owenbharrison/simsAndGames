#pragma once
#ifndef SHAPE_STRUCT_H
#define SHAPE_STRUCT_H

#include "mesh.h"

#include "linemesh.h"

#include "cmn/math/v3d.h"

#include "cmn/math/mat4.h"

//come on you guys!
//there it is right there in front of you the whole time.
//you're dereferencing a null pointer!
//open your eyes.
float segIntersectTri(
	const cmn::vf3d& s0, const cmn::vf3d& s1,
	const cmn::vf3d& t0, const cmn::vf3d& t1, const cmn::vf3d& t2,
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
	cmn::vf3d a=s1-s0;
	cmn::vf3d b=t0-t1;
	cmn::vf3d c=t0-t2;
	cmn::vf3d d=t0-s0;
	cmn::vf3d bxc=b.cross(c);
	float det=a.dot(bxc);
	//parallel
	if(std::abs(det)<epsilon) return -1;

	cmn::vf3d f=c.cross(a)/det;
	float u=f.dot(d);
	if(uptr) *uptr=u;

	cmn::vf3d g=a.cross(b)/det;
	float v=g.dot(d);
	if(vptr) *vptr=v;

	//within unit uv triangle
	if(u<0||u>1) return -1;
	if(v<0||v>1) return -1;
	if(u+v>1) return -1;

	//get t
	cmn::vf3d e=bxc/det;
	float t=e.dot(d);

	//behind ray
	if(t<0) return -1;

	return t;
}

//which pt on triangle surface which is closest to given pt?
//https://stackoverflow.com/a/74395029
cmn::vf3d getClosePt(
	const cmn::vf3d& pt,
	const cmn::vf3d& t0, const cmn::vf3d& t1, const cmn::vf3d& t2
) {
	cmn::vf3d ab=t1-t0;
	cmn::vf3d ac=t2-t0;

	cmn::vf3d ap=pt-t0;
	float d1=ab.dot(ap);
	float d2=ac.dot(ap);
	if(d1<=0&&d2<=0) return t0;

	cmn::vf3d bp=pt-t1;
	float d3=ab.dot(bp);
	float d4=ac.dot(bp);
	if(d3>=0&&d4<=d3) return t1;

	cmn::vf3d cp=pt-t2;
	float d5=ab.dot(cp);
	float d6=ac.dot(cp);
	if(d6>=0&&d5<=d6) return t2;

	float vc=d1*d4-d3*d2;
	if(vc<=0&&d1>=0&&d3<=0) {
		float v=d1/(d1-d3);
		return t0+v*ab;
	}

	float vb=d5*d2-d1*d6;
	if(vb<=0&&d2>=0&&d6<=0) {
		float v=d2/(d2-d6);
		return t0+v*ac;
	}

	float va=d3*d6-d5*d4;
	if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0) {
		float v=(d4-d3)/((d4-d3)+(d5-d6));
		return t1+v*(t2-t1);
	}

	float denom=1/(va+vb+vc);
	float v=vb*denom;
	float w=vc*denom;
	return t0+v*ab+w*ac;
}

struct Shape {
	Mesh mesh;

	LineMesh linemesh;

	sg_view tex{};

	cmn::vf3d translation, rotation, scale{1, 1, 1};
	cmn::mat4 model=cmn::mat4::makeIdentity();
	cmn::mat4 inv_model=cmn::mat4::makeIdentity();

	bool draggable=false;

	Shape() {}

	Shape(const Mesh& m, sg_view t, bool d) {
		mesh=m;
		linemesh=LineMesh::makeFromMesh(m);
		linemesh.randomizeColors();
		linemesh.updateVertexBuffer();
		tex=t;
		draggable=d;
	}

	void updateMatrixes() {
		//xyz euler angles?
		cmn::mat4 rot_x=cmn::mat4::makeRotX(rotation.x);
		cmn::mat4 rot_y=cmn::mat4::makeRotY(rotation.y);
		cmn::mat4 rot_z=cmn::mat4::makeRotZ(rotation.z);
		cmn::mat4 rot=cmn::mat4::mul(rot_z, cmn::mat4::mul(rot_y, rot_x));

		cmn::mat4 scl=cmn::mat4::makeScale(scale);

		cmn::mat4 trans=cmn::mat4::makeTranslation(translation);

		//combine & invert
		model=cmn::mat4::mul(trans, cmn::mat4::mul(rot, scl));
		inv_model=cmn::mat4::inverse(model);
	}

	float intersectRay(const cmn::vf3d& orig_world, const cmn::vf3d& dir_world) const {
		//localize ray
		float w=1;//want translation
		cmn::vf3d orig_local=matMulVec(inv_model, orig_world, w);
		w=0;//no translation
		cmn::vf3d dir_local=matMulVec(inv_model, dir_world, w);
		//renormalization is not necessary here, because
		//  rayIntersect is basically segment intersection,
		//  but it feels wrong not to pass a unit vector.
		dir_local=dir_local.norm();

		//find closest tri
		float record=-1;
		for(const auto& it:mesh.tris) {
			//valid intersection?
			float dist=segIntersectTri(
				orig_local, orig_local+dir_local,
				mesh.verts[it.a].pos,
				mesh.verts[it.b].pos,
				mesh.verts[it.c].pos
			);
			if(dist<0) continue;

			//"sort" while iterating
			if(record<0||dist<record) record=dist;
		}

		//i hate fp== comparisons.
		if(record<0) return -1;

		//get point from local -> world & get dist to orig
		//i need to do this because of the non-uniform scale
		cmn::vf3d p_local=orig_local+record*dir_local;
		w=1;//want translation
		cmn::vf3d p_world=matMulVec(model, p_local, w);
		return (p_world-orig_world).mag();
	}

	//which pt on mesh surface is closest to given pt?
	cmn::vf3d getClosePt(cmn::vf3d pt) const {
		//localize pt
		float w=1;
		pt=cmn::matMulVec(inv_model, pt, w);

		//which tri has the closest pt?
		float record=-1;
		cmn::vf3d closest_pt;
		for(const auto& t:mesh.tris) {
			cmn::vf3d close_pt=::getClosePt(pt,
				mesh.verts[t.a].pos,
				mesh.verts[t.b].pos,
				mesh.verts[t.c].pos
			);
			float dist_sq=(close_pt-pt).mag_sq();
			if(record<0||dist_sq<record) {
				record=dist_sq;
				closest_pt=close_pt;
			}
		}

		//worldize pt
		w=1;
		return cmn::matMulVec(model, closest_pt, w);
	}
};
#endif