//a model matrix per-face MIGHT be overkill, but who cares?
#pragma once
#ifndef MINESWEEPER_CLASS_H
#define MINESWEEPER_CLASS_H

//for memcpy
#include <string>
#include <vector>

#include <stack>

struct Cell {
	int num_bombs=-1;
	bool bomb=false;
	bool swept=false;
	bool flagged=false;

	bool operator!=(const Cell& c) const {
		if(num_bombs!=c.num_bombs) return true;
		if(bomb!=c.bomb) return true;
		if(swept!=c.swept) return true;
		if(flagged!=c.flagged) return true;
		return false;
	}
};

class Minesweeper {
	int width=0, height=0, depth=0;
	int num_cells=0;

	int num_bombs=0;

	void copyFrom(const Minesweeper&), clear();

	bool floodsweep(int i, int j, int k);

	void updatePrev(), checkState(), unflagSwept(), updateFaces();

public:
	Cell* cells=nullptr;
	Cell* prev_cells=nullptr;

	enum State {
		START=0,
		PLAYING,
		PAUSED,
		LOST,
		WON
	} state=START;

	//base and flagged face instances(in local space)
	//this assumes xz vertex buffer
	std::vector<cmn::mat4> tile_faces, flag_faces;

	float timer=0;

	Minesweeper() {}

	Minesweeper(int w, int h, int d, int n) {
		width=w;
		height=h;
		depth=d;
		num_cells=width*height*depth;

		num_bombs=n;

		cells=new Cell[num_cells];
		prev_cells=new Cell[num_cells];

		reset();
	}

	//ro3 1
	Minesweeper(const Minesweeper& m) {
		copyFrom(m);
	}

	//ro3 2
	~Minesweeper() {
		clear();
	}

	//ro3 3
	Minesweeper& operator=(const Minesweeper& m) {
		if(&m!=this) {
			clear();

			copyFrom(m);
		}

		return *this;
	}

	int getWidth() const { return width; }
	int getHeight() const { return height; }
	int getDepth() const { return depth; }
	int getNumCells() const { return num_cells; }

	bool inRangeX(int i) const { return i>=0&&i<width; }
	bool inRangeY(int j) const { return j>=0&&j<height; }
	bool inRangeZ(int k) const { return k>=0&&k<depth; }
	bool inRange(int i, int j, int k) const {
		return inRangeX(i)&&inRangeY(j)&&inRangeZ(k);
	}

	int ix(int i, int j, int k) const {
		return i+width*j+width*height*k;
	}

	int getNumBombs() const { return num_bombs; }

	void reset() {
		state=START;

		for(int i=0; i<num_cells; i++) {
			cells[i]=Cell{};
			prev_cells[i]=Cell{};
		}

		//fill grid with bombs
		for(int b=0; b<num_bombs; b++) {
			//find NON bombed cell
			int i, j, k;
			do {
				i=std::rand()%width;
				j=std::rand()%height;
				k=std::rand()%depth;
			} while(cells[ix(i, j, k)].bomb);
			//bomb it
			cells[ix(i, j, k)].bomb=true;
		}

		//for each cell
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				for(int k=0; k<depth; k++) {
					//bomb num=-1
					if(cells[ix(i, j, k)].bomb) continue;

					//count neighbors
					int num=0;
					for(int di=-1; di<=1; di++) {
						for(int dj=-1; dj<=1; dj++) {
							for(int dk=-1; dk<=1; dk++) {
								//increment if bomb
								int ni=i+di, nj=j+dj, nk=k+dk;
								if(inRange(ni, nj, nk)&&cells[ix(ni, nj, nk)].bomb) num++;
							}
						}
					}

					//set value to count.
					cells[ix(i, j, k)].num_bombs=num;
				}
			}
		}

		timer=0;

		update(0);
	}

	//toggle between paused/playing...
	void pause() {
		if(state==PAUSED) state=PLAYING;
		else if(state==PLAYING) state=PAUSED;
	}

	void flag(int i, int j, int k) {
		if(!(state==START||state==PLAYING)) return;

		if(!inRange(i, j, k)) return;

		cells[ix(i, j, k)].flagged^=true;

		if(state==START) state=PLAYING;
	}

	void sweep(int i, int j, int k) {
		if(!(state==START||state==PLAYING)) return;

		if(!inRange(i, j, k)) return;

		//unflag before sweeping.
		Cell& c=cells[ix(i, j, k)];
		if(c.flagged) c.flagged=false;
		else {
			if(state==START) {
				//non-tedious first move:
				//	dont explode and ensure cavity
				while(c.bomb||!floodsweep(i, j, k)) reset();

				state=PLAYING;
			} else {
				if(c.bomb) c.swept=true;
				else if(!c.swept) floodsweep(i, j, k);
			}
		}
	}

	void update(float dt){
		unflagSwept();

		checkState();

		bool changed=false;
		for(int i=0; i<num_cells; i++) {
			if(cells[i]!=prev_cells[i]) {
				changed=true;
				break;
			}
		}
		if(changed) updateFaces();
		
		updatePrev();

		if(state==PLAYING) timer+=dt;
	}
};

