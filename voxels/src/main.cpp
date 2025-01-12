//impl BVH
//impl 3d bresenhams? 3body problem slicing?

#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "math/vf3d.h"
#include "math/mat4.h"

//vector-matrix multiplication
vf3d operator*(const vf3d& v, const Mat4& m) {
	vf3d r;
	r.x=v.x*m.v[0][0]+v.y*m.v[1][0]+v.z*m.v[2][0]+v.w*m.v[3][0];
	r.y=v.x*m.v[0][1]+v.y*m.v[1][1]+v.z*m.v[2][1]+v.w*m.v[3][1];
	r.z=v.x*m.v[0][2]+v.y*m.v[1][2]+v.z*m.v[2][2]+v.w*m.v[3][2];
	r.w=v.x*m.v[0][3]+v.y*m.v[1][3]+v.z*m.v[2][3]+v.w*m.v[3][3];
	return r;
}

//matrix-matrix multiplication
Mat4 operator*(const Mat4& a, const Mat4& b) {
	Mat4 m;
	for(int i=0; i<4; i++) {
		for(int j=0; j<4; j++) {
			float sum=0;
			for(int k=0; k<4; k++) sum+=a.v[i][k]*b.v[k][j];
			m.v[i][j]=sum;
		}
	}
	return m;
}

#include "gfx/triangle.h"
#include "gfx/mesh.h"

#include "voxel_set.h"

