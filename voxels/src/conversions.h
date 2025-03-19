//this file houses the functions to convert between meshes and voxelsets
//i.e. shelling + greedymeshing
//and polyhedra pixelization

#include "gfx/mesh.h"
#include "voxel_set.h"

#include <thread>

float random(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=rand()/rand_max;
	return a+t*(b-a);
}

float segIntersectTri(const vf3d& s0, const vf3d& s1, const Triangle& tri) {
	/*segment equation
	s0+t(s1-s0)=p
	triangle equation
	t0+u(t1-t0)+v(t2-t0)
	set equal
	s0+t(s1-s0)=t0+u(t1-t0)+v(t2-t0)
	rearrange
	t(s1-s0)+u(t0-t1)+v(t0-t2)=t0-s0
	solve like matrix!
	*/
	static const float epsilon=1e-6f;
	//column vectors
	vf3d a=s1-s0;
	vf3d b=tri.p[0]-tri.p[1];
	vf3d c=tri.p[0]-tri.p[2];
	vf3d d=tri.p[0]-s0;
	float det=a.x*(b.y*c.z-c.y*b.z)
		-b.x*(a.y*c.z-c.y*a.z)
		+c.x*(a.y*b.z-b.y*a.z);
	//parallel
	if(std::abs(det)<epsilon) return -1;

	//row vectors
	vf3d f=vf3d(
		a.z*c.y-c.z*a.y,
		a.x*c.z-c.x*a.z,
		a.y*c.x-c.y*a.x
	)/det;
	float u=f.dot(d);
	//out of range
	if(u<0||u>1) return -1;

	vf3d g=vf3d(
		a.y*b.z-b.y*a.z,
		a.z*b.x-b.z*a.x,
		a.x*b.y-b.x*a.y
	)/det;
	float v=g.dot(d);
	//out of range
	if(v<0||v>1) return -1;

	//barycentric uv coordinates
	if(u+v>1) return -1;

	vf3d e=vf3d(
		b.y*c.z-c.y*b.z,
		b.z*c.x-c.z*b.x,
		b.x*c.y-c.x*b.y
	)/det;
	return e.dot(d);
}

[[nodiscard]] VoxelSet meshToVoxels(const Mesh& m, float resolution) {
	//find mesh bounds
	AABB3 box=m.getAABB();

	//determine voxelset sizing
	vf3d size=(box.max-box.min)/resolution;
	int w=1+size.x, h=1+size.y, d=1+size.z;

	VoxelSet v(w, h, d);
	v.offset=box.min;
	v.scale=resolution;

	//for every voxel, is it in the polyhedra?
	int num=w*h*d;
	for(int i=0, n=0; i<w; i++) {
		for(int j=0; j<h; j++) {
			for(int k=0; k<d; k++, n++) {
				//polyhedra raycasting algorithm
				vf3d xyz=v.offset+v.scale*vf3d(.5f+i, .5f+j, .5f+k);
				vf3d dir=vf3d(random(-1, 1), random(-1, 1), random(-1, 1)).norm();

				//for every tri
				int num_ix=0;
				for(const auto& tri:m.triangles) {
					float t=segIntersectTri(xyz, xyz+dir, tri);
					if(t>0) num_ix++;
				}

				//odd? inside!
				v(i, j, k)=num_ix%2;
			}
		}
	}

	return v;
}

