#pragma once
#ifndef MAP_CLASS_H

#include <cmath>
#include <vector>

#include "tile.h"
#include "mesh.h"

float randFloat(float a=1, float b=0) {
	static const float rand_max=RAND_MAX;

	float t=rand()/rand_max;
	return a+t*(b-a);
}

//impl chunking...

//is this necessary yet?
struct AABB {
	olc::vf2d min, max;

	bool contains(const olc::vf2d& v) const {
		if(v.x<min.x||v.x>max.x) return false;
		if(v.y<min.y||v.y>max.y) return false;
		return true;
	}

	//still not sure how this works...
	bool overlaps(const AABB& a) const {
		if(min.x>a.max.x||max.x<a.min.x) return false;
		if(min.y>a.max.y||max.y<a.min.y) return false;
		return true;
	}
};

struct Map {
	size_t width, height;
	Tile* tiles=nullptr;

	bool* meshed=nullptr;
	std::list<Mesh> meshes;

	bool* grown=nullptr;

	Map() : Map(1, 1) {}

	Map(size_t w, size_t h) : width(w), height(h) {
		tiles=new Tile[width*height];
		generateTerrain(0);

		meshed=new bool[width*height];
		constructMeshes();

		grown=new bool[width*height];
		memset(grown, false, sizeof(bool)*width*height);
	}

	//1 copy constructor
	void copyFrom(const Map& m) {
		width=m.width;
		height=m.height;

		tiles=new Tile[width*height];
		generateTerrain(0);

		meshed=new bool[width*height];
		constructMeshes();

		grown=new bool[width*height];
		memset(grown, false, sizeof(bool)*width*height);
	}

	Map(const Map& m) {
		copyFrom(m);
	}

	//2 destructor
	void clear() {
		delete[] tiles;
		delete[] meshed;
		delete[] grown;
	}

	~Map() {
		clear();
	}

	//3 assignment operator
	Map& operator=(const Map& m) {
		if(&m==this) return *this;

		clear();

		copyFrom(m);

		return *this;
	}

	bool inRangeX(int i) const { return i>=0&&i<width; }
	bool inRangeY(int j) const { return j>=0&&j<height; }
	bool inRange(int i, int j) const { return inRangeX(i)&&inRangeY(j); }

	//i could make this throw an exception...
	int ix(int i, int j) const {
		return i+width*j;
	}

	//these functions should go into a chunk class.
	//add biomes?
	void generateTerrain(float offset) {
		//for sum of sines i need a random phase shift as well

		//place blocks column by column
		for(int i=0; i<width; i++) {
			float x=2.f*i/width;

			//sum of sines
			float y=0;
			float freq=1, amp=.5f;
			for(int j=0; j<8; j++) {
				y+=amp*std::sinf(offset+freq*x);
				freq*=2, amp/=2;
			}

			//stack based on terrain height
			int hgt=remap(y, -1, 1, 0, height);
			for(int j=0; j<height; j++) {
				int diff=j-hgt;
				tiles[ix(i, j)]=
					diff<0?Tile::Air:
					diff==0?Tile::Grass:
					diff<18?Tile::Dirt:
					Tile::Rock;
			}
		}
	}

#pragma region BLOCK UPDATES
	//note: these only update the tiles, and they return whether anything changed
	//so you have to call constructmeshes after one of these so the meshes also change

