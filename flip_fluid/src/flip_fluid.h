//lines 94-663
#pragma once
#ifndef FLIP_FLUID_CLASS_H
#define FLIP_FLUID_CLASS_H

#include <algorithm>
#include <string>
#include <cmath>

struct FlipFluid {
	float density=0;

	int f_num_x=0, f_num_y=0;
	int f_num_cells=0;

	//cell size
	float h=0;
	float f_inv_spacing=0;

	float* u=nullptr, * v=nullptr;
	float* du=nullptr, * dv=nullptr;
	float* prev_u=nullptr, * prev_v=nullptr;

	float* p=nullptr;
	float* s=nullptr;//solidity?

	enum CellType {
		FluidCell=0,
		AirCell,
		SolidCell
	};
	int* cell_type=nullptr;
	float* cell_color=nullptr;

	int max_particles=0;
	float* particle_pos=nullptr;
	float* particle_color=nullptr;

	float* particle_vel=nullptr;
	float* particle_density=nullptr;
	float particle_rest_density=0;

	float particle_radius=0;
	float p_inv_spacing=0;

	int p_num_x=0;
	int p_num_y=0;
	int p_num_cells=0;

	int* num_cell_particles=nullptr;
	int* first_cell_particle=nullptr;
	int* cell_particle_ids=nullptr;

	int num_particles=0;

	FlipFluid() {}

	//95-141
	FlipFluid(
		float d,
		float width, float height, float sp,
		float r, int m) {
		//fluid
		density=d;

		f_num_x=1+width/sp;
		f_num_y=1+height/sp;
		f_num_cells=f_num_x*f_num_y;

		h=std::max(width/f_num_x, height/f_num_y);
		f_inv_spacing=1/h;

		u=new float[f_num_cells];
		v=new float[f_num_cells];
		du=new float[f_num_cells];
		dv=new float[f_num_cells];
		prev_u=new float[f_num_cells];
		prev_v=new float[f_num_cells];

		p=new float[f_num_cells];
		s=new float[f_num_cells];

		cell_type=new int[f_num_cells];
		cell_color=new float[3*f_num_cells];

		//particles
		max_particles=m;
		
		particle_pos=new float[2*max_particles];
		particle_color=new float[3*max_particles];
		for(int i=0; i<max_particles; i++) {
			particle_color[2+3*i]=1;
		}

		particle_vel=new float[2*max_particles];
		particle_density=new float[f_num_cells];
		particle_rest_density=0;

		particle_radius=r;
		p_inv_spacing=1/(2.2f*particle_radius);
		p_num_x=1+width*p_inv_spacing;
		p_num_x=1+height*p_inv_spacing;
		p_num_cells=p_num_x*p_num_y;

		num_cell_particles=new int[p_num_cells];
		first_cell_particle=new int[1+p_num_cells];
		cell_particle_ids=new int[max_particles];

		num_particles=0;
	}

	//ro3 1
	FlipFluid(const FlipFluid&)=delete;

	//ro3 2
	~FlipFluid() {
		delete[] u;
		delete[] v;
		delete[] du;
		delete[] dv;
		delete[] prev_u;
		delete[] prev_v;

		delete[] p;
		delete[] s;

		delete[] cell_type;
		delete[] cell_color;

		delete[] particle_pos;
		delete[] particle_color;

		delete[] particle_vel;
		delete[] particle_density;

		delete[] num_cell_particles;
		delete[] first_cell_particle;
		delete[] cell_particle_ids;
	}

	//ro3 3
	FlipFluid& operator=(const FlipFluid&)=delete;

	//143-150
	void integrateParticles(float dt, float gravity) {
		for(int i=0; i<num_particles; i++) {
			particle_vel[1+2*i]+=gravity*dt;
			particle_pos[2*i]+=particle_vel[2*i]*dt;
			particle_pos[1+2*i]+=particle_vel[1+2*i]*dt;
		}
	}