[[nodiscard]] VoxelSet meshToVoxels_fast(const Mesh& m, float resolution) {
	//find mesh bounds
	AABB3 box=m.getAABB();

	//determine voxelset sizing
	vf3d size=(box.max-box.min)/resolution;
	int w=1+size.x, h=1+size.y, d=1+size.z;

	VoxelSet v(w, h, d);
	v.offset=box.min;
	v.scale=resolution;

	int num_thr=std::thread::hardware_concurrency();
	std::thread* workers=new std::thread[num_thr];
	for(int gr=0; gr<num_thr; gr++) {
		workers[gr]=std::thread([&v, num_thr, gr, &m]() {
			//for every voxel, is it in the polyhedra?
			for(int i=0; i<v.getW(); i++) {
				for(int j=0; j<v.getH(); j++) {
					for(int k=0; k<v.getD(); k++) {
						int l=v.ix(i, j, k);
						if(l%num_thr!=gr) continue;

						//polyhedra raycasting algorithm
						vf3d xyz=v.offset+v.scale*vf3d(.5f+i, .5f+j, .5f+k);
						vf3d dir=vf3d(random(-1, 1), random(-1, 1), random(-1, 1)).norm();

						//for every tri
						int num_ix=0;
						for(const auto& tri:m.triangles) {
							float t=segIntersectTri(xyz, xyz+dir, tri);
							if(t>0) num_ix++;
						}

						//odd? inside!
						v.grid[l]=num_ix%2;
					}
				}
			}
		});
	}

	for(int i=0; i<8; i++) {
		workers[i].join();
	}

	delete[] workers;

	return v;
}

