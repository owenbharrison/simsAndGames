/*TODO
impl floodfill
* compress shape into multiples
* pass func to determine what to do with new ones
impl polygon to PixelSet
*/

#pragma once
#ifndef PIXELSET_CLASS_H
#define PIXELSET_CLASS_H

struct Mesh {
	int i=0, j=0, w=0, h=0;
	olc::Pixel col;

	Mesh() {}

	Mesh(int i_, int j_, int w_, int h_, olc::Pixel c) : i(i_), j(j_), w(w_), h(h_), col(c) {}
};

typedef unsigned char byte;

#include "aabb.h"

class PixelSet {
	int w, h;

	void copyFrom(const PixelSet& p), clear();

public:
	byte* grid=nullptr;
	bool* colliding=nullptr;

	enum {
		Empty,
		Normal,
		Edge
	};

	vf2d pos, vel, acc;
	float rot=0;
	//this is causing annoying issues...
	vf2d cossin;
	float scale=1;

	std::vector<Mesh> meshes;

	float total_mass=0;
	vf2d center_of_mass;

	PixelSet() : PixelSet(1, 1) {}

	PixelSet(int _w, int _h) : w(_w), h(_h) {
		grid=new byte[w*h];
		colliding=new bool[w*h];
		updateRot();
	}

	//1
	PixelSet(const PixelSet& p) {
		copyFrom(p);
	}

	//2
	~PixelSet() {
		clear();
	}

	//3
	PixelSet& operator=(const PixelSet& p) {
		if(&p==this) return *this;

		clear();

		copyFrom(p);

		return *this;
	}

	//getters
	int getW() const { return w; }
	int getH() const { return h; }

	bool inRangeX(int i) const { return i>=0&&i<w; }
	bool inRangeY(int j) const { return j>=0&&j<h; }

	int ix(int i, int j) const { return i+w*j; }

	const byte& operator()(int i, int j) const { return grid[ix(i, j)]; }
	byte& operator()(int i, int j) { return grid[ix(i, j)]; }

	bool empty() const {
		for(int i=0; i<w*h; i++) {
			if(grid[i]!=Empty) return false;
		}
		return true;
	}

	//helpers?
	AABB getAABB() const {
		AABB box;

		//for each corner point
		for(int p=0; p<4; p++) {
			int i=0, j=0;
			switch(p) {
				case 1: i=w; break;
				case 2: i=w, j=h; break;
				case 3: j=h; break;
			}

			//rotate point and fit in box
			box.fitToEnclose(localToWorld(vf2d(i, j)));
		}
		return box;
	}

	vf2d localToWorld(const vf2d& p) const {
		//order of rotate then scale doesnt matter.
		vf2d scaled=scale*p;
		vf2d rotated(
			cossin.x*scaled.x-cossin.y*scaled.y,
			cossin.y*scaled.x+cossin.x*scaled.y
		);
		//translate
		return pos+rotated;
	}

