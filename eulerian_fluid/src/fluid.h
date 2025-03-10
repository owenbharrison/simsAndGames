#pragma once
#ifndef FLUID_CLASS_H
#define FLUID_CLASS_H

#include <string.h>

template<typename T>
const T& min(const T& a, const T& b) {
	return a<b?a:b;
}

template<typename T>
const T& max(const T& a, const T& b) {
	return a>b?a:b;
}

struct Fluid {
	size_t num_x=0, num_y=0;
	size_t num_cells=0;
	float spacing=0;
	float density=0;
	float* u=nullptr, * v=nullptr;
	float* new_u=nullptr, * new_v=nullptr;
	float* pressure=nullptr;
	bool* solid=nullptr;
	float* m=nullptr;
	float* new_m=nullptr;

	enum {
		U_FIELD=0,
		V_FIELD,
		S_FIELD
	};

	Fluid()=default;

	Fluid(size_t x, size_t y, float sp, float d) {
		//allow for border
		num_x=2+x, num_y=2+y;
		num_cells=num_x*num_y;
		spacing=sp;
		density=d;

		//velocity field
		u=new float[num_cells];
		v=new float[num_cells];
		new_u=new float[num_cells];
		new_v=new float[num_cells];

		pressure=new float[num_cells];
		solid=new bool[num_cells];

		//smoke
		m=new float[num_cells];
		new_m=new float[num_cells];

		//set defaults
		for(size_t i=0; i<num_cells; i++) {
			u[i]=0.f;
			v[i]=0.f;
			solid[i]=false;
			m[i]=1.f;
		}
	}

	void copyFrom(const Fluid& f) {
		num_x=f.num_x;
		num_y=f.num_y;
		num_cells=f.num_cells;
		spacing=f.spacing;
		density=f.density;

		u=new float[num_cells];
		v=new float[num_cells];
		new_u=new float[num_cells];
		new_v=new float[num_cells];
		pressure=new float[num_cells];
		solid=new bool[num_cells];
		m=new float[num_cells];
		new_m=new float[num_cells];

		memcpy(u, f.u, sizeof(float)*num_x*num_y);
		memcpy(v, f.v, sizeof(float)*num_x*num_y);
		memcpy(new_u, f.new_u, sizeof(float)*num_x*num_y);
		memcpy(new_v, f.new_v, sizeof(float)*num_x*num_y);
		memcpy(pressure, f.pressure, sizeof(float)*num_x*num_y);
		memcpy(solid, f.solid, sizeof(bool)*num_x*num_y);
		memcpy(m, f.m, sizeof(float)*num_x*num_y);
		memcpy(new_m, f.new_m, sizeof(float)*num_x*num_y);
	}

	Fluid(const Fluid& f) {
		copyFrom(f);
	}

	void clear() {
		delete[] u;
		delete[] v;
		delete[] new_u;
		delete[] new_v;
		delete[] pressure;
		delete[] solid;
		delete[] m;
		delete[] new_m;
	}

	~Fluid() {
		clear();
	}

	Fluid& operator=(const Fluid& f) {
		if(&f==this) return *this;

		clear();

		copyFrom(f);

		return *this;
	}

	//2d -> 1d flattening
	size_t ix(size_t i, size_t j) const {
		return i*num_y+j;
	}

	void solveIncompressibility(size_t num_iter, float dt) {
		//pressure coefficient
		float cp=density*spacing/dt;
		for(size_t i=0; i<num_cells; i++) {
			pressure[i]=0;
		}

		//for each cell
		for(size_t iter=0; iter<num_iter; iter++) {
			for(size_t i=1; i<num_x-1; i++) {
				for(size_t j=1; j<num_y-1; j++) {
					//skip solid cells
					if(solid[ix(i, j)]) continue;

					//which neighbors are "fluid"?
					bool sx0=!solid[ix(i-1, j)];
					bool sx1=!solid[ix(i+1, j)];
					bool sy0=!solid[ix(i, j-1)];
					bool sy1=!solid[ix(i, j+1)];
					size_t s=sx0+sx1+sy0+sy1;
					//if none "fluid", skip
					if(s==0) continue;

					//calculate divergence=total outflow
					float div=u[ix(i+1, j)]-u[ix(i, j)]+
						v[ix(i, j+1)]-v[ix(i, j)];

					float p=-div/s;
					//converges faster with 1<OR<2
					const float overrelaxation=1.9f;
					p*=overrelaxation;
					pressure[ix(i, j)]+=cp*p;

					//converge u, v such that div=0
					u[ix(i, j)]-=sx0*p;
					u[ix(i+1, j)]+=sx1*p;
					v[ix(i, j)]-=sy0*p;
					v[ix(i, j+1)]+=sy1*p;
				}
			}
		}
	}

