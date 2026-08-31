/*todo:
impl volume calculation
	use density for mass
return cylinder axes?
*/
#pragma once
#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

#include "particle.h"

#include "constraint.h"

#include "spring.h"

#include <list>

#include <vector>

#include "common/geom/aabb3.h"

//for pi
#include "common/utils.h"

#include <unordered_map>

class Shape {
	void copyFrom(const Shape&), clear();

	void constrainAll();

public:
	std::list<Particle> particles;
	std::vector<Constraint> constraints;
	std::vector<Spring> springs;

	struct Triangle {
		Particle* a, * b, * c;
	};
	std::vector<Triangle> tris;

	float r=1, g=1, b=1;

	cmn::AABBf3 getAABB() const {
		const cmn::vf3d inf(1e300, 1e300, 1e300);
		cmn::AABBf3 box{inf, -inf};
		for(const auto& p:particles) {
			box.fitToEnclose(p.pos);
		}
		return box;
	}

	//centered
	static Shape makePrism(
		const cmn::vf3d& ctr,
		const cmn::vf3d& sz,
		float pr, float pm,
		float r, float g, float b
	) {
		static const int tris[12][3]{
			{1, 3, 5}, {5, 3, 7},//+x
			{3, 2, 7}, {7, 2, 6},//+y
			{5, 7, 4}, {4, 7, 6},//+z
			{4, 6, 0}, {0, 6, 2},//-x
			{0, 1, 4}, {4, 1, 5},//-y
			{0, 2, 1}, {1, 2, 3}//-z
		};

		Shape shp;

		//xyz01: 000, 100, 010, 110, 001, 101, 011, 111
		Particle* cube[8];
		for(int i=0; i<8; i++) {
			int x=1&(i>>0), y=1&(i>>1), z=1&(i>>2);
			auto off=sz*cmn::vf3d(x-.5f, y-.5f, z-.5f);
			shp.particles.push_back(Particle(ctr+off, pr, pm));
			cube[i]=&shp.particles.back();
		}

		shp.constrainAll();

		for(int i=0; i<12; i++) {
			shp.tris.push_back({
				cube[tris[i][0]],
				cube[tris[i][1]],
				cube[tris[i][2]],
				});
		}

		shp.r=r;
		shp.g=g;
		shp.b=b;

		return shp;
	}

	static Shape makeCylinder(
		const cmn::vf3d& p,
		const cmn::vf3d& q,
		float rad, int num,
		float pr, float pm,
		float r, float g, float b
	) {
		Shape shp;

		//coordinate system along segment
		cmn::vf3d y=normalize(q-p);
		cmn::vf3d x=normalize(
			std::abs(y.x)>std::abs(y.z)?
			cmn::vf3d(-y.y, y.x, 0):
			cmn::vf3d(0, -y.z, y.y)
		);
		cmn::vf3d z=cross(x, y);

		//place & store particles
		//even on p cap, odd on q cap
		Particle** grid=new Particle*[2*(1+num)];
		for(int i=0; i<=num; i++) {
			float dx=0, dz=0;
			if(i!=0) {
				float angle=2*cmn::Pi*i/num;
				dx=std::cos(angle);
				dz=std::sin(angle);
			}
			cmn::vf3d off=rad*(dx*x+dz*z);
			shp.particles.push_back(Particle(p+off, pr, pm));
			grid[2*i]=&shp.particles.back();
			shp.particles.push_back(Particle(q+off, pr, pm));
			grid[1+2*i]=&shp.particles.back();
		}

		shp.constrainAll();

		//tessellate surface
		for(int i=0; i<num; i++) {
			//surface element: wrap
			int j=(1+i)%num;
			int pc=2*(1+i), qc=1+pc;
			int pn=2*(1+j), qn=1+pn;
			//caps
			shp.tris.push_back({grid[0], grid[pc], grid[pn]});
			shp.tris.push_back({grid[1], grid[qn], grid[qc]});
			//edge
			shp.tris.push_back({grid[pc], grid[qc], grid[pn]});
			shp.tris.push_back({grid[qc], grid[qn], grid[pn]});
		}

		delete[] grid;

		shp.r=r;
		shp.g=g;
		shp.b=b;

		return shp;
	}

