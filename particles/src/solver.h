#pragma once
#ifndef SOLVER_CLASS_H
#define SOLVER_CLASS_H

#include "common/aabb.h"
#include "common/utils.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

#include "particle.h"

#include <string>

class Solver {
	float cell_size=0;
	int num_cell_x=1;
	int num_cell_y=1;

	cmn::AABB bounds;

	int max_particles=0;
	int num_particles=0;

	void updateCellNum();
	void reallocateCells();

public:
	Particle* particles=nullptr;

	int* grid_heads=nullptr;
	int* particle_next=nullptr;

	Solver() {}

	Solver(int m, cmn::AABB b) {
		max_particles=m;
		particles=new Particle[max_particles];
		
		bounds=b;

		reallocateCells();

		particle_next=new int[max_particles];
	}

	//ro3 1
	Solver(const Solver& s)=delete;

	//ro3 2
	~Solver() {
		delete[] particles;
		delete[] grid_heads;
		delete[] particle_next;
	}

	//ro3 3
	Solver& operator=(const Solver& s)=delete;

	float getCellSize() const { return cell_size; }

	bool cellInRangeX(int i) const { return i>=0&&i<num_cell_x; }
	bool cellInRangeY(int j) const { return j>=0&&j<num_cell_y; }

	int getNumCellX() const { return num_cell_x; }
	int getNumCellY() const { return num_cell_y; }

	int cellIX(int i, int j) const { return i+num_cell_x*j; }

	cmn::AABB getBounds() const { return bounds; }

	int getNumParticles() const { return num_particles; }

	void fillCells() {
		//reset grid heads
		memset(grid_heads, -1, sizeof(int)*num_cell_x*num_cell_y);

		//insert particles into grid
		for(int i=0; i<num_particles; ++i) {
			auto& p=particles[i];

			//skip if out of bounds
			int xi=(p.pos.x-bounds.min.x)/cell_size;
			int yi=(p.pos.y-bounds.min.y)/cell_size;
			if(!cellInRangeX(xi)||!cellInRangeY(yi)) continue;

			//add each particle to front of list
			int ci=cellIX(xi, yi);
			particle_next[i]=grid_heads[ci];
			grid_heads[ci]=i;
		}
	}

	void addParticle(const Particle& o) {
		//return if too many
		if(num_particles==max_particles) return;

		//return if touching outside
		cmn::AABB box=bounds;
		box.min+=o.rad, box.max-=o.rad;
		if(!box.contains(o.pos)) return;

		//return if overlapping
		for(int i=0; i<num_particles; i++) {
			const auto& p=particles[i];
			if((o.pos-p.pos).mag()<o.rad+p.rad) return;
		}

		//add at end
		particles[num_particles]=o;
		num_particles++;

		//if new largest,
		if(2*o.rad>cell_size) {
			//update spacing
			cell_size=2*o.rad;
			updateCellNum();
			reallocateCells();
		}
	}

	void removeParticle(int id) {
		if(id<0||id>=num_particles) return;

		float rad=particles[id].rad;

		//decrease count & shift after back
		num_particles--;
		for(int i=id; i<num_particles; i++) {
			particles[i]=particles[1+i];
		}
		
		float record=-1;
		for(int i=0; i<num_particles; i++) {
			const auto& p=particles[i];
			//there is a bigger particle
			if(p.rad>rad) return;
			if(p.rad>record) record=p.rad;
		}
				
		if(record<0) {
			//that was last particle
			cell_size=0;
			num_cell_x=1;
			num_cell_y=1;
		} else {
			//that was biggest particle
			cell_size=2*record;
			updateCellNum();
		}
		reallocateCells();
				
		return;
	}

	int collideCells(int i1, int j1, int i2, int j2) {
		int checks=0;
		
		//nested linked list traversal
		for(int c1=grid_heads[cellIX(i1, j1)]; c1!=-1; c1=particle_next[c1]) {
			for(int c2=grid_heads[cellIX(i2, j2)]; c2!=-1; c2=particle_next[c2]) {
				//dont check self
				if(c2==c1) continue;

				Particle::checkCollide(particles[c1], particles[c2]);
				
				checks++;
			}
		}

		return checks;
	}

	int solveCollisions() {
		fillCells();

		int checks=0;

		//check self & half of neighbors to avoid redundancy
		const int di[5]{0, 1, -1, 0, 1};
		const int dj[5]{0, 0, 1, 1, 1};

		//for each cell
		for(int i=0; i<num_cell_x; i++) {
			for(int j=0; j<num_cell_y; j++) {
				for(int d=0; d<5; d++) {
					//skip if out of range
					int ci=i+di[d], cj=j+dj[d];
					if(!cellInRangeX(ci)||!cellInRangeY(cj)) continue;
						
					checks+=collideCells(i, j, ci, cj);
				}
			}
		}

		return checks;
	}

	void accelerate(const vf2d& a) {
		for(int i=0; i<num_particles; i++) {
			auto& p=particles[i];
			p.applyForce(p.mass*a);
		}
	}

	void updateKinematics(float dt) {
		for(int i=0; i<num_particles; i++) {
			auto& p=particles[i];

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
	delete[] grid_heads;
	grid_heads=new int[num_cell_x*num_cell_y];
}
#endif