#pragma once
#ifndef CONVERSIONS_UTIL_H
#define CONVERSIONS_UTIL_H

#include "mesh.h"

#include "voxel_set.h"

#include "prism.h"

//for memset & memcpy
#include <cstring>

#include <vector>

VoxelSet meshToVoxels(const Mesh& m, cmn::vf3d res) {
	//get global mesh dimensions
	const cmn::vf3d inf(1e300, 1e300, 1e300);
	cmn::AABBf3 box{inf, -inf};
	for(const auto& v:m.verts) {
		float w=1;
		box.fitToEnclose(matMulVec(m.model, v, w));
	}

	//determine sizing
	cmn::vf3d size=box.max-box.min;
	int width=1+size.x/res.x;
	int height=1+size.y/res.y;
	int depth=1+size.z/res.z;

	//allocate
	VoxelSet v(width, height, depth);
	v.scale=res;
	v.trans=box.min;

	//for every voxel...
	for(int i=0; i<v.getWidth(); i++) {
		for(int j=0; j<v.getHeight(); j++) {
			for(int k=0; k<v.getDepth(); k++) {
				//is it's center in the polyhedra?
				cmn::vf3d pos=v.trans+v.scale*cmn::vf3d(.5f+i, .5f+j, .5f+k);
				if(m.contains(pos)) v.grid[v.ix(i, j, k)]=true;
			}
		}
	}

	return v;
}

//this bad boy is insane.
Mesh voxelsToMesh(const VoxelSet& v) {
	//get sizing
	const int width=v.getWidth();
	const int height=v.getHeight();
	const int depth=v.getDepth();
	const int dims[3]{width, height, depth};

	Mesh m;
	m.scale=v.scale;
	m.trans=v.trans;

	//mesh vertex lookup
	const int w_lkp=1+width;
	const int h_lkp=1+height;
	const int d_lkp=1+depth;
	int* lookup=new int[w_lkp*h_lkp*d_lkp];
	std::memset(lookup, -1, sizeof(int)*w_lkp*h_lkp*d_lkp);
	auto lookupIX=[&] (int i, int j, int k) {
		return i+j*w_lkp+k*w_lkp*h_lkp;
	};

	//vertex index getter
	auto getOrMakeVert=[&] (int x, int y, int z) {
		int& ix=lookup[lookupIX(x, y, z)];
		//not found
		if(ix==-1) {
			//make new & add to lookup
			ix=m.verts.size();
			m.verts.push_back(cmn::vf3d(x, y, z));
		}
		return ix;
	};

	//temporary indexing arrays
	int ijk[3], cijk[3];
	int v0[3], v1[3], v2[3], v3[3];

	//for every axis...
	bool* to_mesh=nullptr, * meshed=nullptr;
	for(int axis_a=0; axis_a<3; axis_a++) {
		//get other two axes
		int axis_b=(axis_a+1)%3, axis_c=(axis_a+2)%3;

		//determine sizing
		const int& dim_a=dims[axis_a];
		const int& dim_b=dims[axis_b];
		const int& dim_c=dims[axis_c];

		//allocate grids
		to_mesh=new bool[dim_b*dim_c];
		meshed=new bool[dim_b*dim_c];
		auto bc_ix=[&dim_b] (int b, int c) { return b+dim_b*c; };

		//slice thru axis
		for(int a=0; a<dim_a; a++) {
			//for each side...
			for(int s=0; s<2; s++) {
				//are we on edge?
				int ca=a-1+2*s;
				bool on_edge=(s==0&&a==0)||(s==1&&a==dim_a-1);

				//which faces to consider?
				std::memset(to_mesh, false, sizeof(bool)*dim_b*dim_c);
				for(int b=0; b<dim_b; b++) {
					for(int c=0; c<dim_c; c++) {
						//skip if air
						ijk[axis_a]=a, ijk[axis_b]=b, ijk[axis_c]=c;
						if(!v.grid[v.ix(ijk[0], ijk[1], ijk[2])]) continue;

						//is neighbor nonexistent or air?
						cijk[axis_a]=ca, cijk[axis_b]=b, cijk[axis_c]=c;

						if(on_edge||!v.grid[v.ix(cijk[0], cijk[1], cijk[2])]) {
							to_mesh[bc_ix(b, c)]=true;
						}
					}
				}

				//greedy meshing <3
				std::memset(meshed, false, sizeof(bool)*dim_b*dim_c);
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
						//base position
						v0[axis_a]=a, v0[axis_b]=b, v0[axis_c]=c;
						//face sides pushed along axes
						v1[0]=v0[0], v1[1]=v0[1], v1[2]=v0[2];//v1=v0
						v1[axis_b]+=sz_b;
						v2[0]=v0[0], v2[1]=v0[1], v2[2]=v0[2];//v2=v0
						v2[axis_b]+=sz_b, v2[axis_c]+=sz_c;
						v3[0]=v0[0], v3[1]=v0[1], v3[2]=v0[2];//v3=v0
						v3[axis_c]+=sz_c;
						if(s==1) {
							//push along axis & swap face order
							v0[axis_a]++, v1[axis_a]++, v2[axis_a]++, v3[axis_a]++;
							std::swap(v1, v3);
						}

						//convert integer space coords to index lookup
						int v0_ix=getOrMakeVert(v0[0], v0[1], v0[2]);
						int v1_ix=getOrMakeVert(v1[0], v1[1], v1[2]);
						int v2_ix=getOrMakeVert(v2[0], v2[1], v2[2]);
						int v3_ix=getOrMakeVert(v3[0], v3[1], v3[2]);
						m.tris.push_back({v0_ix, v3_ix, v1_ix});
						m.tris.push_back({v2_ix, v1_ix, v3_ix});
					}
				}
			}
		}

		delete[] to_mesh;
		delete[] meshed;
	}

	delete[] lookup;

	m.updateMatrixes();

	return m;
}

