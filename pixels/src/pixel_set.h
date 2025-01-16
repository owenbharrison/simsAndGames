/*TODO
impl floodfill
* compress shape into multiples
* pass func to determine what to do with new ones
impl polygon to PixelSet
*/

#pragma once
#ifndef PIXELSET_CLASS_H
#define PIXELSET_CLASS_H

typedef unsigned char byte;

#include "aabb.h"

#include <stack>

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

//polar to cartesian helper
vf2d polar(float rad, float angle) {
	return {rad*std::cosf(angle), rad*std::sinf(angle)};
}

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
		Empty=0,
		Normal,
		Edge
	};

	vf2d pos, old_pos, forces;
	float total_mass=1;
	vf2d center_of_mass;

	float rot=0, old_rot=0, torques=0;
	vf2d cossin;
	float moment_of_inertia=1;

	float scale=1;

	struct Mesh {
		int i=0, j=0, w=0, h=0;
		olc::Pixel col=olc::WHITE;

		Mesh() {}

		Mesh(int i_, int j_, int w_, int h_, olc::Pixel c) : i(i_), j(j_), w(w_), h(h_), col(c) {}
	};
	std::vector<Mesh> meshes;

	struct Outline {
		int i=0, j=0;
		bool vert=false;
		int sz=0;
		olc::Pixel col=olc::WHITE;

		Outline() {}

		Outline(int i_, int j_, bool v, int s, olc::Pixel c) : i(i_), j(j_), vert(v), sz(s), col(c) {}
	};
	std::vector<Outline> outlines;

	olc::Pixel col=olc::WHITE;

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
	static PixelSet fromPolygon(const std::vector<vf2d>& polygon, float scale) {
		//get bounds of polygon
		AABB box;
		for(const auto& p:polygon) box.fitToEnclose(p);

		//determine spacing
		vf2d size=box.max-box.min;
		int w=1+size.x/scale, h=1+size.y/scale;

		//for every point
		PixelSet p(w, h);
		p.pos=box.min;
		p.scale=scale;
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//is it inside the polygon
				vf2d pos=p.localToWorld(vf2d(i, j));

				//deterministic, but less artifacting
				float angle=random(2*Pi);
				vf2d dir=polar(1, angle);

				//polygon raycast algorithm
				int num=0;
				for(int k=0; k<polygon.size(); k++) {
					const auto& a=polygon[k];
					const auto& b=polygon[(k+1)%polygon.size()];
					vf2d tu=lineLineIntersection(a, b, pos, pos+dir);
					if(tu.x>0&&tu.x<1&&tu.y>0) num++;
				}

				//odd? inside!
				p(i, j)=num%2;
			}
		}

		p.updateTypes();
		p.updateMeshes();
		p.updateOutlines();

		p.updateMass();
		p.pos+=box.min-p.getAABB().min;
		p.old_pos=p.pos;
		p.updateInertia();

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
		vf2d scaled=scale*(p-center_of_mass);
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
		return scaled/scale+center_of_mass;
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
		bool edited=false;
		auto edit=[this, &edited] (int x, int y) {
			if(inRangeX(x)&&inRangeY(y)) {
				auto& curr=grid[ix(x, y)];
				if(curr!=PixelSet::Empty) {
					curr=PixelSet::Empty;
					edited=true;
				}
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
		return edited;
	}

	//call this after destruction to see
	//	how many new pixelsets to split into
	//basically a parts detection algorithm
	[[nodiscard]] std::vector<PixelSet> floodfill() const {
		//store which weve visited
		bool* filled=new bool[w*h];
		memset(filled, false, sizeof(bool)*w*h);

		//store current part
		std::vector<int> blob;

		//dynamics info
		vf2d vel=pos-old_pos;
		float rot_vel=rot-old_rot;

		//store each part
		std::vector<PixelSet> pixelsets;

		//for every block
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				blob.clear();

				//iterative floodfill
				std::stack<int> queue;
				queue.push(ix(i, j));
				while(queue.size()) {
					int c=queue.top();
					queue.pop();

					//dont fill air or refill
					if(grid[c]==PixelSet::Empty||filled[c]) continue;

					//update fill grid
					filled[c]=true;
					//store it
					blob.emplace_back(c);

					//update neighbors if in range
					int ci, cj;
					inv_ix(c, ci, cj);
					if(ci>0) queue.push(ix(ci-1, cj));
					if(cj>0) queue.push(ix(ci, cj-1));
					if(ci<w-1) queue.push(ix(ci+1, cj));
					if(cj<h-1) queue.push(ix(ci, cj+1));
				}

				//just making sure...
				if(blob.empty()) continue;

				//construct new pixelset
				int i_min=w, j_min=h;
				int i_max=-1, j_max=-1;
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
				//copy over transforms and dynamics
				p.rot=rot;
				p.old_rot=p.rot-rot_vel;
				p.cossin=cossin;
				p.scale=scale;
				p.col=olc::Pixel(rand()%255, rand()%255, rand()%255);
				for(const auto& b:blob) {
					//undo flattening
					int bi, bj;
					inv_ix(b, bi, bj);
					p(bi-i_min, bj-j_min)=true;
				}
				p.updateTypes();
				p.updateMass();
				//update pos to reflect translation
				//this.localToWorld(i_min, j_min)=p.localToWorld(0, 0)
				p.pos=localToWorld(p.center_of_mass+vf2d(i_min, j_min));
				p.old_pos=p.pos-vel;
				p.updateInertia();
				pixelsets.emplace_back(p);
			}
		}
		delete[] filled;

		//dont recolor if not split
		if(pixelsets.size()==1) pixelsets.front().col=col;
		for(auto& p:pixelsets) {
			p.updateMeshes();
			p.updateOutlines();
		}

		return pixelsets;
	}
