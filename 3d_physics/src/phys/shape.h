#pragma once
#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

#include "constraint.h"
#include "spring.h"

#include "index_triangle.h"

struct IndexEdge {
	int a=0, b=0;

	IndexEdge() {}

	IndexEdge(int a_, int b_) {
		a=a_, b=b_;
		if(a>b) std::swap(a, b);
	}

	bool operator==(const IndexEdge& il) const {
		return a==il.a&&b==il.b;
	}
};

struct EdgeHash {
	std::size_t operator()(const IndexEdge& il) const {
		std::hash<int> hasher;
		return hasher(il.a)^(hasher(il.b)<<1);
	}
};

#include <unordered_set>

#include <fstream>
#include <sstream>
#include <exception>

class Shape {
	int num_ptc=0;

	void copyFrom(const Shape&), clear();

public:
	Particle* particles=nullptr;
	std::vector<Constraint> constraints;
	std::vector<Spring> springs;
	std::vector<IndexTriangle> tris;
	std::vector<IndexEdge> edges;

	olc::Pixel fill=olc::WHITE;

	int id=-1;

	Shape() {}

	Shape(int n) {
		num_ptc=n;
		particles=new Particle[n];
	}

	//pretty hard coded, but whatever
	Shape(const cmn::AABB3& a) : Shape(8) {
		//corners of box
		particles[0]=Particle(a.min);
		particles[1]=Particle(vf3d(a.max.x, a.min.y, a.min.z));
		particles[2]=Particle(vf3d(a.min.x, a.max.y, a.min.z));
		particles[3]=Particle(vf3d(a.max.x, a.max.y, a.min.z));
		particles[4]=Particle(vf3d(a.min.x, a.min.y, a.max.z));
		particles[5]=Particle(vf3d(a.max.x, a.min.y, a.max.z));
		particles[6]=Particle(vf3d(a.min.x, a.max.y, a.max.z));
		particles[7]=Particle(a.max);

		initConstraints();

		//tessellate
		tris.push_back({0, 1, 4});
		tris.push_back({1, 5, 4});
		tris.push_back({0, 2, 1});
		tris.push_back({2, 3, 1});
		tris.push_back({1, 3, 7});
		tris.push_back({1, 7, 5});
		tris.push_back({5, 7, 6});
		tris.push_back({5, 6, 4});
		tris.push_back({0, 6, 2});
		tris.push_back({0, 4, 6});
		tris.push_back({2, 7, 3});
		tris.push_back({2, 6, 7});

		initEdges();

		initMass();
	}

