//my implementation of
//https://github.com/matthias-research/pages/blob/master/tenMinutePhysics/18-flip.html
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "flip_fluid.h"

struct Thing : olc::PixelGameEngine {
	Thing() {
		sAppName="thing";
	}

	FlipFluid* fluid=nullptr;

	bool OnUserCreate() override {
		float sim_height=3;
		float sim_width=sim_height*ScreenWidth()/ScreenHeight();

		int res=100;
		float tank_height=1*sim_height;
		float tank_width=1*sim_width;
		float h=tank_height/res;
		float density=1000;

		float rel_water_height=.8f;
		float rel_water_width=.6f;

		//compute number of particles
		float r=.3f*h;//particle radius w.r.t. cell size
		float dx=2*r;
		float dy=std::sqrtf(3)/2*dx;

		int num_x=(rel_water_width*tank_width-2*h-2*r)/dx;
		int num_y=(rel_water_height*tank_height-2*h-2*r)/dy;
		int max_particles=num_x*num_y;
		
		//create fluid
		fluid=new FlipFluid(density, tank_width, tank_height, h, r, max_particles);

		//create particles
		fluid->num_particles=num_x*num_y;
		int p=0;
		for(int i=0; i<num_x; i++) {
			for(int j=0; j<num_y; j++) {
				fluid->particle_pos[p++]=h+r+dx*i+(j%2==0?0:r);
				fluid->particle_pos[p++]=h+r+dy*j;
			}
		}

		//setup grid cells for tank
		for(int i=0; i<fluid->f_num_x; i++) {
			for(int j=0; j<fluid->f_num_y; j++) {
				float s=1;//fluid
				if(i==0||i==fluid->f_num_x-1||j==0) s=0;//solid
				fluid->s[j+fluid->f_num_y*i]=s;
			}
		}

		//impl on 1021
		//setObstacle(3, 2, true);

		return true;
	}

	bool OnUserDestroy() override {
		delete fluid;
		
		return true;
	}

	bool OnUserUpdate(float dt) override {

		return true;
	}
};


int main() {
	Thing t;
	bool vsync=true;
	if(t.Construct(640, 480, 1, 1, false, vsync)) t.Start();

	return 0;
}