	//set borders to neighbors
	void extrapolate() {
		for(size_t i=0; i<num_x; i++) {
			u[ix(i, 0)]=u[ix(i, 1)];//top
			u[ix(i, num_y-1)]=u[ix(i, num_y-2)];//bottom
		}
		for(size_t j=0; j<num_y; j++) {
			v[ix(0, j)]=v[ix(1, j)];//left
			v[ix(num_x-1, j)]=v[ix(num_x-2, j)];//right	
		}
	}

	float sampleField(float x, float y, size_t field) const {
		float h=spacing;
		float h1=1/h;
		float h2=h/2;

		//clamp query
		x=max(h, min(x, num_x*h));
		y=max(h, min(y, num_y*h));

		float dx=0, dy=0;

		//which field to sample?
		float* f=nullptr;
		switch(field) {
			case U_FIELD: f=u, dy=h2; break;
			case V_FIELD: f=v, dx=h2; break;
			case S_FIELD: f=m, dx=h2, dy=h2; break;
		}
		if(!f) return 0;

		//find four corners to interpolate
		size_t x0=min(size_t(h1*(x-dx)), num_x-1);
		size_t y0=min(size_t(h1*(y-dy)), num_y-1);
		size_t x1=min(x0+1, num_x-1);
		size_t y1=min(y0+1, num_y-1);

		//find interpolation factors
		float tx=h1*((x-dx)-h*x0);
		float ty=h1*((y-dy)-h*y0);
		float sx=1-tx;
		float sy=1-ty;

		//interpolate
		return sx*sy*f[ix(x0, y0)]+
			sx*ty*f[ix(x0, y1)]+
			tx*sy*f[ix(x1, y0)]+
			tx*ty*f[ix(x1, y1)];
	}

	//why j-1?
	float avgU(size_t i, size_t j) const {
		return (
			u[ix(i, j)]+
			u[ix(i, j-1)]+
			u[ix(i+1, j)]+
			u[ix(i+1, j-1)]
			)/4;
	}

	//why i-1?
	float avgV(size_t i, size_t j) const {
		return (
			v[ix(i, j)]+
			v[ix(i, j+1)]+
			v[ix(i-1, j)]+
			v[ix(i-1, j+1)]
			)/4;
	}

	void advectVel(float dt) {
		//store so as not to update as iterating
		memcpy(new_u, u, sizeof(float)*num_x*num_y);
		memcpy(new_v, v, sizeof(float)*num_x*num_y);

		float h=spacing;
		float h2=.5f*spacing;

		for(size_t i=1; i<num_x; i++) {
			for(size_t j=1; j<num_y; j++) {
				if(solid[ix(i, j)]) continue;

				if(!solid[ix(i-1, j)]&&j<num_y-1) {
					float x=h*i, y=h2+h*j;
					float u0=u[ix(i, j)];
					float v0=avgV(i, j);
					new_u[ix(i, j)]=sampleField(x-dt*u0, y-dt*v0, U_FIELD);
				}
				if(!solid[ix(i, j-1)]&&i<num_x-1) {
					float x=h2+h*i, y=h*j;
					float u0=avgU(i, j);
					float v0=v[ix(i, j)];
					new_v[ix(i, j)]=sampleField(x-dt*u0, y-dt*v0, V_FIELD);
				}
			}
		}

		//copy values over
		memcpy(u, new_u, sizeof(float)*num_x*num_y);
		memcpy(v, new_v, sizeof(float)*num_x*num_y);
	}

	void advectSmoke(float dt) {
		memcpy(new_m, m, sizeof(float)*num_x*num_y);

		float h=spacing;
		float h2=.5f*spacing;

		for(size_t i=1; i<num_x-1; i++) {
			for(size_t j=1; j<num_y-1; j++) {
				//skip solid cells
				if(solid[ix(i, j)]) continue;
				float u0=(u[ix(i, j)]+u[ix(i+1, j)])/2;
				float v0=(v[ix(i, j)]+u[ix(i, j+1)])/2;
				float x=h2+h*i-dt*u0;
				float y=h2+h*j-dt*v0;
				new_m[ix(i, j)]=sampleField(x, y, S_FIELD);
			}
		}

		memcpy(m, new_m, sizeof(float)*num_x*num_y);
	}
};
#endif