#pragma once
#ifndef SOLVER_STRUCT_H
#define SOLVER_STRUCT_H

#include "particle.h"

#include <list>

#include <vector>

//for memset
#include <string>

//for swap
#include <algorithm>

struct Constraint {
	int a=0, b=0;
	float len=0;
};

//fisher-yates shuffle
template<typename T>
void shuffle(std::vector<T>& vec) {
	for(int i=vec.size()-1; i>=1; i--) {
		int j=std::rand()%(i+1);
		std::swap(vec[i], vec[j]);
	}
}

struct Solver {
	int max_particles=0;

	cmn::vf2d min, max;

	int num_particles=0;

	float cell_sz=0;
	int num_x=0, num_y=0;

	int* grid_heads=nullptr;

	int* particle_next=nullptr;

	void fillCells();

	void collideCells(int, int, int, int);

public:
	Particle* particles=nullptr;
	std::list<Constraint> constraints;

	Solver(int m, const cmn::vf2d& n, const cmn::vf2d& x) {
		max_particles=m;
		particles=new Particle[max_particles];

		min=n, max=x;
		updateSizing();

		particle_next=new int[max_particles];
	}

	//ro3: 1
	Solver(const Solver& s)=delete;

	//ro3: 2
	~Solver() {
		delete[] particles;
		delete[] grid_heads;
		delete[] particle_next;
	}

	//ro3: 3
	Solver& operator=(const Solver& s)=delete;

	int getMaxParticles() const { return max_particles; }

	int getNumParticles() const { return num_particles; }

	float getCellSize() const { return cell_sz; }

	int getNumX() const { return num_x; }
	int getNumY() const { return num_y; }

	bool inRangeX(int i) const { return i>=0&&i<num_x; }
	bool inRangeY(int j) const { return j>=0&&j<num_y; }

	int ix(int i, int j) const { return i+num_x*j; }

	//index of insertion
	int addParticle(const Particle& p) {
		//not enough space?
		if(num_particles==max_particles) return -1;

		float r=p.getRadius();

		//overlapping wall?
		if(p.pos.x-r<min.x||
			p.pos.y-r<min.y||
			p.pos.x+r>max.x||
			p.pos.y+r>max.y
			) return -1;

		//overlapping particle?
		for(int i=0; i<num_particles; i++) {
			cmn::vf2d sub=particles[i].pos-p.pos;
			float t_rad=particles[i].getRadius()+r;
			if(sub.mag_sq()<t_rad*t_rad) return -1;
		}

		//add at end
		particles[num_particles]=p;
		num_particles++;

		return num_particles-1;
	}

	void removeParticle(int id) {
		//outside range
		if(id<0||id>=num_particles) return;

		//decrease count & swap with last
		int last=--num_particles;
		particles[id]=particles[last];

		//check constraints
		for(auto it=constraints.begin(); it!=constraints.end();) {
			auto& c=*it;

			//remove connected
			if(c.a==id||c.b==id) {
				it=constraints.erase(it);
				continue;
			}

			//fix moved
			if(c.a==last) c.a=id;
			if(c.b==last) c.b=id;

			it++;
		}
	}

	void clear() {
		num_particles=0;
		constraints.clear();
	}