	//152-251
	void pushParticlesApart(int num_iter) {
		float color_diffusion_coeff=.001f;

		//count particles per cell
		memset(num_cell_particles, 0, sizeof(int)*p_num_cells);
		for(int i=0; i<num_particles; i++) {
			float x=particle_pos[2*i];
			float y=particle_pos[1+2*i];

			int xi=std::clamp(int(x*p_inv_spacing), 0, p_num_x-1);
			int yi=std::clamp(int(y*p_inv_spacing), 0, p_num_y-1);
			num_cell_particles[yi+p_num_y*xi]++;
		}

		//partial sums
		int first=0;
		for(int i=0; i<p_num_cells; i++) {
			first+=num_cell_particles[i];
			first_cell_particle[i]=first;
		}
		//guard
		first_cell_particle[p_num_cells]=first;
	
		//fill particles into cells
		for(int i=0; i<num_particles; i++) {
			float x=particle_pos[2*i];
			float y=particle_pos[1+2*i];

			int xi=std::clamp(int(x*p_inv_spacing), 0, p_num_x-1);
			int yi=std::clamp(int(y*p_inv_spacing), 0, p_num_y-1);
			int cell_nr=yi+p_num_y*xi;
			first_cell_particle[cell_nr]--;
			cell_particle_ids[first_cell_particle[cell_nr]]=i;
		}

		//push particles apart
		float min_dist=2*particle_radius;
		float min_dist_sq=min_dist*min_dist;
		for(int iter=0; iter<num_iter; iter++) {
			for(int i=0; i<num_particles; i++) {
				float& px=particle_pos[2*i];
				float& py=particle_pos[1+2*i];
				int pxi=px*p_inv_spacing;
				int pyi=py*p_inv_spacing;
				int x0=std::max(pxi-1, 0);
				int y0=std::max(pyi-1, 0);
				int x1=std::min(pxi+1, p_num_x-1);
				int y1=std::min(pyi+1, p_num_y-1);
				for(int xi=x0; xi<=x1; xi++) {
					for(int yi=y0; yi<=y1; yi++) {
						int cell_nr=yi+p_num_y*xi;
						int first=first_cell_particle[cell_nr];
						int last=first_cell_particle[1+cell_nr];
						for(int j=first; j<last; j++) {
							int id=cell_particle_ids[j];
							if(id==i) continue;

							float& qx=particle_pos[2*id];
							float& qy=particle_pos[1+2*id];
							float dx=qx-px;
							float dy=qy-py;
							float d_sq=dx*dx+dy*dy;
							if(d_sq>min_dist_sq||d_sq==0) continue;

							float d=std::sqrtf(d_sq);
							float s=.5f*(min_dist-d)/d;
							dx*=s, dy*=s;
							px-=dx, py-=dy;
							qx+=dx, py+=dy;

							//diffuse colors
							for(int k=0; k<3; k++) {
								float& color0=particle_color[k+3*i];
								float& color1=particle_color[k+3*id];
								//toward average
								float color=(color0+color1)/2;
								color0+=color_diffusion_coeff*(color-color0);
								color1+=color_diffusion_coeff*(color-color1);
							}
						}
					}
				}
			}
		}
	}

	//253-311
	void handleParticleCollisions(float ox, float oy, float orad) {
		//float h=1/f_inv_spacing;
		float orad_sq=orad*orad;
		float min_dist=particle_radius+orad;
		float min_dist_sq=min_dist*min_dist;

		float min_x=h+particle_radius;
		float max_x=h*(f_num_x-1)-particle_radius;
		float min_y=h+particle_radius;
		float max_y=h*(f_num_y-1)-particle_radius;
	
		for(int i=0; i<num_particles; i++) {
			float& x=particle_pos[2*i];
			float& y=particle_pos[1+2*i];
			
			float dx=x-ox;
			float dy=y-oy;
			float d_sq=dx*dx+dy*dy;

			//obstacle collision
			if(d_sq<min_dist_sq) {
				float d=std::sqrtf(d_sq);
				float amt=(min_dist-d)/d;
				//x=ox, y=oy;
				x+=dx*amt;
				y+=dy*amt;
			}

			//wall collisions
			if(x<min_x) {
				x=min_x;
				particle_vel[2*i]=0;
			}
			if(x>max_x) {
				x=max_x;
				particle_vel[2*i]=0;
			}
			if(y<min_y) {
				y=min_y;
				particle_vel[2*i+1]=0;
			}
			if(y>max_y) {
				y=max_y;
				particle_vel[1+2*i]=0;
			}
		}
	}