std::vector<Prism> voxelsToPrisms(const VoxelSet& v) {
	std::vector<Prism> prisms;
	
	const auto& w_v=v.getWidth();
	const auto& h_v=v.getHeight();
	const auto& d_v=v.getDepth();

	//which voxels have i used?
	bool* meshed=new bool[w_v*d_v];
	auto ik_ix=[w_v] (int i, int k) {
		return i+w_v*k;
	};

	//generalized indexing logic
	const int dims[2]{w_v, d_v};
	int ik[2];

	//slice along height
	for(int j=0; j<h_v; j++) {
		//reset meshed
		std::memset(meshed, false, sizeof(bool)*w_v*d_v);

		//flip prioritized axis each slice
		const int axis_a=j%2, axis_b=(axis_a+1)%2;
		const int dim_a=dims[axis_a], dim_b=dims[axis_b];
		for(int a=0; a<dim_a; a++) {
			for(int b=0; b<dim_b; b++) {
				ik[axis_a]=a, ik[axis_b]=b;

				//skip if air or meshed
				if(!v.grid[v.ix(ik[0], j, ik[1])]
					||meshed[ik_ix(ik[0], ik[1])]) continue;

				//try combine in a
				int sz_a=1;
				for(int a_=1+a; a_<dim_a; a_++, sz_a++) {
					//stop if air or meshed
					ik[axis_a]=a_, ik[axis_b]=b;
					if(!v.grid[v.ix(ik[0], j, ik[1])]
						||meshed[ik_ix(ik[0], ik[1])]) break;
				}

				//try combine combination in b
				int sz_b=1;
				for(int b_=1+b; b_<dim_b; b_++, sz_b++) {
					bool able=true;
					for(int a_=0; a_<sz_a; a_++) {
						//stop if air or meshed
						ik[axis_a]=a+a_, ik[axis_b]=b_;
						if(!v.grid[v.ix(ik[0], j, ik[1])]
							||meshed[ik_ix(ik[0], ik[1])]) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//limit size to valid
				for(int s=0; s<Prism::num_sizes; s++) {
					//check both ways
					const auto& w=Prism::sizes[s][0];
					const auto& h=Prism::sizes[s][1];
					if(w<=sz_a&&h<=sz_b) {
						sz_a=w, sz_b=h;
						break;
					}
					if(w<=sz_b&&h<=sz_a) {
						sz_b=w, sz_a=h;
						break;
					}
				}

				//set meshed
				for(int a_=0; a_<sz_a; a_++) {
					for(int b_=0; b_<sz_b; b_++) {
						ik[axis_a]=a+a_, ik[axis_b]=b+b_;
						meshed[ik_ix(ik[0], ik[1])]=true;
					}
				}

				//append to prism list
				Prism p;
				ik[axis_a]=a, ik[axis_b]=b;
				p.x=ik[0], p.y=j, p.z=ik[1];
				ik[axis_a]=sz_a, ik[axis_b]=sz_b;
				p.w=ik[0], p.d=ik[1];
				prisms.push_back(p);
			}
		}
	}

	delete[] meshed;

	return prisms;
}
#endif