	Shape(const cmn::AABB3& a, float res, float dt) {
		//spacing
		vf3d sz=a.max-a.min;
		int w=std::ceil(sz.x/res);
		w=1+std::max(1, w);
		int h=std::ceil(sz.y/res);
		h=1+std::max(1, h);
		int d=std::ceil(sz.z/res);
		d=1+std::max(1, d);
		
		//allocate
		num_ptc=w*h*d;
		particles=new Particle[num_ptc];

		//init particles
		auto ix=[&] (int i, int j, int k) { return i+w*j+w*h*k; };
		for(int i=0; i<w; i++) {
			float t=i/(w-1.f);
			float x=a.min.x+t*sz.x;
			for(int j=0; j<h; j++) {
				float u=j/(h-1.f);
				float y=a.min.y+u*sz.y;
				for(int k=0; k<d; k++) {
					float v=k/(d-1.f);
					float z=a.min.z+v*sz.z;
					particles[ix(i, j, k)]=Particle({x, y, z});
				}
			}
		}

		//tris
		for(int i=1; i<w; i++) {
			for(int j=1; j<h; j++) {
				tris.push_back({ix(i-1, j, 0), ix(i, j-1, 0), ix(i-1, j-1, 0)});
				tris.push_back({ix(i-1, j, 0), ix(i, j, 0), ix(i, j-1, 0)});
				tris.push_back({ix(i, j-1, d-1), ix(i-1, j, d-1), ix(i-1, j-1, d-1)});
				tris.push_back({ix(i, j-1, d-1), ix(i, j, d-1), ix(i-1, j, d-1)});
			}
		}
		for(int j=1; j<h; j++) {
			for(int k=1; k<d; k++) {
				tris.push_back({ix(0, j-1, k), ix(0, j, k-1), ix(0, j-1, k-1)});
				tris.push_back({ix(0, j-1, k), ix(0, j, k), ix(0, j, k-1)});
				tris.push_back({ix(w-1, j, k-1), ix(w-1, j-1, k), ix(w-1, j-1, k-1)});
				tris.push_back({ix(w-1, j, k-1), ix(w-1, j, k), ix(w-1, j-1, k)});
			}
		}
		for(int k=1; k<d; k++) {
			for(int i=1; i<w; i++) {
				tris.push_back({ix(i, 0, k-1), ix(i-1, 0, k), ix(i-1, 0, k-1)});
				tris.push_back({ix(i, 0, k-1), ix(i, 0, k), ix(i-1, 0, k)});
				tris.push_back({ix(i-1, h-1, k), ix(i, h-1, k-1), ix(i-1, h-1, k-1)});
				tris.push_back({ix(i-1, h-1, k), ix(i, h-1, k), ix(i, h-1, k-1)});
			}
		}

		initMass();

		//springs(depend on mass)
		const float sq2=std::sqrt(2.f);
		const float sq3=std::sqrt(3.f);
		for(int i=0; i<w; i++) {
			bool i0=i>0;
			for(int j=0; j<h; j++) {
				bool j0=j>0;
				for(int k=0; k<d; k++) {
					bool k0=k>0;
					//xyz edges
					if(i0) springs.emplace_back(&particles[ix(i-1, j, k)], &particles[ix(i, j, k)], dt);
					if(j0) springs.emplace_back(&particles[ix(i, j-1, k)], &particles[ix(i, j, k)], dt);
					if(k0) springs.emplace_back(&particles[ix(i, j, k-1)], &particles[ix(i, j, k)], dt);

					//face diagonals
					if(i0&&j0) {
						Spring s1(&particles[ix(i-1, j, k)], &particles[ix(i, j-1, k)], dt);
						s1.stiffness*=sq2, s1.damping*=sq2, springs.push_back(s1);
						Spring s2(&particles[ix(i-1, j-1, k)], &particles[ix(i, j, k)], dt);
						s2.stiffness*=sq2, s2.damping*=sq2, springs.push_back(s2);
					}
					if(j0&&k0) {
						Spring s1(&particles[ix(i, j-1, k)], &particles[ix(i, j, k-1)], dt);
						s1.stiffness*=sq2, s1.damping*=sq2, springs.push_back(s1);
						Spring s2(&particles[ix(i, j-1, k-1)], &particles[ix(i, j, k)], dt);
						s2.stiffness*=sq2, s2.damping*=sq2, springs.push_back(s2);
					}
					if(k0&&i0) {
						Spring s1(&particles[ix(i, j, k-1)], &particles[ix(i-1, j, k)], dt);
						s1.stiffness*=sq2, s1.damping*=sq2, springs.push_back(s1);
						Spring s2(&particles[ix(i-1, j, k-1)], &particles[ix(i, j, k)], dt);
						s2.stiffness*=sq2, s2.damping*=sq2, springs.push_back(s2);
					}
					
					//cube diagonals
					if(i0&&j0&&k0) {
						Spring s1(&particles[ix(i, j-1, k-1)], &particles[ix(i-1, j, k)], dt);
						s1.stiffness*=sq3, s1.damping*=sq3, springs.push_back(s1);
						Spring s2(&particles[ix(i-1, j, k-1)], &particles[ix(i, j-1, k)], dt);
						s2.stiffness*=sq3, s2.damping*=sq3, springs.push_back(s2);
						Spring s3(&particles[ix(i-1, j-1, k)], &particles[ix(i, j, k-1)], dt);
						s3.stiffness*=sq3, s3.damping*=sq3, springs.push_back(s3);
						Spring s4(&particles[ix(i-1, j-1, k-1)], &particles[ix(i, j, k)], dt);
						s4.stiffness*=sq3, s4.damping*=sq3, springs.push_back(s4);
					}
				}
			}
		}

		//xyz edges
		for(int i=1; i<w; i++) {
			edges.emplace_back(ix(i-1, 0, 0), ix(i, 0, 0));
			edges.emplace_back(ix(i-1, h-1, 0), ix(i, h-1, 0));
			edges.emplace_back(ix(i-1, 0, d-1), ix(i, 0, d-1));
			edges.emplace_back(ix(i-1, h-1, d-1), ix(i, h-1, d-1));
		}
		for(int j=1; j<h; j++) {
			edges.emplace_back(ix(0, j-1, 0), ix(0, j, 0));
			edges.emplace_back(ix(0, j-1, d-1), ix(0, j, d-1));
			edges.emplace_back(ix(w-1, j-1, 0), ix(w-1, j, 0));
			edges.emplace_back(ix(w-1, j-1, d-1), ix(w-1, j, d-1));
		}
		for(int k=1; k<d; k++) {
			edges.emplace_back(ix(0, 0, k-1), ix(0, 0, k));
			edges.emplace_back(ix(w-1, 0, k-1), ix(w-1, 0, k));
			edges.emplace_back(ix(0, h-1, k-1), ix(0, h-1, k));
			edges.emplace_back(ix(w-1, h-1, k-1), ix(w-1, h-1, k));
		}
		
		//face edges
		for(int i=1; i<w-1; i++) {
			for(int j=1; j<h; j++) {
				edges.emplace_back(ix(i, j-1, 0), ix(i, j, 0));
				edges.emplace_back(ix(i, j-1, d-1), ix(i, j, d-1));
			}
			for(int k=1; k<d; k++) {
				edges.emplace_back(ix(i, 0, k-1), ix(i, 0, k));
				edges.emplace_back(ix(i, h-1, k-1), ix(i, h-1, k));
			}
		}
		for(int j=1; j<h-1; j++) {
			for(int k=1; k<d; k++) {
				edges.emplace_back(ix(0, j, k-1), ix(0, j, k));
				edges.emplace_back(ix(w-1, j, k-1), ix(w-1, j, k));
			}
			for(int i=1; i<w; i++) {
				edges.emplace_back(ix(i-1, j, 0), ix(i, j, 0));
				edges.emplace_back(ix(i-1, j, d-1), ix(i, j, d-1));
			}
		}
		for(int k=1; k<d-1; k++) {
			for(int i=1; i<w; i++) {
				edges.emplace_back(ix(i-1, 0, k), ix(i, 0, k));
				edges.emplace_back(ix(i-1, h-1, k), ix(i, h-1, k));
			}
			for(int j=1; j<h; j++) {
				edges.emplace_back(ix(0, j-1, k), ix(0, j, k));
				edges.emplace_back(ix(w-1, j-1, k), ix(w-1, j, k));
			}
		}
	}

