#pragma once
#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

#include "phys/constraint.h"

struct IndexTriangle {
	int a=0, b=0, c=0;
};

class Shape {
	int num=0;

	void copyFrom(const Shape&), clear();

public:
	Particle* particles=nullptr;
	std::list<Constraint> constraints;
	std::list<IndexTriangle> index_tris;

	olc::Pixel col=olc::WHITE;

	Shape() {}

	Shape(int n) {
		num=n;
		particles=new Particle[n];
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
		return num;
	}

	cmn::AABB3 getAABB() const {
		cmn::AABB3 box;
		for(int i=0; i<num; i++) {
			box.fitToEnclose(particles[i].pos);
		}
		return box;
	}

	void update(float dt) {
		for(auto& c:constraints) {
			c.update();
		}

		for(int i=0; i<num; i++) {
			particles[i].update(dt);
		}
	}

	void keepOut(Shape& s) {

	}

	//SUPER hard coded but whatever
	static Shape makeBox(const cmn::AABB3& a) {
		Shape s(8);
		vf3d v0=a.min, v7=a.max;
		s.particles[0]=Particle(v0);
		s.particles[1]=Particle(vf3d(v7.x, v0.y, v0.z));
		s.particles[2]=Particle(vf3d(v0.x, v7.y, v0.z));
		s.particles[3]=Particle(vf3d(v7.x, v7.y, v0.z));
		s.particles[4]=Particle(vf3d(v0.x, v0.y, v7.z));
		s.particles[5]=Particle(vf3d(v7.x, v0.y, v7.z));
		s.particles[6]=Particle(vf3d(v0.x, v7.y, v7.z));
		s.particles[7]=Particle(v7);

		s.constraints.emplace_back(&s.particles[0], &s.particles[1]);
		s.constraints.emplace_back(&s.particles[1], &s.particles[3]);
		s.constraints.emplace_back(&s.particles[3], &s.particles[2]);
		s.constraints.emplace_back(&s.particles[2], &s.particles[0]);
		s.constraints.emplace_back(&s.particles[0], &s.particles[4]);
		s.constraints.emplace_back(&s.particles[1], &s.particles[5]);
		s.constraints.emplace_back(&s.particles[2], &s.particles[6]);
		s.constraints.emplace_back(&s.particles[3], &s.particles[7]);
		s.constraints.emplace_back(&s.particles[4], &s.particles[5]);
		s.constraints.emplace_back(&s.particles[5], &s.particles[7]);
		s.constraints.emplace_back(&s.particles[7], &s.particles[6]);
		s.constraints.emplace_back(&s.particles[6], &s.particles[4]);

		//add diagonals
		s.constraints.emplace_back(&s.particles[1], &s.particles[4]);
		s.constraints.emplace_back(&s.particles[0], &s.particles[5]);
		s.constraints.emplace_back(&s.particles[1], &s.particles[2]);
		s.constraints.emplace_back(&s.particles[0], &s.particles[3]);
		s.constraints.emplace_back(&s.particles[1], &s.particles[7]);
		s.constraints.emplace_back(&s.particles[3], &s.particles[5]);
		s.constraints.emplace_back(&s.particles[5], &s.particles[6]);
		s.constraints.emplace_back(&s.particles[4], &s.particles[7]);
		s.constraints.emplace_back(&s.particles[0], &s.particles[6]);
		s.constraints.emplace_back(&s.particles[2], &s.particles[4]);
		s.constraints.emplace_back(&s.particles[2], &s.particles[7]);
		s.constraints.emplace_back(&s.particles[3], &s.particles[6]);

		//s.constraints.emplace_back(&s.particles[0], &s.particles[7]);
		//s.constraints.emplace_back(&s.particles[1], &s.particles[6]);
		//s.constraints.emplace_back(&s.particles[2], &s.particles[5]);
		//s.constraints.emplace_back(&s.particles[3], &s.particles[4]);

		//tesselate
		s.index_tris.push_back({0, 1, 4});
		s.index_tris.push_back({1, 5, 4});
		s.index_tris.push_back({0, 2, 1});
		s.index_tris.push_back({2, 3, 1});
		s.index_tris.push_back({1, 3, 7});
		s.index_tris.push_back({1, 7, 5});
		s.index_tris.push_back({5, 7, 6});
		s.index_tris.push_back({5, 6, 4});
		s.index_tris.push_back({0, 6, 2});
		s.index_tris.push_back({0, 4, 6});
		s.index_tris.push_back({2, 7, 3});
		s.index_tris.push_back({2, 6, 7});

		return s;
	}
};

void Shape::copyFrom(const Shape& s) {
	//allocate
	num=s.num;
	particles=new Particle[num];

	//copy over particles
	for(int i=0; i<num; i++) {
		particles[i]=s.particles[i];
	}

	//copy over constraints with pointer arithmetic
	for(const auto& c:s.constraints) {
		constraints.emplace_back(particles+(c.a-s.particles), particles+(c.b-s.particles));
	}

	//copy over index tris
	index_tris=s.index_tris;

	//copy over graphics
	col=s.col;
}

void Shape::clear() {
	delete[] particles;

	constraints.clear();

	index_tris.clear();
}
#endif