	//313-380
	void updateParticleDensity() {
		float h2=.5f*h;

		memset(particle_density, 0.f, sizeof(float)*f_num_cells);

		for(int i=0; i<num_particles; i++) {
			float x=particle_pos[2*i];
			float y=particle_pos[1+2*i];

			x=std::clamp(x, h, h*(f_num_x-1));
			y=std::clamp(y, h, h*(f_num_y-1));

			int x0=f_inv_spacing*(x-h2);
			float tx=f_inv_spacing*(x-h2-h*x0);
			int x1=std::min(1+x0, f_num_x-2);

			int y0=f_inv_spacing*(y-h2);
			float ty=f_inv_spacing*(y-h2-h*y0);
			int y1=std::min(1+y0, f_num_y-2);

			float sx=1-tx;
			float sy=1-ty;

			if(x0<f_num_x&&y0<f_num_y) particle_density[y0+f_num_y*x0]+=sx*sy;
			if(x1<f_num_x&&y0<f_num_y) particle_density[y0+f_num_y*x1]+=tx*sy;
			if(x0<f_num_x&&y1<f_num_y) particle_density[y1+f_num_y*x0]+=sx*ty;
			if(x1<f_num_x&&y1<f_num_y) particle_density[y1+f_num_y*x1]+=tx*ty;
		}

		if(particle_rest_density==0) {
			float sum=0;
			int num_fluid_cells=0;
			for(int i=0; i<f_num_cells; i++) {
				if(cell_type[i]==FluidCell) {
					sum+=particle_density[i];
					num_fluid_cells++;
				}
			}

			if(num_fluid_cells>0) particle_rest_density=sum/num_fluid_cells;
		}
	}