	void addSolidRect(const cmn::vf2d& tl, float rad, float m, int w, int h) {
		//too small
		if(w<1||h<1) return;

		//flattened index helper
		auto gix=[&] (int i, int j) { return i+w*j; };

		//store new particles
		int* grid=new int[w*h];

		//determine spacing
		float step=2*rad+m;
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				float x=tl.x+step*i;
				float y=tl.y+step*j;
				grid[gix(i, j)]=addParticle(Particle({x, y}, rad));
			}
		}

		//connect grid on adjacent & diagonals
		std::vector<Constraint> new_cst;
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				int curr=grid[gix(i, j)];
				int rgt=i<w-1?grid[gix(i+1, j)]:-1;
				int btm=j<h-1?grid[gix(i, j+1)]:-1;
				int btm_rgt=(i<w-1)&&(j<h-1)?grid[gix(i+1, j+1)]:-1;
				new_cst.push_back({curr, rgt});
				new_cst.push_back({curr, btm});
				new_cst.push_back({curr, btm_rgt});
				new_cst.push_back({rgt, btm});
			}
		}

		//free refs
		delete[] grid;

		//randomize, validate, & init len
		shuffle(new_cst);
		for(const auto& c:new_cst) {
			if(c.a==-1||c.b==-1) continue;

			cmn::vf2d sub=particles[c.a].pos-particles[c.b].pos;
			constraints.push_back({c.a, c.b, sub.mag()});
		}
	}

	void addEmptyRect(cmn::vf2d tl, float rad, float m, int w, int h) {
		//too small
		if(w<2||h<2) return;

		//store new particles
		const int num_loop=2*w+2*h-4;
		int* loop=new int[num_loop];

		//add in CW loop
		int k=0;
		float step=2*rad+m;
		for(int i=0; i<w-1; i++, k++) {
			loop[k]=addParticle(Particle(tl, rad));
			tl.x+=step;
		}
		for(int j=0; j<h-1; j++, k++) {
			loop[k]=addParticle(Particle(tl, rad));
			tl.y+=step;
		}
		for(int i=0; i<w-1; i++, k++) {
			loop[k]=addParticle(Particle(tl, rad));
			tl.x-=step;
		}
		for(int j=0; j<h-1; j++, k++) {
			loop[k]=addParticle(Particle(tl, rad));
			tl.y-=step;
		}

		//connect everything
		std::vector<Constraint> new_cst;
		for(int i=0; i<num_loop; i++) {
			for(int j=1+i; j<num_loop; j++) {
				new_cst.push_back({loop[i], loop[j]});
			}
		}

		//free refs
		delete[] loop;
	
		//randomize, validate, & init len
		shuffle(new_cst);
		for(const auto& c:new_cst) {
			if(c.a==-1||c.b==-1) continue;

			cmn::vf2d sub=particles[c.a].pos-particles[c.b].pos;
			constraints.push_back({c.a, c.b, sub.mag()});
		}
	}

	void updateConsraints() {
		for(const auto& c:constraints) {
			auto& a=particles[c.a].pos;
			auto& b=particles[c.b].pos;

			//separating axis
			cmn::vf2d ab=b-a;
			float mag=ab.mag();

			//safe norm
			cmn::vf2d norm=mag==0?cmn::vf2d(1, 0):ab/mag;

			//push apart
			float diff=(mag-c.len)/2;
			a+=diff*norm;
			b-=diff*norm;
		}
	}

	void updateSizing() {
		//find largest particle
		float max_rad=-1;
		for(int i=0; i<num_particles; i++) {
			const auto& r=particles[i].getRadius();
			if(r>max_rad) max_rad=r;
		}

		//determine sizing
		int old_num_x=num_x;
		int old_num_y=num_y;
		if(max_rad<0) {
			cell_sz=0;
			num_x=1;
			num_y=1;
		} else {
			cell_sz=2*max_rad;
			num_x=1+(max.x-min.x)/cell_sz;
			num_y=1+(max.y-min.y)/cell_sz;
		}

		//skip if same
		if(num_x==old_num_x&&num_y==old_num_y) return;

		//free & reallocate
		delete[] grid_heads;
		grid_heads=new int[num_x*num_y];
	}

	void solveCollisions() {
		fillCells();

		//check self & half of neighbors to avoid redundancy
		const int di[5]{0, 1, -1, 0, 1};
		const int dj[5]{0, 0, 1, 1, 1};

		//for each cell
		for(int i=0; i<num_x; i++) {
			for(int j=0; j<num_y; j++) {
				for(int d=0; d<5; d++) {
					//skip if out of range
					int ci=i+di[d], cj=j+dj[d];
					if(!inRangeX(ci)||!inRangeY(cj)) continue;

					collideCells(i, j, ci, cj);
				}
			}
		}
	}

	void accelerate(const cmn::vf2d& a) {
		for(int i=0; i<num_particles; i++) {
			auto& p=particles[i];
			p.applyForce(p.getMass()*a);
		}
	}

	void integrateParticles(float dt) {
		for(int i=0; i<num_particles; i++) {
			auto& p=particles[i];

			p.update(dt);

			p.keepInsideRegion(min, max);
		}
	}
};

void Solver::fillCells() {
	//reset grid heads
	std::memset(grid_heads, -1, sizeof(int)*num_x*num_y);

	//insert particles into grid
	for(int i=0; i<num_particles; i++) {
		auto& p=particles[i];

		//skip if out of bounds
		int xi=(p.pos.x-min.x)/cell_sz;
		int yi=(p.pos.y-min.y)/cell_sz;
		if(!inRangeX(xi)||!inRangeY(yi)) continue;

		//add each particle to front of list
		int ci=ix(xi, yi);
		particle_next[i]=grid_heads[ci];
		grid_heads[ci]=i;
	}
}

void Solver::collideCells(int i1, int j1, int i2, int j2) {
	//nested linked list traversal
	int s1=grid_heads[ix(i1, j1)], s2=grid_heads[ix(i2, j2)];
	for(int p1=s1; p1!=-1; p1=particle_next[p1]) {
		for(int p2=s2; p2!=-1; p2=particle_next[p2]) {
			//dont check self
			if(p2==p1) continue;

			Particle::checkCollide(particles[p1], particles[p2]);
		}
	}
}
#endif