	static Shape makeCone(
		const cmn::vf3d& p,
		const cmn::vf3d& q,
		float rad, int num,
		float pr, float pm,
		float r, float g, float b
	) {
		Shape shp;

		//coordinate system along segment
		cmn::vf3d y=normalize(q-p);
		cmn::vf3d x=normalize(
			std::abs(y.x)>std::abs(y.z)?
			cmn::vf3d(-y.y, y.x, 0):
			cmn::vf3d(0, -y.z, y.y)
		);
		cmn::vf3d z=cross(x, y);

		//place & store particles
		//01=axis, else=base
		Particle** grid=new Particle*[2+num];
		for(int i=0; i<2+num; i++) {
			float dx=0, dz=0;
			cmn::vf3d ctr=p;
			if(i==0) ctr=q;
			else if(i==1);
			else {
				float angle=2*cmn::Pi*i/num;
				dx=std::cos(angle);
				dz=std::sin(angle);
			}
			cmn::vf3d off=rad*(dx*x+dz*z);
			shp.particles.push_back(Particle(ctr+off, pr, pm));
			grid[i]=&shp.particles.back();
		}

		shp.constrainAll();

		//tessellate surface
		for(int i=0; i<num; i++) {
			//edge element: wrap
			int j=(1+i)%num;
			int c=2+i, n=2+j;
			//slope
			shp.tris.push_back({grid[0], grid[n], grid[c]});
			//base
			shp.tris.push_back({grid[1], grid[c], grid[n]});
		}

		delete[] grid;

		shp.r=r;
		shp.g=g;
		shp.b=b;

		return shp;
	}

	static Shape makeTorus(
		const cmn::vf3d& ctr,
		float rad_xz, int num_xz,
		float rad_y, int num_y,
		float pr, float pm,
		float r, float g, float b
	) {
		Shape shp;

		auto ix=[&] (int i, int j) { return j+num_y*i; };
		Particle** grid=new Particle*[num_xz*num_y];

		const float inv_num_xz=1.f/num_xz;
		const float inv_num_y=1.f/num_y;
		for(int i=0; i<num_xz; i++) {
			float theta=2*cmn::Pi*i*inv_num_xz;

			//offset from big radius
			float dx=std::sin(theta);
			float ox=rad_xz*dx;
			float dz=std::cos(theta);
			float oz=rad_xz*dz;

			for(int j=0; j<num_y; j++) {
				float phi=2*cmn::Pi*j*inv_num_y;

				//scale xz by little radius
				float dr=std::sin(phi);
				float nx=dx*dr;
				float ny=std::cos(phi);
				float nz=dz*dr;
				float x=ox+rad_y*nx;
				float z=oz+rad_y*nz;
				float y=rad_y*ny;

				auto pos=ctr+cmn::vf3d(x, y, z);
				shp.particles.push_back(Particle(pos, pr, pm));
				grid[ix(i, j)]=&shp.particles.back();
			}
		}

		shp.constrainAll();

		//tessellate grid into surface
		for(int i=0; i<num_xz; i++) {
			for(int j=0; j<num_y; j++) {
				int ni=(1+i)%num_xz;
				int nj=(1+j)%num_y;
				const auto& cc=ix(i, j);
				const auto& nc=ix(ni, j);
				const auto& cn=ix(i, nj);
				const auto& nn=ix(ni, nj);
				shp.tris.push_back({grid[cc], grid[cn], grid[nc]});
				shp.tris.push_back({grid[cn], grid[nn], grid[nc]});
			}
		}

		delete[] grid;

		shp.r=r;
		shp.g=g;
		shp.b=b;

		return shp;
	}
};

void Shape::copyFrom(const Shape& shp) {
	//copy particles & init lookup
	std::unordered_map<const Particle*, Particle*> s2me;
	for(const auto& p:shp.particles) {
		particles.push_back(p);
		s2me[&p]=&particles.back();
	}

	//copy connections & change ptrs w/ relativity
	constraints=shp.constraints;
	for(auto& c:constraints) {
		c.a=s2me[c.a];
		c.b=s2me[c.b];
	}
	springs=shp.springs;
	for(auto& s:springs) {
		s.a=s2me[s.a];
		s.b=s2me[s.b];
	}
	tris=shp.tris;
	for(auto& t:tris) {
		t.a=s2me[t.a];
		t.b=s2me[t.b];
		t.c=s2me[t.c];
	}

	//copy cosmetics
	r=shp.r, g=shp.g, b=shp.b;
}

void Shape::clear() {
	particles.clear();
	constraints.clear();
	springs.clear();
}

//uniquely connect all particles w/ constraints
void Shape::constrainAll() {
	constraints.clear();
	for(auto ait=particles.begin(); ait!=particles.end(); ait++) {
		for(auto bit=std::next(ait); bit!=particles.end(); bit++) {
			constraints.push_back(Constraint(*ait, *bit));
		}
	}
}
#endif