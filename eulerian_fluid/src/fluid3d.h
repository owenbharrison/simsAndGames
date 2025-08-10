//this is NOT rigorous. i just followed the patterns from the 2d version...
#pragma once
#ifndef FLUID3D_CLASS_H
#define FLUID3D_CLASS_H

#include <string.h>

template<typename T>
const T& min(const T& a, const T& b) {
	return a<b?a:b;
}

template<typename T>
const T& max(const T& a, const T& b) {
	return a>b?a:b;
}

struct Fluid3D {
	int num_x=0, num_y=0, num_z=0;
	int num_cells=0;
	float density=0;
	float h=0;
	float* u=nullptr, * v=nullptr, * w=nullptr;
	float* new_u=nullptr, * new_v=nullptr, * new_w=nullptr;
	float* pressure=nullptr;
	bool* solid=nullptr;
	float* m=nullptr;
	float* new_m=nullptr;

	enum {
		U_FIELD=0,
		V_FIELD,
		W_FIELD,
		S_FIELD
	};

	Fluid3D()=default;

	Fluid3D(int x, int y, int z, float d, float h_) {
		//allow for border
		num_x=2+x, num_y=2+y, num_z=2+z;
		num_cells=num_x*num_y*num_z;
		density=d;
		h=h_;

		//velocity field
		u=new float[num_cells];
		v=new float[num_cells];
		w=new float[num_cells];
		new_u=new float[num_cells];
		new_v=new float[num_cells];
		new_w=new float[num_cells];

		pressure=new float[num_cells];
		solid=new bool[num_cells];

		//smoke
		m=new float[num_cells];
		new_m=new float[num_cells];

		//set defaults
		for(int i=0; i<num_cells; i++) {
			u[i]=0.f, v[i]=0.f, w[i]=0.f;
			solid[i]=false;
			m[i]=1.f;
		}
	}

	void copyFrom(const Fluid3D& f) {
		num_x=f.num_x;
		num_y=f.num_y;
		num_z=f.num_z;
		num_cells=f.num_cells;
		density=f.density;
		h=f.h;

		u=new float[num_cells];
		v=new float[num_cells];
		w=new float[num_cells];
		new_u=new float[num_cells];
		new_v=new float[num_cells];
		new_w=new float[num_cells];
		pressure=new float[num_cells];
		solid=new bool[num_cells];
		m=new float[num_cells];
		new_m=new float[num_cells];

		memcpy(u, f.u, sizeof(float)*num_cells);
		memcpy(v, f.v, sizeof(float)*num_cells);
		memcpy(w, f.w, sizeof(float)*num_cells);
		memcpy(new_u, f.new_u, sizeof(float)*num_cells);
		memcpy(new_v, f.new_v, sizeof(float)*num_cells);
		memcpy(new_w, f.new_w, sizeof(float)*num_cells);
		memcpy(pressure, f.pressure, sizeof(float)*num_cells);
		memcpy(solid, f.solid, sizeof(bool)*num_cells);
		memcpy(m, f.m, sizeof(float)*num_cells);
		memcpy(new_m, f.new_m, sizeof(float)*num_cells);
	}

	Fluid3D(const Fluid3D& f) {
		copyFrom(f);
	}

	void clear() {
		delete[] u;
		delete[] v;
		delete[] w;
		delete[] new_u;
		delete[] new_v;
		delete[] new_w;
		delete[] pressure;
		delete[] solid;
		delete[] m;
		delete[] new_m;
	}

	~Fluid3D() {
		clear();
	}

	Fluid3D& operator=(const Fluid3D& f) {
		if(&f==this) return *this;

		clear();

		copyFrom(f);

		return *this;
	}

	//2d -> 1d flattening
	int ix(int i, int j, int k) const {
		return i+num_x*j+num_y*num_x*k;
	}