	//1
	Shape(const Shape& s) {
		copyFrom(s);
	}

	//2
	~Shape() {
		clear();
	}

	//3
	Shape& operator=(const Shape& s) {
		if(&s==this) return *this;

		clear();

		copyFrom(s);

		return *this;
	}

	void initConstraints() {
		constraints.clear();

		//connect everything together
		for(int i=0; i<num_ptc; i++) {
			for(int j=i+1; j<num_ptc; j++) {
				constraints.emplace_back(&particles[i], &particles[j]);
			}
		}
	}

	void initEdges() {
		edges.clear();

		//extract edges
		std::unordered_map<IndexEdge, std::vector<const IndexTriangle*>, EdgeHash> edge_tris;
		for(const auto& it:tris) {
			edge_tris[IndexEdge(it.a, it.b)].push_back(&it);
			edge_tris[IndexEdge(it.b, it.c)].push_back(&it);
			edge_tris[IndexEdge(it.c, it.a)].push_back(&it);
		}

		//collect non-coplanar edges
		const float thresh=std::cos(Pi/180);//1 deg
		for(const auto& et:edge_tris) {
			if(et.second.size()==1) {//include boundary edges
				edges.push_back(et.first);
			} else if(et.second.size()==2) {
				auto& t1=et.second[0], & t2=et.second[1];
				vf3d ab1=particles[t1->b].pos-particles[t1->a].pos;
				vf3d ac1=particles[t1->c].pos-particles[t1->a].pos;
				vf3d n1=ab1.cross(ac1).norm();
				vf3d ab2=particles[t2->b].pos-particles[t2->a].pos;
				vf3d ac2=particles[t2->c].pos-particles[t2->a].pos;
				vf3d n2=ab2.cross(ac2).norm();
				if(n1.dot(n2)<thresh) edges.push_back(et.first);
			}
		}
	}

