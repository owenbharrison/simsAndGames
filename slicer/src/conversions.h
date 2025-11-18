#pragma once
#ifndef CONVERSIONS_UTIL_H
#define CONVERSIONS_UTIL_H

#include "mesh.h"
#include "voxel_set.h"

//for memset & memcpy
#include <string>

VoxelSet meshToVoxels(const Mesh& m, vf3d res) {
	//get global mesh dimensions
	cmn::AABB3 m_box;
	for(const auto& v:m.verts) {
		m_box.fitToEnclose(v*m.mat_model);
	}

	//determine sizing
	vf3d m_dims=m_box.max-m_box.min;
	int width=1+m_dims.x/res.x;
	int height=1+m_dims.y/res.y;
	int depth=1+m_dims.z/res.z;

	//allocate
	VoxelSet v(width, height, depth);
	v.scale=res;
	v.offset=m_box.min;

	//for every voxel...
	for(int i=0; i<v.getWidth(); i++) {
		for(int j=0; j<v.getHeight(); j++) {
			for(int k=0; k<v.getDepth(); k++) {
				//is it in the polyhedra?
				vf3d pos=v.offset+v.scale*vf3d(.5f+i, .5f+j, .5f+k);
				if(m.contains(pos)) v.grid[v.ix(i, j, k)]=true;
			}
		}
	}

	return v;
}

Mesh voxelsToMesh(const VoxelSet& v) {
	//get sizing
	const int width=v.getWidth();
	const int height=v.getHeight();
	const int depth=v.getDepth();
	const int dims[3]{width, height, depth};

	Mesh m;
	m.offset=v.offset;
	m.scale=v.scale;

	//mesh vertex lookup
	int* lookup=new int[(1+width)*(1+height)*(1+depth)];
	std::memset(lookup, -1, sizeof(int)*(1+width)*(1+height)*(1+depth));
	auto lookupIX=[&] (int i, int j, int k) {
		return i+j*(1+width)+k*(1+width)*(1+height);
	};

	//vertex index getter
	auto getOrMakeVert=[&] (int x, int y, int z) {
		int& ix=lookup[lookupIX(x, y, z)];
		//not found
		if(ix==-1) {
			//make new & add to lookup
			ix=m.verts.size();
			m.verts.push_back(vf3d(x, y, z));
		}
		return ix;
	};

	//temporary indexing arrays
	int ijk[3], cijk[3];
	int v0[3], v1[3], v2[3], v3[3];

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

				//greedymeshing!
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
						v0[axis_a]=a;
						v0[axis_b]=b;
						v0[axis_c]=c;
						//face sides pushed along axes
						std::memcpy(v1, v0, sizeof(int)*3);
						v1[axis_b]+=sz_b;
						std::memcpy(v2, v0, sizeof(int)*3);
						v2[axis_b]+=sz_b, v2[axis_c]+=sz_c;
						std::memcpy(v3, v0, sizeof(int)*3);
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
						m.index_tris.push_back({v0_ix, v3_ix, v1_ix});
						m.index_tris.push_back({v2_ix, v1_ix, v3_ix});
					}
				}
			}
		}

		delete[] to_mesh;
		delete[] meshed;
	}

	delete[] lookup;

	m.updateMatrixes();
	m.updateTriangles();

	return m;
}
#endif