void Minesweeper::copyFrom(const Minesweeper& m) {
	width=m.width;
	height=m.height;
	depth=m.depth;
	num_cells=m.num_cells;
	cells=new Cell[num_cells];
	std::memcpy(cells, m.cells, sizeof(Cell)*num_cells);
	prev_cells=new Cell[num_cells];
	std::memcpy(prev_cells, m.prev_cells, sizeof(Cell)*num_cells);
	num_bombs=m.num_bombs;
	state=m.state;
	tile_faces=m.tile_faces;
	flag_faces=m.flag_faces;
	timer=m.timer;
}

void Minesweeper::clear() {
	delete[] cells;
	delete[] prev_cells;
}

//returns whether function made a cavity.
bool Minesweeper::floodsweep(int i, int j, int k) {
	using vi3d=cmn::v3d_generic<int>;

	bool flood=false;

	std::stack<vi3d> to_sweep;
	to_sweep.push({i, j, k});
	while(to_sweep.size()) {
		vi3d top=to_sweep.top();
		to_sweep.pop();
		Cell& t=cells[ix(top.x, top.y, top.z)];

		t.swept=true;

		if(t.num_bombs==0) {
			flood=true;

			//for each adjacent neighbor
			for(int di=-1; di<=1; di++) {
				for(int dj=-1; dj<=1; dj++) {
					for(int dk=-1; dk<=1; dk++) {
						//skip if out of range
						vi3d next{top.x+di, top.y+dj, top.z+dk};
						if(!inRange(next.x, next.y, next.z)) continue;

						//sweep next if unswept and unflagged
						Cell& n=cells[ix(next.x, next.y, next.z)];
						if(!n.swept&&!n.flagged) to_sweep.push(next);
					}
				}
			}
		}
	}

	return flood;
}

void Minesweeper::updatePrev() {
	std::memcpy(prev_cells, cells, sizeof(Cell)*num_cells);
}

void Minesweeper::checkState() {
	//are any bombs swept?
	for(int i=0; i<num_cells; i++) {
		Cell& c=cells[i];
		if(c.bomb&&c.swept) {
			state=LOST;
			return;
		}
	}

	//are all nonbombs swept?
	for(int i=0; i<num_cells; i++) {
		Cell& c=cells[i];
		if(!c.bomb&&!c.swept) return;
	}

	state=WON;
}

void Minesweeper::unflagSwept() {
	for(int i=0; i<num_cells; i++) {
		auto& c=cells[i];
		if(c.flagged&&c.swept) c.flagged=false;
	}
}

void Minesweeper::updateFaces() {
	tile_faces.clear();
	flag_faces.clear();

	//add matrix with new coordinate system and translation(to flag or not to flag)
	auto addFace=[&] (const cmn::vf3d& x, const cmn::vf3d& y, const cmn::vf3d& z, const cmn::vf3d& t, bool f) {
		cmn::mat4 m;
		m(0, 0)=x.x, m(0, 1)=y.x, m(0, 2)=z.x, m(0, 3)=t.x;
		m(1, 0)=x.y, m(1, 1)=y.y, m(1, 2)=z.y, m(1, 3)=t.y;
		m(2, 0)=x.z, m(2, 1)=y.z, m(2, 2)=z.z, m(2, 3)=t.z;
		m(3, 3)=1;
		if(f) flag_faces.push_back(m);
		else tile_faces.push_back(m);
	};

	const cmn::vf3d x(1, 0, 0);
	const cmn::vf3d y(0, 1, 0);
	const cmn::vf3d z(0, 0, 1);

	//for each cell
	for(int i=0; i<width; i++) {
		for(int j=0; j<height; j++) {
			for(int k=0; k<depth; k++) {
				//skip if swept
				const auto& c=cells[ix(i, j, k)];
				if(c.swept) continue;

				//if on edge or neighbor swept: orient faces
				if(!inRange(i-1, j, k)||cells[ix(i-1, j, k)].swept)
					addFace(z, -x, -y, cmn::vf3d(i, j+1, k), c.flagged);
				if(!inRange(i+1, j, k)||cells[ix(i+1, j, k)].swept)
					addFace(-z, x, -y, cmn::vf3d(i+1, j+1, k+1), c.flagged);
				if(!inRange(i, j-1, k)||cells[ix(i, j-1, k)].swept)
					addFace(x, -y, -z, cmn::vf3d(i, j, k+1), c.flagged);
				if(!inRange(i, j+1, k)||cells[ix(i, j+1, k)].swept)
					addFace(x, y, z, cmn::vf3d(i, j+1, k), c.flagged);
				if(!inRange(i, j, k-1)||cells[ix(i, j, k-1)].swept)
					addFace(-x, -z, -y, cmn::vf3d(i+1, j+1, k), c.flagged);
				if(!inRange(i, j, k+1)||cells[ix(i, j, k+1)].swept)
					addFace(x, z, -y, cmn::vf3d(i, j+1, k+1), c.flagged);
			}
		}
	}
}
#endif