#pragma once
#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

#include "phys/constraint.h"

struct IndexTriangle {
	int a=0, b=0, c=0;
};

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

class Shape {
	int num_ptc=0;

	void initConstraints(), initEdges();
	void initMass();

	void copyFrom(const Shape&), clear();

public:
	Particle* particles=nullptr;
	std::list<Constraint> constraints;
	std::list<IndexTriangle> tris;
	std::list<IndexEdge> edges;

	olc::Pixel col=olc::WHITE;

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

		//tesselate
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

	void update(float dt) {
		for(auto& c:constraints) {
			c.update();
		}

		for(int i=0; i<num_ptc; i++) {
			particles[i].update(dt);
		}
	}
};

void Shape::initConstraints() {
	constraints.clear();

	//connect everything together
	for(int i=0; i<num_ptc; i++) {
		for(int j=i+1; j<num_ptc; j++) {
			constraints.emplace_back(&particles[i], &particles[j]);
		}
	}
}

void Shape::initEdges() {
	edges.clear();

	//extract edges
	std::unordered_map<IndexEdge, std::vector<const IndexTriangle*>, EdgeHash> edge_tris;
	for(const auto& it:tris) {
		edge_tris[IndexEdge(it.a, it.b)].push_back(&it);
		edge_tris[IndexEdge(it.b, it.c)].push_back(&it);
		edge_tris[IndexEdge(it.c, it.a)].push_back(&it);
	}

	//collect non-coplanar edges
	const float thresh=std::cosf(Pi/180);//1 deg
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
void Shape::initMass() {
	//calculate volume using signed tetrahedrons
	float volume=0;
	for(const auto& it:tris) {
		vf3d& pa=particles[it.a].pos;
		vf3d& pb=particles[it.b].pos;
		vf3d& pc=particles[it.c].pos;
		volume+=pa.dot(pb.cross(pc));
	}
	volume=std::fabsf(volume)/6;
	float ind_mass=volume/num_ptc;
	for(int i=0; i<num_ptc; i++) {
		particles[i].mass=ind_mass;
	}
}

void Shape::copyFrom(const Shape& s) {
	//allocate
	num_ptc=s.num_ptc;
	particles=new Particle[num_ptc];

	//copy over particles
	for(int i=0; i<num_ptc; i++) {
		particles[i]=s.particles[i];
	}

	//copy over constraints with pointer arithmetic
	for(const auto& c:s.constraints) {
		constraints.emplace_back(particles+(c.a-s.particles), particles+(c.b-s.particles));
	}

	//copy over	tris & edges
	tris=s.tris;

	//copy over edges
	edges=s.edges;

	//copy over graphics
	col=s.col;
}

void Shape::clear() {
	delete[] particles;

	constraints.clear();

	tris.clear();
}
#endif