#pragma endregion

#pragma region GRAPHICS
	void clearMeshes() {
		meshes.clear();
	}

	//greedy meshing!
	void updateMeshes() {
		clearMeshes();

		//store which blocks have been meshed
		bool* meshed=new bool[w*h];
		memset(meshed, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont mesh air
				const auto& curr=grid[ix(i, j)];
				if(curr==PixelSet::Empty) continue;

				//dont remesh
				if(meshed[ix(i, j)]) continue;

				//combine into row
				int ext_w=1;
				for(int i_=1+i; i_<w; i_++, ext_w++) {
					//only mesh similar and dont remesh
					if(grid[ix(i_, j)]==PixelSet::Empty||meshed[ix(i_, j)]) break;
				}

				//combine entire rows
				int ext_h=1;
				for(int j_=1+j; j_<h; j_++, ext_h++) {
					bool able=true;
					for(int i_=0; i_<ext_w; i_++) {
						//only mesh similar and dont remesh
						if(grid[ix(i+i_, j_)]==PixelSet::Empty||meshed[ix(i+i_, j_)]) {
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
				meshes.emplace_back(i, j, ext_w, ext_h, col);
			}
		}
		delete[] meshed;
	}

	void clearOutlines() {
		outlines.clear();
	}

	//how to make this smaller?
	void updateOutlines() {
		clearOutlines();
		
		bool* to_line=new bool[w*h];
		bool* lined=new bool[w*h];

#pragma region TOP
		//which edges should we consider?
		memset(to_line, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont line air
				int k=ix(i, j);
				if(grid[k]==PixelSet::Empty) continue;

				//place line on edge or boundary
				if(j==0||grid[ix(i, j-1)]==PixelSet::Empty) to_line[k]=true;
			}
		}

		//greedy lining
		memset(lined, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont reline
				int k=ix(i, j);
				if(!to_line[k]||lined[k]) continue;

				//combine
				int ext=1;
				for(int i_=1+i; i_<w; i_++, ext++) {
					//should we or have we lined?
					int k_=ix(i_, j);
					if(!to_line[k_]||lined[k_]) break;
					//set lined
					lined[k_]=true;
				}

				//add line
				outlines.emplace_back(i, j, false, ext, olc::BLACK);
			}
		}
#pragma endregion

#pragma region LEFT
		//which edges should we consider?
		memset(to_line, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont line air
				int k=ix(i, j);
				if(grid[k]==PixelSet::Empty) continue;

				//place line on edge or boundary
				if(i==0||grid[ix(i-1, j)]==PixelSet::Empty) to_line[k]=true;
			}
		}

		//greedy lining
		memset(lined, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont reline
				int k=ix(i, j);
				if(!to_line[k]||lined[k]) continue;

				//combine
				int ext=1;
				for(int j_=1+j; j_<h; j_++, ext++) {
					//should we or have we lined?
					int k_=ix(i, j_);
					if(!to_line[k_]||lined[k_]) break;
					//set lined
					lined[k_]=true;
				}

				//add line
				outlines.emplace_back(i, j, true, ext, olc::BLACK);
			}
		}
#pragma endregion