	//impl surface depth in which not to grow grass below...
	//grass requires light level - worst premade ever
	bool growGrass() {
		memset(grown, false, sizeof(bool)*width*height);

		bool changed=false;

		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				//not dirt?
				if(tiles[ix(i, j)]!=Tile::Dirt) continue;

				//should it only ask up, left, right?
				bool can_spread=false;
				for(int k=0; k<3; k++) {
					int ni=i, nj=j;
					switch(k) {
						case 0: ni--; break;
						case 1: ni++; break;
						case 2: nj--; break;
					}
					if(!inRange(ni, nj)) continue;

					//is the dirt touching air
					if(tiles[ix(ni, nj)]==Tile::Air) {
						can_spread=true;
						break;
					}
				}
				if(!can_spread) continue;

				bool to_spread=false;
				for(int di=-1; di<=1; di++) {
					for(int dj=-1; dj<=1; dj++) {
						int ni=i+di, nj=j+dj;
						if(!inRange(ni, nj)) continue;

						//only spread from grass
						if(tiles[ix(ni, nj)]!=Tile::Grass) continue;

						//only spread if said tile hasnt been
						//updated to avoid immediate spread
						if(!grown[ix(ni, nj)]) {
							to_spread=true;
							break;
						}
					}
				}
				if(!to_spread) continue;

				tiles[ix(i, j)]=Tile::Grass;
				grown[ix(i, j)]=true;
				changed=true;
			}
		}

		return changed;
	}

	//this will need to be a chunk method
	//i can pass the surrounding chunks when i need to later.
	//for the edges if there is no chunk assume it is a barrier.
	bool moveDynamicTiles() {
		//from -> to
		std::vector<olc::vi2d> swaps;
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				const int k=ix(i, j);
				const auto props=getProperties(tiles[k]);
				if(props==TileProps::None) continue;

				//the subtraction is to determine density
				//and whether a tile should sink into another
				const auto type=getType(tiles[k]);

				//CHECK DOWN FIRST
				bool btm_edge=j==height-1;
				if((props&TileProps::MoveDown)&&!btm_edge&&(type-getType(tiles[ix(i, j+1)])>0)) {
					swaps.emplace_back(k, ix(i, j+1));
					continue;
				}

				//make sure it doesnt diagonally go thru blocks
				bool left_edge=i==0;
				bool side_left=!left_edge&&(type-getType(tiles[ix(i-1, j)])>0);
				bool right_edge=i==width-1;
				bool side_right=!right_edge&&(type-getType(tiles[ix(i+1, j)])>0);

				//THEN DIAGONAL
				if(props&TileProps::MoveDiag) {
					bool down_left=side_left&&!btm_edge&&(type-getType(tiles[ix(i-1, j+1)])>0);
					bool down_right=side_right&&!btm_edge&&(type-getType(tiles[ix(i+1, j+1)])>0);
					//choose random if both
					if(down_left&&down_right) down_right=!(down_left=randFloat()<.5f);
					if(down_left) {
						swaps.emplace_back(k, ix(i-1, j+1));
						continue;
					}
					if(down_right) {
						swaps.emplace_back(k, ix(i+1, j+1));
						continue;
					}
				}

				//THEN ADJACENT
				if(props&TileProps::MoveSide) {
					//choose random if both
					if(side_left&&side_right) side_right=!(side_left=randFloat()<.5f);
					if(side_left) {
						swaps.emplace_back(k, ix(i-1, j));
						continue;
					}
					if(side_right) {
						swaps.emplace_back(k, ix(i+1, j));
						continue;
					}
				}
			}
		}

		//sort swaps by destination
		std::sort(swaps.begin(), swaps.end(), [] (const olc::vi2d& a, const olc::vi2d& b) {
			return a.y<b.y;
		});

		//only swap unique destinations
		//i.e. skip dupes and update the first one
		int prev=-1;//no index should be -1
		for(const auto& s:swaps) {
			if(s.y!=prev) std::swap(tiles[s.x], tiles[s.y]);
			prev=s.y;
		}

		//did i swap anything?
		return swaps.size();
	}

	//lava+water=obsidian
	//this function is subject to change.
	//it needs more generalization for other combinations
	bool combineLiquids() {
		std::list<size_t> to_air, to_obby;
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				//replace lava with obby
				if(tiles[ix(i, j)]!=Tile::Lava) continue;

				//adjacent neighbors
				bool crystalize=false;
				for(int k=0; k<4; k++) {
					int ni=i, nj=j;
					switch(k) {
						case 0: ni--; break;
						case 1: nj--; break;
						case 2: ni++; break;
						case 3: nj++; break;
					}
					if(!inRange(ni, nj)) continue;

					//only combine with water
					if(tiles[ix(ni, nj)]!=Tile::Water) continue;

					to_air.push_back(ix(ni, nj));
					crystalize=true;
				}
				if(crystalize) to_obby.push_back(ix(i, j));
			}
		}

		//never set cellular automata while iterating!
		for(const auto& a:to_air) tiles[a]=Tile::Air;
		for(const auto& o:to_obby) tiles[o]=Tile::Obsidian;

		return to_air.size()+to_obby.size();
	}

#pragma endregion

	//greedy meshing! :D
	//how will i render water?
	void constructMeshes() {
		memset(meshed, false, sizeof(bool)*width*height);

		meshes.clear();
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				if(meshed[ix(i, j)]) continue;

				//dont mesh air.
				auto& t=tiles[ix(i, j)];
				if(t==Tile::Air) continue;

				//can combine down?
				int ext_h=1;
				for(int l=j+1; l<height; l++, ext_h++) {
					int z=ix(i, l);
					if(tiles[z]!=t||meshed[z]) break;
				}

				//can combine right?
				int ext_w=1;
				for(int k=i+1; k<width; k++, ext_w++) {
					//check entire column
					bool able=true;
					for(int l=0; l<ext_h; l++) {
						int z=ix(k, j+l);
						if(tiles[z]!=t||meshed[z]) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int k=0; k<ext_w; k++) {
					for(int l=0; l<ext_h; l++) {
						meshed[ix(i+k, j+l)]=true;
					}
				}

				//append to meshlist
				meshes.push_back({{i, j}, {ext_w, ext_h}, t});
			}
		}
	}

	std::vector<AABB> getIntersectingTilesX(const AABB& r) {
		olc::vf2d sz=r.max-r.min;

		std::vector<AABB> a;
		a.reserve(int(1+sz.x)*int(1+sz.y));
		for(int i=0; i<=sz.x; i++) {
			for(int j=0; j<=sz.y; j++) {

			}
		}
	}
};
#endif//MAP_CLASS_H