	//382-498
	void transferVelocities(bool to_grid, float flip_ratio=.5f) {
		float h2=.5f*h;

		if(to_grid) {
			memcpy(prev_u, u, sizeof(float)*f_num_cells);
			memcpy(prev_v, v, sizeof(float)*f_num_cells);

			memset(du, 0.f, sizeof(float)*f_num_cells);
			memset(dv, 0.f, sizeof(float)*f_num_cells);
			memset(u, 0.f, sizeof(float)*f_num_cells);
			memset(v, 0.f, sizeof(float)*f_num_cells);

			for(int i=0; i<f_num_cells; i++) {
				cell_type[i]=s[i]==0?SolidCell:AirCell;
			}

			for(int i=0; i<num_particles; i++) {
				float x=particle_pos[2*i];
				float y=particle_pos[1+2*i];
				int xi=std::clamp(int(f_inv_spacing*x), 0, f_num_x-1);
				int yi=std::clamp(int(f_inv_spacing*y), 0, f_num_y-1);
				int cell_nr=yi+f_num_y*xi;
				if(cell_type[cell_nr]==AirCell) cell_type[cell_nr]=FluidCell;
			}
		}

		for(int comp=0; comp<2; comp++) {
			float dx=comp==0?0:h2;
			float dy=comp==0?h2:0;
			
			float* f=comp==0?u:v;
			float* prev_f=comp==0?prev_u:prev_v;
			float* d=comp==0?du:dv;

			for(int i=0; i<i<num_particles; i++) {
				float x=particle_pos[2*i];
				float y=particle_pos[1+2*i];

				x=std::clamp(x, h, h*(f_num_x-1));
				y=std::clamp(y, h, h*(f_num_y-1));

				int x0=f_inv_spacing*(x-h2);
				float tx=f_inv_spacing*(x-h2-h*x0);
				int x1=std::min(1+x0, f_num_x-2);

				int y0=f_inv_spacing*(y-h2);
				float ty=f_inv_spacing*(y-h2-h*y0);
				int y1=std::min(1+y0, f_num_y-2);

				float sx=1-tx;
				float sy=1-ty;

				float d0=sx*sy;
				float d1=tx*sy;
				float d2=tx*ty;
				float d3=sx*ty;

				int nr0=y0+x0*f_num_y;
				int nr1=y0+x1*f_num_y;
				int nr2=y1+x1*f_num_y;
				int nr3=y1+x0*f_num_y;
				
				if(to_grid) {
					float pv=particle_vel[comp+2*i];
					f[nr0]+=pv*d0, d[nr0]+=d0;
					f[nr1]+=pv*d1, d[nr1]+=d1;
					f[nr2]+=pv*d2, d[nr2]+=d2;
					f[nr3]+=pv*d3, d[nr3]+=d3;
				} else {
					int offset=comp==0?f_num_y:1;
					bool valid0=cell_type[nr0]!=AirCell||cell_type[nr0-offset]!=AirCell;
					bool valid1=cell_type[nr1]!=AirCell||cell_type[nr1-offset]!=AirCell;
					bool valid2=cell_type[nr2]!=AirCell||cell_type[nr2-offset]!=AirCell;
					bool valid3=cell_type[nr3]!=AirCell||cell_type[nr3-offset]!=AirCell;
					
					float& v=particle_vel[comp+2*i];
					float d_sum=valid0*d0+valid1*d1+valid2*d2+valid3*d3;
					if(d_sum>0) {
						float pic_v=(
							valid0*d0*f[nr0]+
							valid1*d1*f[nr1]+
							valid2*d2*f[nr2]+
							valid3*d3*f[nr3]
						)/d_sum;
						float corr=(
							valid0*d0*(f[nr0]-prev_f[nr0])+
							valid1*d1*(f[nr1]-prev_f[nr1])+
							valid2*d2*(f[nr2]-prev_f[nr2])+
							valid3*d3*(f[nr3]-prev_f[nr3])
						)/d_sum;
						float flip_v=v+corr;
						v=(1-flip_ratio)*pic_v+flip_ratio*flip_v;
					}
				}
			}

			if(to_grid) {
				for(int i=0; i<f_num_cells; i++) {
					if(d[i]>0) f[i]/=d[i];
				}

				//restore solid cells
				for(int i=0; i<f_num_x; i++) {
					for(int j=0; j<f_num_y; j++) {
						bool solid=cell_type[j+f_num_y*i]==SolidCell;
						if(solid||(i>0&&cell_type[j+f_num_y*(i-1)]==SolidCell)) u[j+f_num_y*i]=prev_u[j+f_num_y*i];
						if(solid||(j>0&&cell_type[j-1+f_num_y*i]==SolidCell)) v[j+f_num_y*i]=prev_v[j+f_num_y*i];
					}
				}
			}
		}
	}

	//500-558
	void solveIncompressibility(int num_iters, float dt, float over_relaxation, bool compensate_drift=true) {
		memset(p, 0.f, sizeof(float)*f_num_cells);
		memcpy(prev_u, u, sizeof(float)*f_num_cells);
		memcpy(prev_v, v, sizeof(float)*f_num_cells);
		
		float cp=density*h/dt;
		 
		for(int iter=0; iter<num_iters; iter++) {
			for(int i=1; i<f_num_x-1; i++) {
				for(int j=1; j<f_num_y-1; j++) {
					if(cell_type[j+f_num_y*i]!=FluidCell) continue;

					int center=j+f_num_y*i;
					int left=j+f_num_y*(i-1);
					int right=j+f_num_y*(i+1);
					int bottom=j-1+f_num_y*i;
					int top=j+1+f_num_y*i;

					float sx0=s[left];
					float sx1=s[right];
					float sy0=s[bottom];
					float sy1=s[top];
					float s_sum=sx0+sx1+sy0+sy1;
					if(s==0) continue;

					float div=u[right]-u[center]+v[top]-v[center];

					if(particle_rest_density>0&&compensate_drift) {
						float k=1;
						float compression=particle_density[j+f_num_y*i]-particle_rest_density;
						if(compression>0) div-=k*compression;
					}

					float p_val=-div/s_sum;
					p_val*=over_relaxation;
					p[center]+=cp*p_val;

					u[center]-=sx0*p_val;
					u[right]+=sx1*p_val;
					v[center]-=sy0*p_val;
					v[top]+=sy1*p_val;
				}
			}
		}
	}

