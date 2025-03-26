#pragma once
#ifndef SOLVER_CLASS_H
#define SOLVER_CLASS_H

#include "aabb.h"

#include "particle.h"

#include <list>
#include <vector>

//clever default param placement:
//random()=0-1
//random(a)=0-a
//random(a, b)=a-b
float random(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=rand()/rand_max;
	return a+t*(b-a);
}

vf2d polar(float rad, float angle) {
	return {rad*std::cosf(angle), rad*std::sinf(angle)};
}

vf2d reflect(const vf2d& in, const vf2d& norm) {
	return in-2*norm.dot(in)*norm;
}

class Solver {
	float cell_size=0;
	int num_cell_x=1;
	int num_cell_y=1;
	std::list<int>* cells=nullptr;

	int cellIX(int i, int j) const {
		return i+num_cell_x*j;
	}

	void updateCellNum();
	void reallocateCells();
	void fillCells();

public:
	std::vector<Particle> particles;

	AABB bounds;

	Solver() {
		reallocateCells();
	}

	//1
	Solver(const Solver& s)=delete;

	//2
	~Solver() {
		delete[] cells;
	}

	//3
	Solver& operator=(const Solver& s)=delete;

	//if NOT overlapping others, update spacial hash
	Particle* addParticle(const Particle& o) {
		for(const auto& p:particles) {
			if((o.pos-p.pos).mag()<o.rad+p.rad) return nullptr;
		}

		particles.push_back(o);
		if(2*o.rad>cell_size) {
			cell_size=2*o.rad;
			updateCellNum();
			reallocateCells();
		}
		return &particles.back();
	}

	//if found, update spacial hash
	void removeParticle(const Particle& o) {
		for(auto it=particles.begin(); it!=particles.end(); it++) {
			if(&*it==&o) {
				float rad=it->rad;
				particles.erase(it);
				
				float record=-1;
				for(const auto& p:particles) {
					if(p.rad>rad) return;
					if(p.rad>record) record=p.rad;
				}
				
				if(record<0) {
					//that was last particle
					cell_size=0;
					num_cell_x=1;
					num_cell_x=1;
				} else {
					//that was biggest particle
					cell_size=2*record;
					updateCellNum();
				}
				reallocateCells();
				
				return;
			}
		}
	}

	float getCellSize() const {
		return cell_size;
	}

	int getNumCellX() const {
		return num_cell_x;
	}

	int getNumCellY() const {
		return num_cell_y;
	}

	bool cellInRangeX(int i) const {
		return i>=0&&i<num_cell_x;
	}

	bool cellInRangeY(int j) const {
		return j>=0&&j<num_cell_y;
	}

	const std::list<int>& getCell(int i, int j) const {
		return cells[cellIX(i, j)];
	}

	void collideParticles(int k1, int k2) {
		auto& a=particles[k1];
		auto& b=particles[k2];

		//are the circles overlapping?
		vf2d sub=a.pos-b.pos;
		float dist_sq=sub.mag2();
		float rad=a.rad+b.rad;
		float rad_sq=rad*rad;
		if(dist_sq<rad_sq) {
			float dist=std::sqrtf(dist_sq);
			float delta=dist-rad;

			//dont divide by 0
			vf2d norm;
			if(dist<1e-6f) norm=polar(1, random(2*Pi));
			else norm=sub/dist;

			float inv_sum=a.inv_mass+b.inv_mass;
			vf2d diff=delta*norm/inv_sum;
			a.pos-=a.inv_mass*diff;
			b.pos+=b.inv_mass*diff;
		}
	}

	int collideCells(int i1, int j1, int i2, int j2) {
		int checks=0;
		
		const auto& c1=cells[cellIX(i1, j1)];
		const auto& c2=cells[cellIX(i2, j2)];
		for(const auto& k1:c1) {
			for(const auto& k2:c2) {
				//dont check self
				if(k2==k1) continue;

				checks++;

				collideParticles(k1, k2);
			}
		}

		return checks;
	}

	int solveCollisions() {
		fillCells();

		int checks=0;

		const int di[5]{1, 1, 0, 0, -1};
		const int dj[5]{0, 1, 0, 1, 1};

		//for each cell
		for(int i=0; i<num_cell_x; i++) {
			for(int j=0; j<num_cell_y; j++) {
				for(int k=0; k<5; k++) {
					//check adjacent including self
					int oi=i+di[k], oj=j+dj[k];
					if(!cellInRangeX(oi)||!cellInRangeY(oj)) continue;
						
					checks+=collideCells(i, j, oi, oj);
				}
			}
		}

		return checks;
	}

	void applyGravity(float g) {
		for(auto& p:particles) {
			p.applyForce(vf2d(0, p.mass*g));
		}
	}

	void updateKinematics(float dt) {
		for(auto& p:particles) {
			p.update(dt);

			p.keepIn(bounds);
		}
	}
};

void Solver::updateCellNum() {
	num_cell_x=1+(bounds.max.x-bounds.min.x)/cell_size;
	num_cell_y=1+(bounds.max.y-bounds.min.y)/cell_size;
}

void Solver::reallocateCells() {
	delete[] cells;
	cells=new std::list<int>[num_cell_x*num_cell_y];
}

void Solver::fillCells() {
	//first clear it
	for(int i=0; i<num_cell_x; i++) {
		for(int j=0; j<num_cell_y; j++) {
			cells[cellIX(i, j)].clear();
		}
	}

	//add all particles
	for(int k=0; k<particles.size(); k++) {
		const auto& p=particles[k];

		int i=p.pos.x/cell_size;
		int j=p.pos.y/cell_size;
		if(!cellInRangeX(i)||!cellInRangeY(j)) continue;

		cells[cellIX(i, j)].emplace_back(k);
	}
}
#endif