[[nodiscard]] Mesh voxelsToMesh(const VoxelSet& v) {
	Mesh m;

	const int w=v.getW(), h=v.getH(), d=v.getD();

	bool* to_mesh=nullptr;
	bool* meshed=nullptr;

#pragma region SLICE THRU X
	to_mesh=new bool[h*d];
	meshed=new bool[h*d];
	auto jk_ix=[h](int j, int k) { return j+h*k; };

	for(int i=0; i<w; i++) {
		//which faces should we consider?
		memset(to_mesh, false, sizeof(bool)*h*d);
		for(int j=0; j<h; j++) {
			for(int k=0; k<d; k++) {
				//dont mesh air
				if(v(i, j, k)==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(i==0||v(i-1, j, k)==VoxelSet::Empty) to_mesh[jk_ix(j, k)]=true;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*h*d);
		for(int j=0; j<h; j++) {
			for(int k=0; k<d; k++) {
				if(!to_mesh[jk_ix(j, k)]) continue;

				//dont remesh
				if(meshed[jk_ix(j, k)]) continue;

				//combine single
				int ext_h=1;
				for(int j_=1+j; j_<h; j_++, ext_h++) {
					if(meshed[jk_ix(j_, k)]||!to_mesh[jk_ix(j_, k)]) break;
				}

				//combine entire
				int ext_d=1;
				for(int k_=1+k; k_<d; k_++, ext_d++) {
					bool able=true;
					for(int j_=0; j_<ext_h; j_++) {
						if(meshed[jk_ix(j+j_, k_)]||!to_mesh[jk_ix(j+j_, k_)]) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int j_=0; j_<ext_h; j_++) {
					for(int k_=0; k_<ext_d; k_++) {
						meshed[jk_ix(j+j_, k+k_)]=true;
					}
				}

				//triangulate
				vf3d v0(i, j, k);
				vf3d va(0, ext_h, 0);
				vf3d vb(0, 0, ext_d);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				m.triangles.push_back({v0, v2, v1});
				m.triangles.push_back({v0, v3, v2});
			}
		}

		//other side

		//which faces should we consider?
		memset(to_mesh, false, sizeof(bool)*h*d);
		for(int j=0; j<h; j++) {
			for(int k=0; k<d; k++) {
				//dont mesh air
				if(v(i, j, k)==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(i==w-1||v(i+1, j, k)==VoxelSet::Empty) to_mesh[jk_ix(j, k)]=true;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*h*d);
		for(int j=0; j<h; j++) {
			for(int k=0; k<d; k++) {
				if(!to_mesh[jk_ix(j, k)]) continue;

				//dont remesh
				if(meshed[jk_ix(j, k)]) continue;

				//combine single
				int ext_h=1;
				for(int j_=1+j; j_<h; j_++, ext_h++) {
					if(meshed[jk_ix(j_, k)]||!to_mesh[jk_ix(j_, k)]) break;
				}

				//combine entire
				int ext_d=1;
				for(int k_=1+k; k_<d; k_++, ext_d++) {
					bool able=true;
					for(int j_=0; j_<ext_h; j_++) {
						if(meshed[jk_ix(j+j_, k_)]||!to_mesh[jk_ix(j+j_, k_)]) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int j_=0; j_<ext_h; j_++) {
					for(int k_=0; k_<ext_d; k_++) {
						meshed[jk_ix(j+j_, k+k_)]=true;
					}
				}

				//triangulate
				vf3d v0(i+1, j, k);
				vf3d va(0, ext_h, 0);
				vf3d vb(0, 0, ext_d);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				m.triangles.push_back({v0, v1, v2});
				m.triangles.push_back({v0, v2, v3});
			}
		}
	}

	delete[] to_mesh;
	delete[] meshed;
#pragma endregion

#pragma region SLICE THRU Y
	to_mesh=new bool[d*w];
	meshed=new bool[d*w];
	auto ki_ix=[d](int k, int i) { return k+d*i; };

	for(int j=0; j<h; j++) {
		//which faces should we consider?
		memset(to_mesh, false, sizeof(bool)*d*w);
		for(int k=0; k<d; k++) {
			for(int i=0; i<w; i++) {
				//dont mesh air
				if(v(i, j, k)==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(j==0||v(i, j-1, k)==VoxelSet::Empty) to_mesh[ki_ix(k, i)]=true;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*d*w);
		for(int k=0; k<d; k++) {
			for(int i=0; i<w; i++) {
				if(!to_mesh[ki_ix(k, i)]) continue;

				//dont remesh
				if(meshed[ki_ix(k, i)]) continue;

				//combine single
				int ext_d=1;
				for(int k_=1+k; k_<d; k_++, ext_d++) {
					if(meshed[ki_ix(k_, i)]||!to_mesh[ki_ix(k_, i)]) break;
				}

				//combine entire
				int ext_w=1;
				for(int i_=1+i; i_<w; i_++, ext_w++) {
					bool able=true;
					for(int k_=0; k_<ext_d; k_++) {
						if(meshed[ki_ix(k+k_, i_)]||!to_mesh[ki_ix(k+k_, i_)]) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int k_=0; k_<ext_d; k_++) {
					for(int i_=0; i_<ext_w; i_++) {
						meshed[ki_ix(k+k_, i+i_)]=true;
					}
				}

				//triangulate
				vf3d v0(i, j, k);
				vf3d va(0, 0, ext_d);
				vf3d vb(ext_w, 0, 0);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				m.triangles.push_back({v0, v2, v1});
				m.triangles.push_back({v0, v3, v2});
			}
		}

		//other side

		//which faces should we consider?
		memset(to_mesh, false, sizeof(bool)*d*w);
		for(int k=0; k<d; k++) {
			for(int i=0; i<w; i++) {
				//dont mesh air
				if(v(i, j, k)==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(j==h-1||v(i, j+1, k)==VoxelSet::Empty) to_mesh[ki_ix(k, i)]=true;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*d*w);
		for(int k=0; k<d; k++) {
			for(int i=0; i<w; i++) {
				if(!to_mesh[ki_ix(k, i)]) continue;

				//dont remesh
				if(meshed[ki_ix(k, i)]) continue;

				//combine single
				int ext_d=1;
				for(int k_=1+k; k_<d; k_++, ext_d++) {
					if(meshed[ki_ix(k_, i)]||!to_mesh[ki_ix(k_, i)]) break;
				}

				//combine entire
				int ext_w=1;
				for(int i_=1+i; i_<w; i_++, ext_w++) {
					bool able=true;
					for(int k_=0; k_<ext_d; k_++) {
						if(meshed[ki_ix(k+k_, i_)]||!to_mesh[ki_ix(k+k_, i_)]) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int k_=0; k_<ext_d; k_++) {
					for(int i_=0; i_<ext_w; i_++) {
						meshed[ki_ix(k+k_, i+i_)]=true;
					}
				}

				//triangulate
				vf3d v0(i, j+1, k);
				vf3d va(0, 0, ext_d);
				vf3d vb(ext_w, 0, 0);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				m.triangles.push_back({v0, v1, v2});
				m.triangles.push_back({v0, v2, v3});
			}
		}
	}

	delete[] to_mesh;
	delete[] meshed;
#pragma endregion

#pragma region SLICE THRU Z
	to_mesh=new bool[w*h];
	meshed=new bool[w*h];
	auto ij_ix=[w](int i, int j) { return i+w*j; };

	for(int k=0; k<d; k++) {
		//which faces should we consider?
		memset(to_mesh, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont mesh air
				if(v(i, j, k)==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(k==0||v(i, j, k-1)==VoxelSet::Empty) to_mesh[ij_ix(i, j)]=true;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				if(!to_mesh[ij_ix(i, j)]) continue;

				//dont remesh
				if(meshed[ij_ix(i, j)]) continue;

				//combine single
				int ext_w=1;
				for(int i_=1+i; i_<w; i_++, ext_w++) {
					if(meshed[ij_ix(i_, j)]||!to_mesh[ij_ix(i_, j)]) break;
				}

				//combine entire
				int ext_h=1;
				for(int j_=1+j; j_<h; j_++, ext_h++) {
					bool able=true;
					for(int i_=0; i_<ext_w; i_++) {
						if(meshed[ij_ix(i+i_, j_)]||!to_mesh[ij_ix(i+i_, j_)]) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int i_=0; i_<ext_w; i_++) {
					for(int j_=0; j_<ext_h; j_++) {
						meshed[ij_ix(i+i_, j+j_)]=true;
					}
				}

				//triangulate
				vf3d v0(i, j, k);
				vf3d va(ext_w, 0, 0);
				vf3d vb(0, ext_h, 0);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				m.triangles.push_back({v0, v2, v1});
				m.triangles.push_back({v0, v3, v2});
			}
		}

		//other side

		//which faces should we consider?
		memset(to_mesh, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				//dont mesh air
				if(v(i, j, k)==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(k==d-1||v(i, j, k+1)==VoxelSet::Empty) to_mesh[ij_ix(i, j)]=true;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				if(!to_mesh[ij_ix(i, j)]) continue;

				//dont remesh
				if(meshed[ij_ix(i, j)]) continue;

				//combine single
				int ext_w=1;
				for(int i_=1+i; i_<w; i_++, ext_w++) {
					if(meshed[ij_ix(i_, j)]||!to_mesh[ij_ix(i_, j)]) break;
				}

				//combine entire
				int ext_h=1;
				for(int j_=1+j; j_<h; j_++, ext_h++) {
					bool able=true;
					for(int i_=0; i_<ext_w; i_++) {
						if(meshed[ij_ix(i+i_, j_)]||!to_mesh[ij_ix(i+i_, j_)]) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int i_=0; i_<ext_w; i_++) {
					for(int j_=0; j_<ext_h; j_++) {
						meshed[ij_ix(i+i_, j+j_)]=true;
					}
				}

				//triangulate
				vf3d v0(i, j, k+1);
				vf3d va(ext_w, 0, 0);
				vf3d vb(0, ext_h, 0);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				m.triangles.push_back({v0, v1, v2});
				m.triangles.push_back({v0, v2, v3});
			}
		}
	}

	delete[] to_mesh;
	delete[] meshed;
#pragma endregion

	//transform and scale
	for(auto& t:m.triangles) {
		for(int i=0; i<3; i++) {
			auto& p=t.p[i];
			p=v.offset+v.scale*p;
		}
	}

	return m;
}

//HOW CAN I MAKE THIS SMALLER??
[[nodiscard]] Mesh voxelsToMesh_types(const VoxelSet& v) {
	Mesh m;

	const int w=v.getW(), h=v.getH(), d=v.getD();

	byte* to_mesh=nullptr;
	bool* meshed=nullptr;

#pragma region SLICE THRU X
	to_mesh=new byte[h*d];
	meshed=new bool[h*d];
	auto jk_ix=[h](int j, int k) { return j+h*k; };

	for(int i=0; i<w; i++) {
		//which faces should we consider?
		memset(to_mesh, VoxelSet::Empty, sizeof(byte)*h*d);
		for(int j=0; j<h; j++) {
			for(int k=0; k<d; k++) {
				const auto& curr=v(i, j, k);
				//dont mesh air
				if(curr==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(i==0||v(i-1, j, k)==VoxelSet::Empty) to_mesh[jk_ix(j, k)]=curr;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*h*d);
		for(int j=0; j<h; j++) {
			for(int k=0; k<d; k++) {
				const auto& curr=to_mesh[jk_ix(j, k)];
				if(curr==VoxelSet::Empty) continue;

				//dont remesh
				if(meshed[jk_ix(j, k)]) continue;

				//combine single
				int ext_h=1;
				for(int j_=1+j; j_<h; j_++, ext_h++) {
					if(meshed[jk_ix(j_, k)]||to_mesh[jk_ix(j_, k)]!=curr) break;
				}

				//combine entire
				int ext_d=1;
				for(int k_=1+k; k_<d; k_++, ext_d++) {
					bool able=true;
					for(int j_=0; j_<ext_h; j_++) {
						if(meshed[jk_ix(j+j_, k_)]||to_mesh[jk_ix(j+j_, k_)]!=curr) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int j_=0; j_<ext_h; j_++) {
					for(int k_=0; k_<ext_d; k_++) {
						meshed[jk_ix(j+j_, k+k_)]=true;
					}
				}

				//triangulate
				vf3d v0(i, j, k);
				vf3d va(0, ext_h, 0);
				vf3d vb(0, 0, ext_d);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				olc::Pixel col;
				switch(curr) {
					case VoxelSet::Surface: col=olc::GREEN; break;
					case VoxelSet::Edge: col=olc::BLUE; break;
					case VoxelSet::Corner: col=olc::RED; break;
				}
				m.triangles.push_back({v0, v2, v1, col});
				m.triangles.push_back({v0, v3, v2, col});
			}
		}

		//other side

		//which faces should we consider?
		memset(to_mesh, VoxelSet::Empty, sizeof(byte)*h*d);
		for(int j=0; j<h; j++) {
			for(int k=0; k<d; k++) {
				const auto& curr=v(i, j, k);
				//dont mesh air
				if(curr==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(i==w-1||v(i+1, j, k)==VoxelSet::Empty) to_mesh[jk_ix(j, k)]=curr;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*h*d);
		for(int j=0; j<h; j++) {
			for(int k=0; k<d; k++) {
				const auto& curr=to_mesh[jk_ix(j, k)];
				if(curr==VoxelSet::Empty) continue;

				//dont remesh
				if(meshed[jk_ix(j, k)]) continue;

				//combine single
				int ext_h=1;
				for(int j_=1+j; j_<h; j_++, ext_h++) {
					if(meshed[jk_ix(j_, k)]||to_mesh[jk_ix(j_, k)]!=curr) break;
				}

				//combine entire
				int ext_d=1;
				for(int k_=1+k; k_<d; k_++, ext_d++) {
					bool able=true;
					for(int j_=0; j_<ext_h; j_++) {
						if(meshed[jk_ix(j+j_, k_)]||to_mesh[jk_ix(j+j_, k_)]!=curr) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int j_=0; j_<ext_h; j_++) {
					for(int k_=0; k_<ext_d; k_++) {
						meshed[jk_ix(j+j_, k+k_)]=true;
					}
				}

				//triangulate
				vf3d v0(i+1, j, k);
				vf3d va(0, ext_h, 0);
				vf3d vb(0, 0, ext_d);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				olc::Pixel col;
				switch(curr) {
					case VoxelSet::Surface: col=olc::GREEN; break;
					case VoxelSet::Edge: col=olc::BLUE; break;
					case VoxelSet::Corner: col=olc::RED; break;
				}
				m.triangles.push_back({v0, v1, v2, col});
				m.triangles.push_back({v0, v2, v3, col});
			}
		}
	}

	delete[] to_mesh;
	delete[] meshed;
#pragma endregion

#pragma region SLICE THRU Y
	to_mesh=new byte[d*w];
	meshed=new bool[d*w];
	auto ki_ix=[d](int k, int i) { return k+d*i; };

	for(int j=0; j<h; j++) {
		//which faces should we consider?
		memset(to_mesh, VoxelSet::Empty, sizeof(byte)*d*w);
		for(int k=0; k<d; k++) {
			for(int i=0; i<w; i++) {
				const auto& curr=v(i, j, k);
				//dont mesh air
				if(curr==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(j==0||v(i, j-1, k)==VoxelSet::Empty) to_mesh[ki_ix(k, i)]=curr;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*d*w);
		for(int k=0; k<d; k++) {
			for(int i=0; i<w; i++) {
				const auto& curr=to_mesh[ki_ix(k, i)];
				if(curr==VoxelSet::Empty) continue;

				//dont remesh
				if(meshed[ki_ix(k, i)]) continue;

				//combine single
				int ext_d=1;
				for(int k_=1+k; k_<d; k_++, ext_d++) {
					if(meshed[ki_ix(k_, i)]||to_mesh[ki_ix(k_, i)]!=curr) break;
				}

				//combine entire
				int ext_w=1;
				for(int i_=1+i; i_<w; i_++, ext_w++) {
					bool able=true;
					for(int k_=0; k_<ext_d; k_++) {
						if(meshed[ki_ix(k+k_, i_)]||to_mesh[ki_ix(k+k_, i_)]!=curr) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int k_=0; k_<ext_d; k_++) {
					for(int i_=0; i_<ext_w; i_++) {
						meshed[ki_ix(k+k_, i+i_)]=true;
					}
				}

				//triangulate
				vf3d v0(i, j, k);
				vf3d va(0, 0, ext_d);
				vf3d vb(ext_w, 0, 0);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				olc::Pixel col;
				switch(curr) {
					case VoxelSet::Surface: col=olc::GREEN; break;
					case VoxelSet::Edge: col=olc::BLUE; break;
					case VoxelSet::Corner: col=olc::RED; break;
				}
				m.triangles.push_back({v0, v2, v1, col});
				m.triangles.push_back({v0, v3, v2, col});
			}
		}

		//other side

		//which faces should we consider?
		memset(to_mesh, VoxelSet::Empty, sizeof(bool)*d*w);
		for(int k=0; k<d; k++) {
			for(int i=0; i<w; i++) {
				const auto& curr=v(i, j, k);
				//dont mesh air
				if(curr==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(j==h-1||v(i, j+1, k)==VoxelSet::Empty) to_mesh[ki_ix(k, i)]=curr;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*d*w);
		for(int k=0; k<d; k++) {
			for(int i=0; i<w; i++) {
				const auto& curr=to_mesh[ki_ix(k, i)];
				if(curr==VoxelSet::Empty) continue;

				//dont remesh
				if(meshed[ki_ix(k, i)]) continue;

				//combine single
				int ext_d=1;
				for(int k_=1+k; k_<d; k_++, ext_d++) {
					if(meshed[ki_ix(k_, i)]||to_mesh[ki_ix(k_, i)]!=curr) break;
				}

				//combine entire
				int ext_w=1;
				for(int i_=1+i; i_<w; i_++, ext_w++) {
					bool able=true;
					for(int k_=0; k_<ext_d; k_++) {
						if(meshed[ki_ix(k+k_, i_)]||to_mesh[ki_ix(k+k_, i_)]!=curr) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int k_=0; k_<ext_d; k_++) {
					for(int i_=0; i_<ext_w; i_++) {
						meshed[ki_ix(k+k_, i+i_)]=true;
					}
				}

				//triangulate
				vf3d v0(i, j+1, k);
				vf3d va(0, 0, ext_d);
				vf3d vb(ext_w, 0, 0);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				olc::Pixel col;
				switch(curr) {
					case VoxelSet::Surface: col=olc::GREEN; break;
					case VoxelSet::Edge: col=olc::BLUE; break;
					case VoxelSet::Corner: col=olc::RED; break;
				}
				m.triangles.push_back({v0, v1, v2, col});
				m.triangles.push_back({v0, v2, v3, col});
			}
		}
	}

	delete[] to_mesh;
	delete[] meshed;
#pragma endregion

#pragma region SLICE THRU Z
	to_mesh=new byte[w*h];
	meshed=new bool[w*h];
	auto ij_ix=[w](int i, int j) { return i+w*j; };

	for(int k=0; k<d; k++) {
		//which faces should we consider?
		memset(to_mesh, VoxelSet::Empty, sizeof(byte)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				const auto& curr=v(i, j, k);
				//dont mesh air
				if(curr==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(k==0||v(i, j, k-1)==VoxelSet::Empty) to_mesh[ij_ix(i, j)]=curr;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				const auto& curr=to_mesh[ij_ix(i, j)];
				if(curr==VoxelSet::Empty) continue;

				//dont remesh
				if(meshed[ij_ix(i, j)]) continue;

				//combine single
				int ext_w=1;
				for(int i_=1+i; i_<w; i_++, ext_w++) {
					if(meshed[ij_ix(i_, j)]||to_mesh[ij_ix(i_, j)]!=curr) break;
				}

				//combine entire
				int ext_h=1;
				for(int j_=1+j; j_<h; j_++, ext_h++) {
					bool able=true;
					for(int i_=0; i_<ext_w; i_++) {
						if(meshed[ij_ix(i+i_, j_)]||to_mesh[ij_ix(i+i_, j_)]!=curr) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int i_=0; i_<ext_w; i_++) {
					for(int j_=0; j_<ext_h; j_++) {
						meshed[ij_ix(i+i_, j+j_)]=true;
					}
				}

				//triangulate
				vf3d v0(i, j, k);
				vf3d va(ext_w, 0, 0);
				vf3d vb(0, ext_h, 0);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				olc::Pixel col;
				switch(curr) {
					case VoxelSet::Surface: col=olc::GREEN; break;
					case VoxelSet::Edge: col=olc::BLUE; break;
					case VoxelSet::Corner: col=olc::RED; break;
				}
				m.triangles.push_back({v0, v2, v1, col});
				m.triangles.push_back({v0, v3, v2, col});
			}
		}

		//other side

		//which faces should we consider?
		memset(to_mesh, VoxelSet::Empty, sizeof(byte)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				const auto& curr=v(i, j, k);
				//dont mesh air
				if(curr==VoxelSet::Empty) continue;

				//place faces on edges or boundary
				if(k==d-1||v(i, j, k+1)==VoxelSet::Empty) to_mesh[ij_ix(i, j)]=curr;
			}
		}

		//greedy meshing
		memset(meshed, false, sizeof(bool)*w*h);
		for(int i=0; i<w; i++) {
			for(int j=0; j<h; j++) {
				const auto& curr=to_mesh[ij_ix(i, j)];
				if(curr==VoxelSet::Empty) continue;

				//dont remesh
				if(meshed[ij_ix(i, j)]) continue;

				//combine single
				int ext_w=1;
				for(int i_=1+i; i_<w; i_++, ext_w++) {
					if(meshed[ij_ix(i_, j)]||to_mesh[ij_ix(i_, j)]!=curr) break;
				}

				//combine entire
				int ext_h=1;
				for(int j_=1+j; j_<h; j_++, ext_h++) {
					bool able=true;
					for(int i_=0; i_<ext_w; i_++) {
						if(meshed[ij_ix(i+i_, j_)]||to_mesh[ij_ix(i+i_, j_)]!=curr) {
							able=false;
							break;
						}
					}
					if(!able) break;
				}

				//set meshed
				for(int i_=0; i_<ext_w; i_++) {
					for(int j_=0; j_<ext_h; j_++) {
						meshed[ij_ix(i+i_, j+j_)]=true;
					}
				}

				//triangulate
				vf3d v0(i, j, k+1);
				vf3d va(ext_w, 0, 0);
				vf3d vb(0, ext_h, 0);
				vf3d v1=v0+va, v3=v0+vb;
				vf3d v2=v0+va+vb;
				olc::Pixel col;
				switch(curr) {
					case VoxelSet::Surface: col=olc::GREEN; break;
					case VoxelSet::Edge: col=olc::BLUE; break;
					case VoxelSet::Corner: col=olc::RED; break;
				}
				m.triangles.push_back({v0, v1, v2, col});
				m.triangles.push_back({v0, v2, v3, col});
			}
		}
	}

	delete[] to_mesh;
	delete[] meshed;
#pragma endregion

	//transform and scale
	for(auto& t:m.triangles) {
		for(int i=0; i<3; i++) {
			auto& p=t.p[i];
			p=v.offset+v.scale*p;
		}
	}

	return m;
}