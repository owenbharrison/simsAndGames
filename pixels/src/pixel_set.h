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

//clever default param placement:
//random()=0-1
//random(a)=0-a
//random(a, b)=a-b
float random(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=rand()/rand_max;
	return a+t*(b-a);
}

constexpr float Pi=3.1415927f;

//thanks wikipedia + pattern recognition
//NOTE: this returns t and u, NOT the point.
vf2d lineLineIntersection(
	const vf2d& a, const vf2d& b,
	const vf2d& c, const vf2d& d) {
	vf2d ab=a-b, ac=a-c, cd=c-d;
	return vf2d(
		ac.cross(cd),
		ac.cross(ab)
	)/ab.cross(cd);
}

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
	vf2d cossin;
	float scale=1;

	std::vector<Mesh> meshes;

	float total_mass=0;
	vf2d center_of_mass;

#pragma region CONSTRUCTION
	PixelSet() : PixelSet(1, 1) {}

	PixelSet(int _w, int _h) : w(_w), h(_h) {
		grid=new byte[w*h];
		memset(grid, false, sizeof(byte)*w*h);

		colliding=new bool[w*h];
		memset(colliding, false, sizeof(bool)*w*h);

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

	//could make this faster with scanline rasterization...
	static PixelSet fromOutline(const std::vector<vf2d>& outline) {
		//get bounds of outline
		AABB box;
		for(const auto& o:outline) box.fitToEnclose(o);

		//determine spacing
		vf2d size=box.max-box.min;
		const float scale=5;
		int w=1+size.x/scale, h=1+size.y/scale;

		//for every point
		PixelSet p(w, h);
		p.pos=box.min;
		p.scale=scale;
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//is it inside the outline
				vf2d pos=p.localToWorld(vf2d(i, j));

				//deterministic, but less artifacting
				float angle=random(2*Pi);
				vf2d dir(std::cosf(angle), std::sinf(angle));

				//polygon raycast algorithm
				int num=0;
				for(int k=0; k<outline.size(); k++) {
					const auto& a=outline[k];
					const auto& b=outline[(k+1)%outline.size()];
					vf2d tu=lineLineIntersection(a, b, pos, pos+dir);
					if(tu.x>0&&tu.x<1&&tu.y>0) num++;
				}

				//odd? inside!
				p(i, j)=num%2;
			}
		}

		//should these be encapsulated into a function?
		p.updateTypes();
		p.updateMass();
		p.updateMeshes();

		return p;
	}
