#pragma once
#ifndef MINESWEEPER_3D_CLASS_H
#define MINESWEEPER_3D_CLASS_H

#define CELL_TRIANGLE 1

#include <string>
#include <vector>

#include <fstream>
#include <sstream>

struct Cell {
	int num_bombs=-1;
	bool bomb=false;
	bool swept=false;
	bool flagged=false;
};

class Minesweeper {
	int width=0, height=0, depth=0;
	int num_cells=0;

	int num_bombs=0;

	void copyFrom(const Minesweeper&), clear();

	bool floodsweep(int i, int j, int k);
	int autosweep(int i, int j, int k);

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

	std::vector<cmn::Triangle> unswept_tris;

	Minesweeper() {}

	static bool create(Minesweeper& m, int w, int h, int d, int n) {
		//invalid game dimensions
		if(w<1||h<1||d<1) return false;
		if(w>32||h>32||d>32) return false;

		m.width=w;
		m.height=h;
		m.depth=d;
		m.num_cells=m.width*m.height*m.depth;

		//too many bombs
		if(n>=m.num_cells) return false;
		m.num_bombs=n;

		m.cells=new Cell[m.num_cells];
		m.prev_cells=new Cell[m.num_cells];

		m.reset();

		return true;
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

		updatePrev();
	}

	void flag(int i, int j, int k) {
		if(!(state==START||state==PLAYING)) return;

		if(!inRange(i, j, k)) return;

		cells[ix(i, j, k)].flagged^=true;

		if(state==START) state=PLAYING;
	}