	void solveIncompressibility(int num_iter, float dt) {
		//pressure coefficient
		float cp=density*h/dt;
		for(int i=0; i<num_cells; i++) {
			pressure[i]=0.f;
		}

		//for each cell
		for(int iter=0; iter<num_iter; iter++) {
			for(int i=1; i<num_x-1; i++) {
				for(int j=1; j<num_y-1; j++) {
					for(int k=1; k<num_z-1; k++) {
					//skip solid cells
						if(solid[ix(i, j, k)]) continue;

						//which neighbors are "fluid"?
						bool sx0=!solid[ix(i-1, j, k)];
						bool sx1=!solid[ix(i+1, j, k)];
						bool sy0=!solid[ix(i, j-1, k)];
						bool sy1=!solid[ix(i, j+1, k)];
						bool sz0=!solid[ix(i, j, k-1)];
						bool sz1=!solid[ix(i, j, k+1)];
						int s=sx0+sx1+sy0+sy1+sz0+sz1;
						//if none "fluid", skip
						if(s==0) continue;

						//calculate divergence=total outflow
						float div=u[ix(i+1, j, k)]-u[ix(i, j, k)]+
							v[ix(i, j+1, k)]-v[ix(i, j, k)]+
							w[ix(i, j, k+1)]-w[ix(i, j, k)];

						float p=-div/s;
						//converges faster with 1<OR<2
						const float overrelaxation=1.9f;
						p*=overrelaxation;
						pressure[ix(i, j, k)]+=cp*p;

						//converge u, v such that div=0
						u[ix(i, j, k)]-=sx0*p;
						u[ix(i+1, j, k)]+=sx1*p;
						v[ix(i, j, k)]-=sy0*p;
						v[ix(i, j+1, k)]+=sy1*p;
						w[ix(i, j, k)]-=sz0*p;
						w[ix(i, j, k+1)]+=sz1*p;
					}
				}
			}
		}
	}

	//set borders to neighbors
	void extrapolate() {
		for(int i=0; i<num_x; i++) {
			for(int j=0; j<num_y; j++) {
				u[ix(i, j, 0)]=u[ix(i, j, 1)];
				u[ix(i, j, num_z-1)]=u[ix(i, j, num_z-2)];
				v[ix(i, j, 0)]=v[ix(i, j, 1)];
				v[ix(i, j, num_z-1)]=v[ix(i, j, num_z-2)];
			}
		}
		for(int j=0; j<num_y; j++) {
			for(int k=0; k<num_z; k++) {
				v[ix(0, j, k)]=v[ix(1, j, k)];
				v[ix(num_x-1, j, k)]=v[ix(num_x-2, j, k)];
				w[ix(0, j, k)]=w[ix(1, j, k)];
				w[ix(num_x-1, j, k)]=w[ix(num_x-2, j, k)];
			}
		}
		for(int k=0; k<num_z; k++) {
			for(int i=0; i<num_x; i++) {
				w[ix(i, 0, k)]=w[ix(i, 1, k)];
				w[ix(i, num_y-1, k)]=w[ix(i, num_y-2, k)];
				u[ix(i, 0, k)]=u[ix(i, 1, k)];
				u[ix(i, num_y-1, k)]=u[ix(i, num_y-2, k)];
			}
		}
	}

	float sampleField(float x, float y, float z, int field) const {
		float h1=1/h;
		float h2=h/2;

		//clamp query
		x=max(h, min(x, num_x*h));
		y=max(h, min(y, num_y*h));
		z=max(h, min(z, num_z*h));

		float dx=0.f, dy=0.f, dz=0.f;

		//which field to sample?
		float* f=nullptr;
		switch(field) {
			case U_FIELD: f=u, dy=h2; break;
			case V_FIELD: f=v, dx=h2; break;
			case W_FIELD: f=w, dz=h2; break;
			case S_FIELD: f=m, dx=h2, dy=h2; break;
		}
		if(!f) return 0.f;

		//find four corners to interpolate
		int x0=min(int(h1*(x-dx)), num_x-1);
		int y0=min(int(h1*(y-dy)), num_y-1);
		int z0=min(int(h1*(z-dz)), num_z-1);
		int x1=min(x0+1, num_x-1);
		int y1=min(y0+1, num_y-1);
		int z1=min(z0+1, num_z-1);

		//find interpolation factors
		float tx=h1*((x-dx)-h*x0);
		float ty=h1*((y-dy)-h*y0);
		float tz=h1*((z-dz)-h*z0);
		float sx=1-tx;
		float sy=1-ty;
		float sz=1-tz;

		//interpolate
		return sx*sy*sz*f[ix(x0, y0, z0)]+
			sx*sy*tz*f[ix(x0, y0, z1)]+
			sx*ty*sz*f[ix(x0, y1, z0)]+
			sx*ty*tz*f[ix(x0, y1, z1)]+
			tx*sy*sz*f[ix(x1, y0, z0)]+
			tx*sy*tz*f[ix(x1, y0, z1)]+
			tx*ty*sz*f[ix(x1, y1, z0)]+
			tx*ty*tz*f[ix(x1, y1, z1)];
	}