	//inverse of rotation matrix is its transpose!
	vf2d worldToLocal(const vf2d& p) const {
		//undo transformations...
		vf2d rotated=p-pos;
		vf2d scaled(
			cossin.x*rotated.x+cossin.y*rotated.y,
			cossin.x*rotated.y-cossin.y*rotated.x
		);
		return scaled/scale;
	}

#pragma region DESTRUCTION
	//think of this as a trim whitespace function
	void compress() {
		//uhh
		if(empty()) return;

		//find bounds of solids:
		int i_min=w-1, j_min=h-1;
		int i_max=0, j_max=0;

		//for every solid block
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				if(grid[ix(i, j)]==Empty) continue;

				//update bounds
				if(i<i_min) i_min=i;
				if(j<j_min) j_min=j;
				if(i>i_max) i_max=i;
				if(j>j_max) j_max=j;
			}
		}

		//is compression warranted?
		if(i_min!=0||j_min!=0||i_max!=w-1||j_max!=h-1) {
			//copy to smaller grid
			PixelSet p(1+i_max-i_min, 1+j_max-j_min);
			p.pos=pos;
			p.rot=rot;
			p.cossin=cossin;
			p.scale=scale;
			for(int i=0; i<p.getW(); i++) {
				for(int j=0; j<p.getH(); j++) {
					p(i, j)=grid[ix(i_min+i, j_min+j)];
				}
			}

			//update self
			operator=(p);

			//retranslate based on minimum
			pos=localToWorld(vf2d(i_min, j_min));
		}
	}

	//edge detection for collision routine
	void updateTypes() {
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				auto& t=grid[ix(i, j)];
				if(t==Empty) continue;

				//which neighbors are empty?
				bool left=i==0||grid[ix(i-1, j)]==Empty;
				bool right=i==w-1||grid[ix(i+1, j)]==Empty;
				bool up=j==0||grid[ix(i, j-1)]==Empty;
				bool down=j==h-1||grid[ix(i, j+1)]==Empty;
				//if any neighbor empty, make edge else normal
				t=left||right||up||down?Edge:Normal;
			}
		}
	}

	void updateMass() {
		center_of_mass={0, 0};
		total_mass=0;
		for(int i=0; i<p->getW(); i++) {
			for(int j=0; j<p->getH(); j++) {
				if((*p)(i, j)==PixelSet::Empty) continue;

				float mass=1;
				total_mass+=mass;

				center_of_mass+=mass*vf2d(.5f+i, .5f+j);
			}
		}
		center_of_mass/=total_mass;
	}

	void clearMeshes() {
		meshes.clear();
	}

	void updateMeshes() {
		clearMeshes();

		bool* meshed=new bool[w*h];
		memset(meshed, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont mesh air
				const auto& curr=grid[ix(i, j)];
				if(curr==PixelSet::Empty) continue;

				//dont remesh
				if(meshed[ix(i, j)]) continue;

				//greedy meshing!

				//combine rows
				int ext_w=1;
				for(int i_=1+i; i_<w; i_++, ext_w++) {
					//only mesh similar and dont remesh
					if(grid[ix(i_, j)]!=curr||meshed[ix(i_, j)]) break;
				}

				//combine entire columns
				int ext_h=1;
				for(int j_=1+j; j_<h; j_++, ext_h++) {
					bool able=true;
					for(int i_=0; i_<ext_w; i_++) {
						//only mesh similar and dont remesh
						if(grid[ix(i+i_, j_)]!=curr||meshed[ix(i+i_, j_)]) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int i_=0; i_<ext_w; i_++) {
					for(int j_=0; j_<ext_h; j_++) {
						meshed[ix(i+i_, j+j_)]=true;
					}
				}

				//triangulate
				olc::Pixel col;
				switch(curr) {
					case PixelSet::Normal: col=olc::WHITE; break;
					case PixelSet::Edge: col=olc::BLUE; break;
				}
				meshes.emplace_back(i, j, ext_w, ext_h, col);
			}
		}
		delete[] meshed;
	}

	void slice(const vf2d& a, const vf2d& b) {}
#pragma endregion

#pragma region PHYSICS
	void applyForce(const vf2d& f) {
		acc+=f;
	}

	void updateRot() {
		//precompute trig
		cossin={std::cosf(rot), std::sinf(rot)};
	}

	void update(float dt) {
		//euler explicit?
		vel+=acc*dt;
		pos+=vel*dt;

		//reset forces
		acc={0, 0};

		updateRot();
	}

	//check MY edges against THEIR everything
	void collide(PixelSet& p) {
		//check bounding boxes
		if(!getAABB().overlaps(p.getAABB())) return;

		//for each of MY blocks
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//skip if not edge
				if(grid[ix(i, j)]!=PixelSet::Edge) continue;

				//my block in world space
				vf2d my_world=localToWorld(vf2d(i, j));

				//my block in their space
				vf2d my_in_their=p.worldToLocal(my_world);

				//is it a valid position?
				int pi=std::floor(my_in_their.x), pj=std::floor(my_in_their.y);
				if(!p.inRangeX(pi)||!p.inRangeY(pj)) continue;

				//is there anything there?
				if(p(pi, pj)==Empty) continue;

				//update flags
				p.colliding[p.ix(pi, pj)]=true;
				colliding[ix(i, j)]=true;
			}
		}
	}
#pragma endregion
};

void PixelSet::copyFrom(const PixelSet& p) {
	//reallocate and copy over grid stuff
	w=p.w, h=p.h;
	grid=new byte[w*h];
	colliding=new bool[w*h];
	memcpy(grid, p.grid, sizeof(byte)*w*h);
	//copying of meshed not necessary

	//not sure if i need ALL of these, but whetever
	pos=p.pos;
	vel=p.vel;
	acc=p.acc;

	rot=p.rot;
	cossin=p.cossin;

	scale=p.scale;
}

void PixelSet::clear() {
	delete[] grid;
	delete[] colliding;

	clearMeshes();
}
#endif//PIXELSET_CLASS_H