	//returns how many penalties?
	int sweep(int i, int j, int k) {
		if(!(state==START||state==PLAYING)) return 0;

		if(!inRange(i, j, k)) return 0;

		int pen=0;

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
				if(c.bomb) c.swept=true, state=LOST;
				else if(c.swept) {
					//clicked on number
					if(c.num_bombs) pen=autosweep(i, j, k);
				} else floodsweep(i, j, k);
			}
		}

		return pen;
	}

	//toggle between paused/playing...
	void pause() {
		if(state==PAUSED) state=PLAYING;
		else if(state==PLAYING) state=PAUSED;
	}

	void updatePrev() {
		std::memcpy(prev_cells, cells, sizeof(Cell)*num_cells);
	}
	
	void checkState() {
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

	//maybe put all of these into a update/sanitize function?
	void unflagSwept() {
		for(int i=0; i<num_cells; i++) {
			auto& c=cells[i];
			if(c.flagged&&c.swept) c.flagged=false;
		}
	}

	//which sprites indexes for tile and flag cells?
	void triangulateUnswept(int tile_ix, int flag_ix) {
		unswept_tris.clear();

		auto ijk2pos=[this] (int i, int j, int k) {
			return vf3d(i-.5f*width, j-.5f*height, k-.5f*depth);
		};
		const cmn::v2d vt[4]{
			{0, 0},
			{1, 0},
			{0, 1},
			{1, 1}
		};
		auto tessellate=[&] (
			int i, int j, int k, bool tf, vf3d* v, int a, int b, int c, int d
			) {
			int ix=tf?tile_ix:flag_ix;
			//pack coordinates into bottom 15 bits of id
			int id=(CELL_TRIANGLE<<24)|(ix<<16)|(i<<10)|(j<<5)|k;
			unswept_tris.push_back({
				v[a], v[c], v[b],
				vt[2], vt[1], vt[0],
				olc::WHITE, id
				});
			unswept_tris.push_back({
				v[a], v[d], v[c],
				vt[2], vt[3], vt[1],
				olc::WHITE, id
				});
		};

		//for each cell
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				for(int k=0; k<depth; k++) {
					//skip if swept
					const auto& c=cells[ix(i, j, k)];
					if(c.swept) continue;

					//setup cube points
					vf3d v[8]{
						ijk2pos(i, j, k),
						ijk2pos(i+1, j, k),
						ijk2pos(i, j+1, k),
						ijk2pos(i+1, j+1, k),
						ijk2pos(i, j, k+1),
						ijk2pos(i+1, j, k+1),
						ijk2pos(i, j+1, k+1),
						ijk2pos(i+1, j+1, k+1)
					};
					//check each boundary
					if(!inRange(i-1, j, k)||cells[ix(i-1, j, k)].swept) {
						tessellate(i, j, k, !c.flagged, v, 0, 2, 6, 4);
					}
					if(!inRange(i+1, j, k)||cells[ix(i+1, j, k)].swept) {
						tessellate(i, j, k, !c.flagged, v, 5, 7, 3, 1);
					}
					if(!inRange(i, j-1, k)||cells[ix(i, j-1, k)].swept) {
						tessellate(i, j, k, !c.flagged, v, 1, 0, 4, 5);
					}
					if(!inRange(i, j+1, k)||cells[ix(i, j+1, k)].swept) {
						tessellate(i, j, k, !c.flagged, v, 2, 3, 7, 6);
					}
					if(!inRange(i, j, k-1)||cells[ix(i, j, k-1)].swept) {
						tessellate(i, j, k, !c.flagged, v, 1, 3, 2, 0);
					}
					if(!inRange(i, j, k+1)||cells[ix(i, j, k+1)].swept) {
						tessellate(i, j, k, !c.flagged, v, 4, 6, 7, 5);
					}
				}
			}
		}
	}

	bool save(const std::string& filename) const {
		std::ofstream file(filename);
		if(file.fail()) return false;

		file<<width<<' '<<height<<' '<<depth<<'\n';
		file<<num_bombs<<' '<<state<<'\n';
		for(int i=0; i<num_cells; i++) {
			const auto& c=cells[i];
			file<<
				c.num_bombs<<' '<<
				c.bomb<<' '<<
				c.swept<<' '<<
				c.flagged<<'\n';
		}

		file.close();

		return true;
	}

	static bool load(Minesweeper& m, const std::string& filename) {
		std::ifstream file(filename);
		if(file.fail()) return false;

		std::string line;
		std::stringstream line_str;

		//get header
		std::getline(file, line);
		line_str.str(line), line_str.clear();
		int width=0, height=0, depth=0;
		line_str>>width>>height>>depth;
		std::getline(file, line);
		line_str.str(line), line_str.clear();
		int num_bombs=0;
		State state=START;
		line_str<<num_bombs<<state;

		//initialize game
		if(!Minesweeper::create(m, width, height, depth, num_bombs)) return false;
		m.state=state;

		//read cells
		for(int i=0; i<m.num_cells; i++) {
			std::getline(file, line);
			line_str.str(line), line_str.clear();

			auto& c=m.cells[i];
			line_str>>
				c.num_bombs>>
				c.bomb>>
				c.swept>>
				c.flagged;
		}

		//update previous
		m.updatePrev();

		file.close();

		return true;
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
	unswept_tris=m.unswept_tris;
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

//returns number of penalties
int Minesweeper::autosweep(int i, int j, int k) {
	using vi3d=cmn::v3d_generic<int>;

	//count flagged, unswept cells
	std::vector<vi3d> flags;
	for(int di=-1; di<=1; di++) {
		for(int dj=-1; dj<=1; dj++) {
			for(int dk=-1; dk<=1; dk++) {
				int ni=i+di, nj=j+dj, nk=k+dk;
				if(!inRange(ni, nj, nk)) continue;

				auto& n=cells[ix(ni, nj, nk)];
				if(n.flagged&&!n.swept) flags.push_back({ni, nj, nk});
			}
		}
	}

	//are there enough flags?
	auto& c=cells[ix(i, j, k)];
	if(flags.size()<c.num_bombs) return 0;

	//count erroneous flags
	int penalties=0;
	for(const auto& fl:flags) {
		auto& f=cells[ix(fl.x, fl.y, fl.z)];
		if(!f.bomb) {
			f.swept=true;
			penalties++;
		}
	}

	//if there are none,
	if(penalties==0) {
		//sweep all non flagged cells
		for(int di=-1; di<=1; di++) {
			for(int dj=-1; dj<=1; dj++) {
				for(int dk=-1; dk<=1; dk++) {
					int ni=i+di, nj=j+dj, nk=k+dk;
					if(!inRange(ni, nj, nk)) continue;

					if(!cells[ix(ni, nj, nk)].flagged) floodsweep(ni, nj, nk);
				}
			}
		}
	}

	return penalties;
}
#endif