	//evenly distribute mass among particles
	void initMass() {
		//calculate volume using signed tetrahedrons
		float volume=0;
		for(const auto& it:tris) {
			vf3d& pa=particles[it.a].pos;
			vf3d& pb=particles[it.b].pos;
			vf3d& pc=particles[it.c].pos;
			volume+=pa.dot(pb.cross(pc));
		}
		volume=std::abs(volume)/6;

		//distribute mass
		float ind_mass=volume/num_ptc;
		for(int i=0; i<num_ptc; i++) {
			particles[i].mass=ind_mass;
		}
	}

	int getNum() const {
		return num_ptc;
	}

	cmn::AABB3 getAABB() const {
		cmn::AABB3 box;
		for(int i=0; i<num_ptc; i++) {
			box.fitToEnclose(particles[i].pos);
		}
		return box;
	}

	float intersectRay(const vf3d& orig, const vf3d& dir) const {
		if(!rayIntersectBox(orig, dir, getAABB())) return -1;

		//sort by closest
		float record=-1;
		for(const auto& it:tris) {
			cmn::Triangle t{
				particles[it.a].pos,
				particles[it.b].pos,
				particles[it.c].pos
			};
			float dist=segIntersectTri(orig, orig+dir, t);
			if(dist>0) {
				if(record<0||dist<record) {
					record=dist;
				}
			}
		}

		return record;
	}

	void update(float dt) {
		//update "joints"
		for(auto& c:constraints) {
			c.update();
		}

		for(auto& s:springs) {
			s.update(dt);
		}

		for(int i=0; i<num_ptc; i++) {
			particles[i].update(dt);
		}
	}

	void keepIn(const cmn::AABB3& box) {
		for(int i=0; i<num_ptc; i++) {
			particles[i].keepIn(box);
		}
	}
};

void Shape::copyFrom(const Shape& shp) {
	//allocate
	num_ptc=shp.num_ptc;
	particles=new Particle[num_ptc];

	//copy over particles
	for(int i=0; i<num_ptc; i++) {
		particles[i]=shp.particles[i];
	}

	//copy over constraints with pointer arithmetic
	for(const auto& sc:shp.constraints) {
		Constraint c;
		c.a=particles+(sc.a-shp.particles);
		c.b=particles+(sc.b-shp.particles);
		c.rest_len=sc.rest_len;
		constraints.push_back(c);
	}

	//copy over springs with pointer arithmetic
	for(const auto& ss:shp.springs) {
		Spring s;
		s.a=particles+(ss.a-shp.particles);
		s.b=particles+(ss.b-shp.particles);
		s.rest_len=ss.rest_len;
		s.stiffness=ss.stiffness;
		s.damping=ss.damping;
		springs.push_back(s);
	}

	//copy over	tris & edges
	tris=shp.tris;

	//copy over edges
	edges=shp.edges;

	//copy over graphics
	fill=shp.fill;
	id=shp.id;
}

void Shape::clear() {
	delete[] particles;

	constraints.clear();

	springs.clear();

	tris.clear();

	edges.clear();
}
#endif