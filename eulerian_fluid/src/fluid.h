//thanks matthias muller
//https://www.youtube.com/watch?v=iKAVRgIrUOU
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

class Fluid {
	int num_x=0, num_y=0;
	int num_cells=0;

public:
	float density=0;
	float h=0, h1=0;
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

	Fluid(int x, int y, float d, float h_) {
		//allow for border
		num_x=2+x, num_y=2+y;
		num_cells=num_x*num_y;
		density=d;
		h=h_;
		h1=1/h;

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
		for(int i=0; i<num_cells; i++) {
			u[i]=0.f;
			v[i]=0.f;
			solid[i]=false;
			m[i]=1.f;
		}
	}

	//1
	Fluid(const Fluid& f)=delete;

	//2
	~Fluid() {
		delete[] u;
		delete[] v;
		delete[] new_u;
		delete[] new_v;
		delete[] pressure;
		delete[] solid;
		delete[] m;
		delete[] new_m;
	}

	//3
	Fluid& operator=(const Fluid& f)=delete;

	int getNumX() const { return num_x; }
	int getNumY() const { return num_y; }
	int getNumCells() const { return num_cells; }

	//2d -> 1d flattening
	int ix(int i, int j) const {
		return i*num_y+j;
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
					//skip solid cells
					if(solid[ix(i, j)]) continue;

					//which neighbors are "fluid"?
					bool sx0=!solid[ix(i-1, j)];
					bool sx1=!solid[ix(i+1, j)];
					bool sy0=!solid[ix(i, j-1)];
					bool sy1=!solid[ix(i, j+1)];
					int s=sx0+sx1+sy0+sy1;
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
		for(int i=0; i<num_x; i++) {
			u[ix(i, 0)]=u[ix(i, 1)];//top
			u[ix(i, num_y-1)]=u[ix(i, num_y-2)];//bottom
		}
		for(int j=0; j<num_y; j++) {
			v[ix(0, j)]=v[ix(1, j)];//left
			v[ix(num_x-1, j)]=v[ix(num_x-2, j)];//right	
		}
	}

	float sampleField(float x, float y, int field) const {
		//clamp query
		x=max(h, min(x, num_x*h));
		y=max(h, min(y, num_y*h));

		float dx=0.f, dy=0.f;

		float h2=h/2;

		//which field to sample?
		float* f=nullptr;
		switch(field) {
			case U_FIELD: f=u, dy=h2; break;
			case V_FIELD: f=v, dx=h2; break;
			case S_FIELD: f=m, dx=h2, dy=h2; break;
		}
		if(!f) return 0.f;

		//find four corners to interpolate
		int x0=min(int(h1*(x-dx)), num_x-1);
		int y0=min(int(h1*(y-dy)), num_y-1);
		int x1=min(x0+1, num_x-1);
		int y1=min(y0+1, num_y-1);

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
	float avgU(int i, int j) const {
		return (
			u[ix(i, j)]+
			u[ix(i, j-1)]+
			u[ix(i+1, j)]+
			u[ix(i+1, j-1)]
			)/4;
	}

	//why i-1?
	float avgV(int i, int j) const {
		return (
			v[ix(i, j)]+
			v[ix(i, j+1)]+
			v[ix(i-1, j)]+
			v[ix(i-1, j+1)]
			)/4;
	}

	void advectVel(float dt) {
		//store so as not to update as iterating
		std::memcpy(new_u, u, sizeof(float)*num_cells);
		std::memcpy(new_v, v, sizeof(float)*num_cells);

		float h2=h/2;

		for(int i=1; i<num_x; i++) {
			for(int j=1; j<num_y; j++) {
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
		std::memcpy(u, new_u, sizeof(float)*num_cells);
		std::memcpy(v, new_v, sizeof(float)*num_cells);
	}

	void advectSmoke(float dt) {
		std::memcpy(new_m, m, sizeof(float)*num_cells);

		float h2=h/2;

		for(int i=1; i<num_x-1; i++) {
			for(int j=1; j<num_y-1; j++) {
				//skip solid cells
				if(solid[ix(i, j)]) continue;
				float u0=(u[ix(i, j)]+u[ix(i+1, j)])/2;
				float v0=(v[ix(i, j)]+v[ix(i, j+1)])/2;
				float x=h2+h*i-dt*u0;
				float y=h2+h*j-dt*v0;
				new_m[ix(i, j)]=sampleField(x, y, S_FIELD);
			}
		}

		std::memcpy(m, new_m, sizeof(float)*num_cells);
	}
};
#endif