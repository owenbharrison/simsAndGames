#pragma once
#ifndef CHUNK_CLASS_H
#define CHUNK_CLASS_H

#include <unordered_map>

struct ChunkCoord {
	int x=0, y=0, z=0;

	bool operator==(const ChunkCoord& other) const {
		return x==other.x&&y==other.y&&z==other.z;
	}
};

//hash function for unordered_map
namespace std {
	template<>
	struct hash<ChunkCoord> {
		size_t operator()(const ChunkCoord& c) const {
			return ((std::hash<int>()(c.x)^(std::hash<int>()(c.y)<<1))>>1)^(std::hash<int>()(c.z)<<1);
		}
	};
}

class Chunk {
	void copyFrom(const Chunk&), clear();

public:
	static constexpr int width=16;
	static constexpr int height=16;
	static constexpr int depth=16;
	static constexpr int size=width*height*depth;

	bool* blocks=new bool[size];

	//store neighbors
	Chunk* left=nullptr, * right=nullptr;//x
	Chunk* bottom=nullptr, * top=nullptr;//y
	Chunk* back=nullptr, * front=nullptr;//z
	ChunkCoord coord;

	std::vector<Triangle> triangles;

	Chunk() {
		blocks=new bool[size];
		for(int i=0; i<size; i++) blocks[i]=false;
	}

	Chunk(int x, int y, int z) : Chunk() {
		coord={x, y, z};
	}

	//1
	Chunk(const Chunk& c) {
		copyFrom(c);
	}

	//2
	~Chunk() {
		clear();
	}

	//3
	Chunk& operator=(const Chunk& c) {
		if(&c==this) return *this;

		clear();

		copyFrom(c);

		return *this;
	}

	static int ix(int i, int j, int k) {
		return i+width*j+height*width*k;
	}

	void triangulate() {
		triangles.clear();

		static const int dims[3]{width, height, depth};

		int ijk[3], cijk[3];

		//for every axis...
		bool* to_mesh=nullptr, * meshed=nullptr;
		for(int axis_a=0; axis_a<3; axis_a++) {
			int axis_b=(axis_a+1)%3, axis_c=(axis_a+2)%3;

			//determine sizing
			const int& dim_a=dims[axis_a];
			const int& dim_b=dims[axis_b];
			const int& dim_c=dims[axis_c];

			//allocate grids
			to_mesh=new bool[dim_b*dim_c];
			meshed=new bool[dim_b*dim_c];
			auto bc_ix=[&dim_b] (int b, int c) { return b+dim_b*c; };

			//which neighbors along axis?
			Chunk* minus=nullptr, * plus=nullptr;
			switch(axis_a) {
				case 0: minus=left, plus=right; break;
				case 1: minus=bottom, plus=top; break;
				case 2: minus=back, plus=front; break;
			}

			//slice thru axis
			for(int a=0; a<dim_a; a++) {
				//for each side...
				for(int s=0; s<2; s++) {
					//check neighbor if on edge
					Chunk* check=this;
					int ca=a-1+2*s;
					if(s==0&&a==0) check=minus, ca=dim_a-1;
					else if(s==1&&a==dim_a-1) check=plus, ca=0;

					//which faces to consider?
					memset(to_mesh, false, sizeof(bool)*dim_b*dim_c);
					for(int b=0; b<dim_b; b++) {
						for(int c=0; c<dim_c; c++) {
							//skip if air
							ijk[axis_a]=a, ijk[axis_b]=b, ijk[axis_c]=c;
							if(!blocks[ix(ijk[0], ijk[1], ijk[2])]) continue;

							//is neighbor nonexistent or air?
							cijk[axis_a]=ca, cijk[axis_b]=b, cijk[axis_c]=c;
							if(!check||!check->blocks[ix(cijk[0], cijk[1], cijk[2])]) {
								to_mesh[bc_ix(b, c)]=true;
							}
						}
					}

					//greedymeshing!
					memset(meshed, false, sizeof(bool)*dim_b*dim_c);
					for(int b=0; b<dim_b; b++) {
						for(int c=0; c<dim_c; c++) {
							//skip if invalid or meshed
							if(!to_mesh[bc_ix(b, c)]||meshed[bc_ix(b, c)]) continue;

							//try combine along dim_b
							int sz_b=1;
							for(int b_=1+b; b_<dim_b; b_++, sz_b++) {
								//stop if invalid or meshed
								if(!to_mesh[bc_ix(b_, c)]||meshed[bc_ix(b_, c)]) break;
							}

							//try combine combination along dim_c
							int sz_c=1;
							for(int c_=1+c; c_<dim_c; c_++, sz_c++) {
								bool able=true;
								for(int b_=0; b_<sz_b; b_++) {
									//stop if invalid or meshed
									if(!to_mesh[bc_ix(b+b_, c_)]||meshed[bc_ix(b+b_, c_)]) {
										able=false;
										break;
									}
								}
								if(!able) break;
							}

							//set meshed
							for(int b_=0; b_<sz_b; b_++) {
								for(int c_=0; c_<sz_c; c_++) {
									meshed[bc_ix(b+b_, c+c_)]=true;
								}
							}

							//tesselate
							vf3d v0(width*coord.x, height*coord.y, depth*coord.z);
							v0[axis_a]+=a, v0[axis_b]+=b, v0[axis_c]+=c;
							vf3d v1=v0; v1[axis_b]+=sz_b;
							vf3d v2=v0; v2[axis_b]+=sz_b, v2[axis_c]+=sz_c;
							vf3d v3=v0; v3[axis_c]+=sz_c;
							if(s==1) {
								//push along axis
								v0[axis_a]++, v1[axis_a]++, v2[axis_a]++, v3[axis_a]++;
								//swap face order
								std::swap(v1, v3);
							}
							triangles.push_back({v0, v3, v1});
							triangles.push_back({v2, v1, v3});
						}
					}
				}
			}

			delete[] to_mesh;
			delete[] meshed;
		}
	}
};

void Chunk::copyFrom(const Chunk& c) {
	blocks=new bool[size];
	for(int i=0; i<size; i++) {
		blocks[i]=c.blocks[i];
	}

	left=c.left, right=c.right;
	bottom=c.bottom, top=c.top;
	back=c.back, front=c.front;

	coord=c.coord;

	triangles=c.triangles;
}

void Chunk::clear() {
	delete[] blocks;
}
#endif 