	float avgU(int i, int j, int k) const {
		return (
			u[ix(i, j, k)]+
			u[ix(i, j, k-1)]+
			u[ix(i, j-1, k)]+
			u[ix(i, j-1, k-1)]+
			u[ix(i+1, j, k)]+
			u[ix(i+1, j, k-1)]+
			u[ix(i+1, j-1, k)]+
			u[ix(i+1, j-1, k-1)]
			)/8;
	}

	float avgV(int i, int j, int k) const {
		return (
			v[ix(i, j, k)]+
			v[ix(i, j, k-1)]+
			v[ix(i-1, j, k)]+
			v[ix(i-1, j, k-1)]+
			v[ix(i, j+1, k)]+
			v[ix(i, j+1, k-1)]+
			v[ix(i-1, j+1, k)]+
			v[ix(i-1, j+1, k-1)]
			)/8;
	}

	float avgW(int i, int j, int k) const {
		return (
			w[ix(i, j, k)]+
			w[ix(i, j-1, k)]+
			w[ix(i-1, j, k)]+
			w[ix(i-1, j-1, k)]+
			w[ix(i, j, k+1)]+
			w[ix(i, j-1, k+1)]+
			w[ix(i-1, j, k+1)]+
			w[ix(i-1, j-1, k+1)]
			)/8;
	}

	void advectVel(float dt) {
		//store so as not to update as iterating
		memcpy(new_u, u, sizeof(float)*num_cells);
		memcpy(new_v, v, sizeof(float)*num_cells);
		memcpy(new_w, w, sizeof(float)*num_cells);

		float h2=h/2;

		for(int i=1; i<num_x; i++) {
			for(int j=1; j<num_y; j++) {
				for(int k=1; k<num_y; k++) {
					if(solid[ix(i, j, k)]) continue;

					if(!solid[ix(i-1, j, k)]&&j<num_y-1&&k<num_z-1) {
						float x=h*i, y=h2+h*j, z=h2+h*k;
						float u0=u[ix(i, j, k)];
						float v0=avgV(i, j, k);
						float w0=avgW(i, j, k);
						new_u[ix(i, j, k)]=sampleField(x-dt*u0, y-dt*v0, z-dt*w0, U_FIELD);
					}
					if(!solid[ix(i, j-1, k)]&&k<num_z-1&&i<num_x-1) {
						float x=h2+h*i, y=h*j, z=h2+h*k;
						float u0=avgU(i, j, k);
						float v0=v[ix(i, j, k)];
						float w0=avgW(i, j, k);
						new_v[ix(i, j, k)]=sampleField(x-dt*u0, y-dt*v0, z-dt*w0, V_FIELD);
					}
					if(!solid[ix(i, j, k-1)]&&i<num_x-1&&j<num_y-1) {
						float x=h2+h*i, y=h2+h*j, z=h*k;
						float u0=avgU(i, j, k);
						float v0=avgV(i, j, k);
						float w0=w[ix(i, j, k)];
						new_w[ix(i, j, k)]=sampleField(x-dt*u0, y-dt*v0, z-dt*w0, W_FIELD);
					}
				}
			}
		}

		//copy values over
		memcpy(u, new_u, sizeof(float)*num_cells);
		memcpy(v, new_v, sizeof(float)*num_cells);
		memcpy(w, new_w, sizeof(float)*num_cells);
	}

	void advectSmoke(float dt) {
		memcpy(new_m, m, sizeof(float)*num_cells);

		float h2=h/2;

		for(int i=1; i<num_x-1; i++) {
			for(int j=1; j<num_y-1; j++) {
				for(int k=1; k<num_z-1; k++) {
				//skip solid cells
					if(solid[ix(i, j, k)]) continue;

					float u0=(u[ix(i, j, k)]+u[ix(i+1, j, k)])/2;
					float v0=(v[ix(i, j, k)]+v[ix(i, j+1, k)])/2;
					float w0=(w[ix(i, j, k)]+w[ix(i, j, k+1)])/2;
					float x=h2+h*i-dt*u0;
					float y=h2+h*j-dt*v0;
					float z=h2+h*k-dt*w0;
					new_m[ix(i, j, k)]=sampleField(x, y, z, S_FIELD);
				}
			}
		}

		memcpy(m, new_m, sizeof(float)*num_cells);
	}
};
#endif