#pragma endregion

	//getters
	int getW() const { return w; }
	int getH() const { return h; }

	bool inRangeX(int i) const { return i>=0&&i<w; }
	bool inRangeY(int j) const { return j>=0&&j<h; }

	int ix(int i, int j) const { return i+w*j; }
	void inv_ix(int k, int& i, int& j) const { i=k%w, j=k/w; }

	const byte& operator()(int i, int j) const { return grid[ix(i, j)]; }
	byte& operator()(int i, int j) { return grid[ix(i, j)]; }

	//helpers?
	bool empty() const {
		for(int i=0; i<w*h; i++) {
			if(grid[i]!=Empty) return false;
		}
		return true;
	}

	[[nodiscard]] AABB getAABB() const {
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

	//center of mass using moments
	void updateMass() {
		center_of_mass={0, 0};
		total_mass=0;
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				if(grid[ix(i, j)]==PixelSet::Empty) continue;

				float mass=1;
				total_mass+=mass;

				center_of_mass+=mass*vf2d(.5f+i, .5f+j);
			}
		}
		center_of_mass/=total_mass;
	}

	//should mesh stuff be in a graphics region?
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

	//https://en.wikipedia.org/wiki/Cohen-Sutherland_algorithm
	[[nodiscard]] bool clip(int& x1, int& y1, int& x2, int& y2) {
		static constexpr int INSIDE=0b0000,
			LEFT=0b0001,
			RIGHT=0b0010,
			BOTTOM=0b0100,
			TOP=0b1000;
		auto getCode=[this] (int x, int y) {
			int code=INSIDE;
			if(x<0) code|=LEFT;
			else if(x>w) code|=RIGHT;
			if(y<0) code|=BOTTOM;
			else if(y>h) code|=TOP;
			return code;
		};

		int c1=getCode(x1, y1),
			c2=getCode(x2, y2);

		//iteratively clip
		while(true) {
			//entirely inside
			if(!(c1|c2)) return true;
			//entirely outside
			else if(c1&c2) return false;
			else {
				int c3=c2>c1?c2:c1;
				int nx=0, ny=0;
				//interpolate to find new intersections
				if(c3&TOP) nx=x1+(x2-x1)*(h-y1)/(y2-y1), ny=h;
				else if(c3&BOTTOM) nx=x1+(x2-x1)*(0-y1)/(y2-y1), ny=0;
				else if(c3&RIGHT) nx=w, ny=y1+(y2-y1)*(w-x1)/(x2-x1);
				else if(c3&LEFT) nx=0, ny=y1+(y2-y1)*(0-x1)/(x2-x1);
				if(c3==c1) x1=nx, y1=ny, c1=getCode(x1, y1);
				else x2=nx, y2=ny, c2=getCode(x2, y2);
			}
		}
		return true;
	}

	//bresenhams line drawing algorithm
	//to rasterize empties along segment
	//returns whether anything happened
	//https://en.wikipedia.org/wiki/Bresenham's_line_algorithm
	[[nodiscard]] bool slice(vf2d s1, vf2d s2) {
		//find bounds of segment
		AABB seg_box;
		seg_box.fitToEnclose(s1);
		seg_box.fitToEnclose(s2);
		//make sure they overlap
		if(!seg_box.overlaps(getAABB())) return false;

		//convert points to my space
		s1=worldToLocal(s1);
		int x1=std::floor(s1.x), y1=std::floor(s1.y);
		s2=worldToLocal(s2);
		int x2=std::floor(s2.x), y2=std::floor(s2.y);

		//clip segment about w, h: is it valid?
		if(!clip(x1, y1, x2, y2)) return false;

		//rasterize EMPTY line
		auto edit=[this] (int x, int y) {
			if(inRangeX(x)&&inRangeY(y)) {
				grid[ix(x, y)]=PixelSet::Empty;
			}
		};

		int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;
		dx=x2-x1; dy=y2-y1;

		dx1=abs(dx), dy1=abs(dy);
		px=2*dy1-dx1, py=2*dx1-dy1;
		if(dy1<=dx1) {
			if(dx>=0) x=x1, y=y1, xe=x2;
			else x=x2, y=y2, xe=x1;

			edit(x, y);

			for(i=0; x<xe; i++) {
				x++;
				if(px<0) px+=2*dy1;
				else {
					if((dx<0&&dy<0)||(dx>0&&dy>0)) y++;
					else y--;
					px+=2*(dy1-dx1);
				}
				edit(x, y);
			}
		} else {
			if(dy>=0) x=x1, y=y1, ye=y2;
			else x=x2, y=y2, ye=y1;

			edit(x, y);

			for(i=0; y<ye; i++) {
				y++;
				if(py<=0) py+=2*dx1;
				else {
					if((dx<0&&dy<0)||(dx>0&&dy>0)) x++;
					else x--;
					py+=2*(dx1-dy1);
				}
				edit(x, y);
			}
		}
		return true;
	}

	//call this after destruction to see
	//how many new pixelsets to split into
	[[nodiscard]] std::list<PixelSet> floodfill() const {
		bool* filled=new bool[w*h];
		memset(filled, false, sizeof(bool)*w*h);

		std::vector<int> blob;
		//recursive auto needs to be passed itself
		auto fill=[this, &filled, &blob] (auto self, int i, int j) {
			int k=ix(i, j);
			//dont fill air or refill
			if(grid[k]==PixelSet::Empty||filled[k]) return;

			//update fill grid
			filled[k]=true;
			//store it
			blob.emplace_back(k);

			//update neighbors if in range
			if(i>0) self(self, i-1, j);
			if(j>0) self(self, i, j-1);
			if(i<w-1) self(self, i+1, j);
			if(j<h-1) self(self, i, j+1);
		};

		std::list<PixelSet> pixelsets;
		
		//floodfill with each block
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont fill air or refill
				int k=ix(i, j);
				if(grid[k]==PixelSet::Empty||filled[k]) continue;

				//floodfill
				blob.clear();
				fill(fill, i, j);

				//construct new pixelset
				int i_min=w-1, j_min=h-1;
				int i_max=0, j_max=0;
				for(const auto& b:blob) {
					//undo flattening
					int bi, bj;
					inv_ix(b, bi, bj);
					//update bounds
					if(bi<i_min) i_min=bi;
					if(bj<j_min) j_min=bj;
					if(bi>i_max) i_max=bi;
					if(bj>j_max) j_max=bj;
				}
				PixelSet p(1+i_max-i_min, 1+j_max-j_min);
				//update pos to reflect translation
				p.pos=localToWorld(vf2d(i_min, j_min));
				//copy over transforms and dynamics
				p.vel=vel;
				p.rot=rot;
				p.cossin=cossin;
				p.scale=scale;
				for(const auto& b:blob) {
					//undo flattening
					int bi, bj;
					inv_ix(b, bi, bj);
					p(bi-i_min, bj-j_min)=true;
				}
				p.updateTypes();
				p.updateMass();
				p.updateMeshes();
				pixelsets.emplace_back(p);
			}
		}

		delete[] filled;

		return pixelsets;
	}
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

	pos=p.pos;
	vel=p.vel;
	acc=p.acc;

	rot=p.rot;
	cossin=p.cossin;

	scale=p.scale;

	meshes=p.meshes;
}

void PixelSet::clear() {
	delete[] grid;
	delete[] colliding;

	clearMeshes();
}
#endif//PIXELSET_CLASS_H