	//560-599
	void updateParticleColors() {
		for(int i=0; i<num_particles; i++) {
			particle_color[3*i]=std::clamp(particle_color[3*i]-.01f, 0.f, 1.f);
			particle_color[1+3*i]=std::clamp(particle_color[1+3*i]-.01f, 0.f, 1.f);
			particle_color[2+3*i]=std::clamp(particle_color[2+3*i]+.01f, 0.f, 1.f);

			float x=particle_pos[2*i];
			float y=particle_pos[1+2*i];
			int xi=std::clamp(int(f_inv_spacing*x), 1, f_num_x-1);
			int yi=std::clamp(int(f_inv_spacing*y), 1, f_num_y-1);
			int cell_nr=yi+f_num_y*xi;

			if(particle_rest_density>0) {
				float rel_density=particle_density[cell_nr]/particle_rest_density;
				if(rel_density<.7f) {
					particle_color[3*i]=.8f;
					particle_color[1+3*i]=.8f;
					particle_color[2+3*i]=1;
				}
			}
		}
	}

	//601-621
	void setSciColor(int cell_nr, float val, float min_val, float max_val) {
		val=std::clamp(val, min_val, max_val-.0001f);
		float d=max_val-min_val;
		val=d==0?.5f:(val-min_val)/d;
		float m=.25f;
		int num=val/m;
		float s=(val-num*m)/m;
		
		float r=0, g=0, b=0;
		switch(num) {
			case 0: r=0; g=s; b=1; break;
			case 1: r=0; g=1; b=1-s; break;
			case 2: r=s; g=1; b=0; break;
			case 3: r=1; g=1-s; b=0; break;
		}

		cell_color[3*cell_nr]=r;
		cell_color[1+3*cell_nr]=g;
		cell_color[2+3*cell_nr]=b;
	}

	//623-641
	void updateCellColors() {
		memset(cell_color, 0.f, sizeof(float)*3*f_num_cells);

		for(int i=0; i<f_num_cells; i++) {
			if(cell_type[i]==SolidCell) {
				cell_color[3*i]=.5f;
				cell_color[3*i+1]=.5f;
				cell_color[3*i+2]=.5f;
			} else if(cell_type[i]==FluidCell) {
				float d=particle_density[i];
				if(particle_rest_density>0) d/=particle_rest_density;
				setSciColor(i, d, 0, 2);
			}
		}
	}

	//643-662
	void simulate(
		float dt, float gravity, float flip_ratio,
		int num_pressure_iters, int num_particle_iters,
		float over_relaxation, bool compensate_drift, bool separate_particles,
		float obstacle_x, float obstacle_y, float obstacle_radius
	) {
		int num_sub_steps=1;
		float sdt=dt/num_sub_steps;

		for(int step=0; step<num_sub_steps; step++) {
			integrateParticles(sdt, gravity);
			if(separate_particles) pushParticlesApart(num_particle_iters);
			handleParticleCollisions(obstacle_x, obstacle_y, obstacle_radius);
			transferVelocities(true);
			updateParticleDensity();
			solveIncompressibility(num_pressure_iters, sdt, over_relaxation, compensate_drift);
			transferVelocities(false, flip_ratio);
		}

		updateParticleColors();
		updateCellColors();
	}
};
#endif