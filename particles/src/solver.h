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
	float cell_size=1;
	int num_cell_x=1;
	int num_cell_y=1;

	void updateSpacialHash();
	void fillSpacialHash();

public:
	std::list<int>* cells=nullptr;
	std::vector<Particle> particles;

	AABB bounds;

	vf2d gravity;

	Solver() {
		cells=new std::list<int>[num_cell_x*num_cell_y];
	}

	void addParticle(const Particle& o) {
		bool overlap=false;
		for(const auto& p:particles) {
			if((o.pos-p.pos).mag()<o.rad+p.rad) {
				overlap=true;
				break;
			}
		}
		//add only if NOT overlapping any others
		if(!overlap) {
			particles.push_back(o);

			updateSpacialHash();
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

	int hashIX(int i, int j) const {
		return i+num_cell_x*j;
	}

	bool hashInRangeX(int i) const {
		return i>=0&&i<num_cell_x;
	}

	bool hashInRangeY(int j) const {
		return j>=0&&j<num_cell_y;
	}

	void solveCollisions() {
		fillSpacialHash();
		
		//for each cell
		for(int ci=0; ci<num_cell_x; ci++) {
			for(int cj=0; cj<num_cell_y; cj++) {
				//check adjacent including self
				for(int di=-1; di<=1; di++) {
					for(int dj=-1; dj<=1; dj++) {
						int ai=ci+di, aj=cj+dj;
						if(!hashInRangeX(ai)||!hashInRangeY(aj)) continue;

						//get cell lists
						const auto& curr=cells[hashIX(ci, cj)];
						const auto& adj=cells[hashIX(ai, aj)];
						//check each against eachother
						for(const auto& c:curr) {
							auto& pa=particles[c];
							for(const auto& a:adj) {
								auto& pb=particles[a];
								//dont check self
								if(a==c) continue;

								//are the circles overlapping?
								vf2d sub=pa.pos-pb.pos;
								float dist_sq=sub.mag2();
								float rad=pa.rad+pb.rad;
								float rad_sq=rad*rad;
								if(dist_sq<rad_sq) {
									float dist=std::sqrtf(dist_sq);
									float delta=dist-rad;

									//dont divide by 0
									vf2d norm;
									if(dist<1e-6f) norm=polar(1, random(2*Pi));
									else norm=sub/dist;

									//vf2d a_vel=pa.pos-pa.oldpos;
									//vf2d b_vel=pb.pos-pb.oldpos;

									vf2d diff=.5f*delta*norm;
									pa.pos-=diff;
									pb.pos+=diff;

									//reflect velocity about collision normal
									//ait->oldpos=ait->pos-reflect(a_vel, norm);
									//bit->oldpos=bit->pos-reflect(b_vel, norm);
								}
							}
						}
					}
				}
			}
		}
	}

	void updateKinematics(float dt) {
		for(auto& p:particles) {
			p.applyForce(gravity*p.mass);

			p.update(dt);

			p.keepIn(bounds);
		}
	}
};

//update sizing and reallocate
void Solver::updateSpacialHash() {
	float max_rad=1;
	for(const auto& p:particles) {
		if(p.rad>max_rad) max_rad=p.rad;
	}
	cell_size=2*max_rad;
	num_cell_x=1+(bounds.max.x-bounds.min.x)/cell_size;
	num_cell_y=1+(bounds.max.y-bounds.min.y)/cell_size;

	delete[] cells;
	cells=new std::list<int>[num_cell_x*num_cell_y];
}

void Solver::fillSpacialHash() {
	//first clear it
	for(int i=0; i<num_cell_x; i++) {
		for(int j=0; j<num_cell_y; j++) {
			cells[hashIX(i, j)].clear();
		}
	}

	//add all particles
	for(int k=0; k<particles.size(); k++) {
		const auto& p=particles[k];

		int i=p.pos.x/cell_size;
		int j=p.pos.y/cell_size;
		if(!hashInRangeX(i)||!hashInRangeY(j)) continue;

		cells[hashIX(i, j)].emplace_back(k);
	}
}
#endif