#pragma region BOTTOM
		//which edges should we consider?
		memset(to_line, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont line air
				int k=ix(i, j);
				if(grid[k]==PixelSet::Empty) continue;

				//place line on edge or boundary
				if(j==h-1||grid[ix(i, j+1)]==PixelSet::Empty) to_line[k]=true;
			}
		}

		//greedy lining
		memset(lined, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont reline
				int k=ix(i, j);
				if(!to_line[k]||lined[k]) continue;

				//combine
				int ext=1;
				for(int i_=1+i; i_<w; i_++, ext++) {
					//should we or have we lined?
					int k_=ix(i_, j);
					if(!to_line[k_]||lined[k_]) break;
					//set lined
					lined[k_]=true;
				}

				//add line
				outlines.emplace_back(i, j+1, false, ext, olc::BLACK);
			}
		}
#pragma endregion

#pragma region RIGHT
		//which edges should we consider?
		memset(to_line, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont line air
				int k=ix(i, j);
				if(grid[k]==PixelSet::Empty) continue;

				//place line on edge or boundary
				if(i==w-1||grid[ix(i+1, j)]==PixelSet::Empty) to_line[k]=true;
			}
		}

		//greedy lining
		memset(lined, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont reline
				int k=ix(i, j);
				if(!to_line[k]||lined[k]) continue;

				//combine
				int ext=1;
				for(int j_=1+j; j_<h; j_++, ext++) {
					//should we or have we lined?
					int k_=ix(i, j_);
					if(!to_line[k_]||lined[k_]) break;
					//set lined
					lined[k_]=true;
				}

				//add line
				outlines.emplace_back(i+1, j, true, ext, olc::BLACK);
			}
		}
#pragma endregion

		delete[] to_line;
		delete[] lined;
	}
#pragma endregion

#pragma region PHYSICS
	void applyForce(const vf2d& f, const vf2d& p) {
		//update translational forces
		forces+=f;

		//torque based on pivot dist
		vf2d r=p-pos;
		torques+=r.cross(f);
	}

	//center of mass using moments
	void updateMass() {
		center_of_mass={0, 0};
		total_mass=0;
		const float mass=scale*scale;
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//only incorporate solid blocks
				if(grid[ix(i, j)]==PixelSet::Empty) continue;

				//sum of masses
				total_mass+=mass;

				//sum of moments
				center_of_mass+=mass*vf2d(.5f+i, .5f+j);
			}
		}
		//com = total moment / total mass
		center_of_mass/=total_mass;
	}

	//this should be called after updatemass
	void updateInertia() {
		//make sure NOT zero
		moment_of_inertia=1e-6f;
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//only incorporate solid blocks
				if(grid[ix(i, j)]==PixelSet::Empty) continue;

				//I=mr^2
				vf2d sub=localToWorld(vf2d(.5f+i, .5f+j))-pos;
				moment_of_inertia+=sub.dot(sub);
			}
		}
		//mass const, so we pull it out
		moment_of_inertia*=scale*scale;
	}

	void updateRot() {
		//precompute trig
		cossin=polar(1, rot);
	}

	void update(float dt) {
		//store vel and update oldpos
		vf2d vel=pos-old_pos;
		old_pos=pos;

		//verlet integration
		vf2d acc=forces/total_mass;
		pos+=vel+acc*dt*dt;

		//reset forces
		forces={0, 0};

		//store rotvel and update oldrot
		float rot_vel=rot-old_rot;
		old_rot=rot;

		//verlet integration
		float rot_acc=torques/moment_of_inertia;
		rot+=rot_vel+rot_acc*dt*dt;
		updateRot();

		//reset torques
		torques=0;
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

				//center of MY block in world space
				vf2d my_world=localToWorld(vf2d(.5f+i, .5f+j));

				//my block in THEIR space
				vf2d my_in_their=p.worldToLocal(my_world);

				//is it a valid position?
				int pi=std::floor(my_in_their.x), pj=std::floor(my_in_their.y);
				if(!p.inRangeX(pi)||!p.inRangeY(pj)) continue;

				//is there anything there?
				if(p(pi, pj)==Empty) continue;

				//update flags
				colliding[ix(i, j)]=true;
				p.colliding[p.ix(pi, pj)]=true;
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

	//positions
	pos=p.pos;
	old_pos=p.old_pos;
	forces=p.forces;

	//masses
	total_mass=p.total_mass;
	center_of_mass=p.center_of_mass;

	//rotations
	rot=p.rot;
	old_rot=p.old_rot;
	cossin=p.cossin;
	torques=p.torques;
	moment_of_inertia=p.moment_of_inertia;

	//scale
	scale=p.scale;

	//graphics
	meshes=p.meshes;
	outlines=p.outlines;
	col=p.col;
}

void PixelSet::clear() {
	delete[] grid;
	delete[] colliding;

	clearMeshes();
	clearOutlines();
}
#endif//PIXELSET_CLASS_H