#pragma region CONVERSIONS
//HOW CAN I MAKE THIS SMALLER??
[[nodiscard]] Mesh voxelsToMesh(const VoxelSet& v) {
	Mesh m;

	const int w=v.getW(), h=v.getH(), d=v.getD();

	byte* to_mesh=nullptr;
	bool* meshed=nullptr;

#pragma region SLICE THRU X
	to_mesh=new byte[h*d];
	meshed=new bool[h*d];
	auto jk_ix=[h] (int j, int k) { return j+h*k; };

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
	auto ki_ix=[d] (int k, int i) { return k+d*i; };

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
	auto ij_ix=[w] (int i, int j) { return i+w*j; };

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

#include <functional>

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
#pragma endregion

vf3d reflect(const vf3d& in, const vf3d& norm) {
	return in-2*norm.dot(in)*norm;
}

struct VoxelGame : olc::PixelGameEngine {
	VoxelGame() {
		sAppName="Voxel Game";
	}

	Mesh mesh;
	Mat4 mat_proj;

	vf3d cam_prev, cam_curr, cam_acc;

	vf3d cam_pos;
	float cam_yaw=0;
	float cam_pitch=0;

	vf3d light_pos;
	bool show_outlines=false;

	std::list<VoxelSet*> objects;

	bool OnUserCreate() override {
		mat_proj=Mat4::makeProj(90.f, float(ScreenHeight())/ScreenWidth(), .1f, 1000.f);

		using namespace std::chrono;
		auto load_start=high_resolution_clock::now();
		Mesh dragon=Mesh::loadFromOBJ("res/mountains.txt");
		auto load_end=high_resolution_clock::now();
		dragon.normalize(100);
		auto load_dur=duration_cast<microseconds>(load_end-load_start);
		std::cout<<"load: "<<load_dur.count()<<"us\n";
		mesh=dragon;

		/*
		auto voxel_start=high_resolution_clock::now();
		VoxelSet v=meshToVoxels(dragon, .2f);
		auto voxel_end=high_resolution_clock::now();
		v.updateTypes();
		auto voxel_dur=duration_cast<microseconds>(voxel_end-voxel_start);
		std::cout<<"voxel: "<<voxel_dur.count()<<"us\n";

		auto mesh_start=high_resolution_clock::now();
		mesh=voxelsToMesh(v);
		auto mesh_end=high_resolution_clock::now();
		mesh.normalize(10);
		auto mesh_dur=duration_cast<microseconds>(mesh_end-mesh_start);
		std::cout<<"mesh: "<<mesh_dur.count()<<"us\n";
		*/

		light_pos=cam_pos;

		return true;
	}

	//how can we make this const?
	int clipAgainstPlane(Triangle& t, const vf3d& ctr, const vf3d& norm, Triangle& a, Triangle& b) {
		vf3d* inside_pts[3]; int inside_ct=0;
		vf3d* outside_pts[3]; int outside_ct=0;
		for(int i=0; i<3; i++) {
			//what side of plane is pt on?
			float dp=norm.dot(t.p[i]-ctr);
			if(dp>0) inside_pts[inside_ct++]=&t.p[i];
			else outside_pts[outside_ct++]=&t.p[i];
		}

		switch(inside_ct) {
			case 0:
				//tri behind plane
				return 0;
			case 1: {
				//form tri
				a.p[0]=*inside_pts[0];
				a.p[1]=segIntersectPlane(*inside_pts[0], *outside_pts[0], ctr, norm);
				a.p[2]=segIntersectPlane(*inside_pts[0], *outside_pts[1], ctr, norm);;
				a.col=t.col;
				return 1;
			}
			case 2: {
				//form quad
				a.p[0]=*inside_pts[0], a.p[1]=*inside_pts[1];
				a.p[2]=segIntersectPlane(*inside_pts[1], *outside_pts[0], ctr, norm);
				a.col=t.col;

				b.p[0]=a.p[0], b.p[1]=a.p[2];
				b.p[2]=segIntersectPlane(*outside_pts[0], a.p[0], ctr, norm);
				b.col=t.col;
				return 2;
			}
			case 3:
				//tri infront of plane
				a=t;
				return 1;
		}
	}

	void DrawTriangleDecal(const vf2d& a, const vf2d& b, const vf2d& c, olc::Pixel col=olc::WHITE) {
		DrawLineDecal(a, b, col);
		DrawLineDecal(b, c, col);
		DrawLineDecal(c, a, col);
	}

	bool OnUserUpdate(float dt) override {
		if(GetKey(olc::Key::CTRL).bHeld) dt/=10;

#pragma region MOVEMENT
		//add mouse later
		if(GetKey(olc::Key::R).bPressed) {
			cam_prev={0, 0, 0};
			cam_curr={0, 0, 0};
		}

		//look up,down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw+=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw-=dt;

		//jumping
		if(GetKey(olc::Key::SPACE).bHeld) {
			cam_prev.y=cam_curr.y-.1f;
		}

		//move forward, backward
		vf3d fb_dir(std::sinf(cam_yaw), 0, std::cosf(cam_yaw));
		if(GetKey(olc::Key::W).bHeld) cam_curr+=.05f*dt*fb_dir;
		if(GetKey(olc::Key::S).bHeld) cam_curr-=.03f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(olc::Key::A).bHeld) cam_curr+=.04f*dt*lr_dir;
		if(GetKey(olc::Key::D).bHeld) cam_curr-=.04f*dt*lr_dir;

		if(GetKey(olc::Key::ENTER).bHeld) light_pos=cam_curr;

		if(GetKey(olc::Key::O).bPressed) show_outlines^=true;

		if(GetKey(olc::Key::HOME).bPressed) {
			//math things
			//aabb things...
		}
#pragma endregion

#pragma region PHYSICS
		{
			//apply forces
			//gravity
			cam_acc+={0, -9.8, 0};

			//update position storing
			vf3d cam_vel=cam_curr-cam_prev;
			cam_prev=cam_curr;

			//verlet integration
			cam_curr+=cam_vel+cam_acc*dt*dt;

			//reset forces
			cam_acc={0, 0, 0};
		}
#pragma endregion

#pragma region COLLISION
		{
			//first find bounds of movement
			AABB3 seg_box;
			seg_box.fitToEnclose(cam_prev);
			seg_box.fitToEnclose(cam_curr);

			vf3d cam_vel=cam_curr-cam_prev;

			//check every tri in mesh
			for(const auto& tri:mesh.triangles) {
				//broad phase detection
				if(!seg_box.overlaps(tri.getAABB())) continue;

				//narrow phase detection
				float t=segIntersectTri(cam_prev, cam_curr, tri);
				if(t>0&&t<1) {
					//linearly interpolate to find point along segment
					vf3d ix=cam_prev+t*(cam_curr-cam_prev);

					//resolve
					//slightly above surface
					cam_curr=ix+vf3d(0, .001f, 0);
					cam_prev=cam_curr;
					//cam_curr-reflect(cam_vel, tri.getNorm());
					break;
				}
			}
		}
#pragma endregion

#pragma region GEOMETRY AND CLIPPING
		//elevate camera from feet
		cam_pos=cam_curr+vf3d(0, 1, 0);

		const vf3d up(0, 1, 0);
		vf3d look_dir(
			std::sinf(cam_yaw)*std::cosf(cam_pitch),
			std::sinf(cam_pitch),
			std::cosf(cam_yaw)*std::cosf(cam_pitch)
		);
		vf3d target=cam_pos+look_dir;

		Mat4 mat_cam=Mat4::makePointAt(cam_pos, target, up);
		Mat4 mat_view=Mat4::quickInverse(mat_cam);

		std::list<Triangle> tris_to_clip;
		for(const auto& t:mesh.triangles) {
			vf3d norm=t.getNorm();

			//is triangle pointing towards me? culling
			if(norm.dot(t.p[0]-cam_pos)<0) {
				//lighting
				vf3d light_dir=(light_pos-t.getCtr()).norm();
				float dp=std::max(.5f, norm.dot(light_dir));

				Triangle tri_view;
				for(int i=0; i<3; i++) {
					tri_view.p[i]=t.p[i]*mat_view;
				}
				tri_view.col=t.col*dp;

				//clip against near plane
				Triangle clipped[2];
				int num=clipAgainstPlane(tri_view, vf3d(0, 0, .1f), vf3d(0, 0, 1), clipped[0], clipped[1]);
				for(int j=0; j<num; j++) {
					Triangle tri_proj;
					for(int k=0; k<3; k++) {
						//project
						tri_proj.p[k]=clipped[j].p[k]*mat_proj;
						tri_proj.p[k]/=tri_proj.p[k].w;

						//x/y inverted so put them back
						tri_proj.p[k].x*=-1;
						tri_proj.p[k].y*=-1;

						//scale into view
						tri_proj.p[k]+=vf3d(1, 1, 0);
						tri_proj.p[k].x*=ScreenWidth()/2;
						tri_proj.p[k].y*=ScreenHeight()/2;
					}
					tri_proj.col=clipped[j].col;

					tris_to_clip.emplace_back(tri_proj);
				}
			}
		}

		//painters algorithm?
		tris_to_clip.sort([] (const Triangle& a, const Triangle& b) {
			float a_z=a.p[0].z+a.p[1].z+a.p[2].z;
			float b_z=b.p[0].z+b.p[1].z+b.p[2].z;
			return a_z>b_z;
		});

#pragma endregion

#pragma region RENDER
		Clear(olc::Pixel(50, 50, 50));

		//screen space clipping!
		//its easier?
		std::list<Triangle> tri_queue;
		for(const auto& t:tris_to_clip) {
			Triangle clipped[2];
			tri_queue={t};
			int num_new=1;
			for(int i=0; i<4; i++) {
				vf3d ctr, norm;
				switch(i) {
					case 0://left
						ctr=vf3d(0, 0, 0);
						norm=vf3d(1, 0, 0);
						break;
					case 1://right
						ctr=vf3d(ScreenWidth(), 0, 0);
						norm=vf3d(-1, 0, 0);
						break;
					case 2://top
						ctr=vf3d(0, 0, 0);
						norm=vf3d(0, 1, 0);
						break;
					case 3://bottom
						ctr=vf3d(0, ScreenHeight(), 0);
						norm=vf3d(0, -1, 0);
						break;
				}

				while(num_new>0) {
					Triangle test=tri_queue.front();
					tri_queue.pop_front();
					num_new--;

					int num_clip=clipAgainstPlane(test, ctr, norm, clipped[0], clipped[1]);
					for(int j=0; j<num_clip; j++) tri_queue.emplace_back(clipped[j]);
				}

				num_new=tri_queue.size();
			}

			//finally!
			for(const auto& t:tri_queue) {
				FillTriangleDecal(
					vf2d(t.p[0].x, t.p[0].y),
					vf2d(t.p[1].x, t.p[1].y),
					vf2d(t.p[2].x, t.p[2].y),
					t.col
				);
				if(show_outlines) {
					DrawTriangleDecal(
						vf2d(t.p[0].x, t.p[0].y),
						vf2d(t.p[1].x, t.p[1].y),
						vf2d(t.p[2].x, t.p[2].y),
						olc::BLACK
					);
				}
			}
		}

#pragma endregion

		return true;
	}
};

int main() {
	srand(time(0));

	VoxelGame vg;
	if(vg.Construct(320, 240, 2, 2, false, true)) vg.Start();

	return 0;
}