//my implementation of
//https://github.com/matthias-research/pages/blob/master/tenMinutePhysics/18-flip.html
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "flip_fluid.h"

struct FluidUI : olc::PixelGameEngine {
	FluidUI() {
		sAppName="Flip Fluid Simulation";
	}

	FlipFluid* fluid=nullptr;
	float c_scale=0;

	bool show_obstacle=true;
	float obstacle_x=0;
	float obstacle_y=0;
	float obstacle_vel_x=0;
	float obstacle_vel_y=0;
	const float obstacle_radius=.15f;

	const float time_step=1/60.f;
	float update_timer=0;

	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;

	bool OnUserCreate() override {
		float sim_height=3;
		c_scale=ScreenHeight()/sim_height;
		float sim_width=ScreenWidth()/c_scale;

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
		float dy=std::sqrt(3)/2*dx;

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

		setObstacle(3, 2, true);

		{
			int sz=256;
			prim_circ_spr=new olc::Sprite(sz, sz);
			SetDrawTarget(prim_circ_spr);
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2, olc::WHITE);
			SetDrawTarget(nullptr);
			prim_circ_dec=new olc::Decal(prim_circ_spr);
		}

		return true;
	}

	//1021
	void setObstacle(float x, float y, bool reset) {
		float vx=0, vy=0;
		if(!reset) {
			vx=(x-obstacle_x)/time_step;
			vy=(y-obstacle_y)/time_step;
		}

		obstacle_x=x;
		obstacle_y=y;
		float cd=std::sqrt(2)*fluid->h;

		for(int i=1; i<fluid->f_num_x-2; i++) {
			for(int j=1; j<fluid->f_num_y-2; j++) {
				fluid->s[j+fluid->f_num_y*i]=1;

				float dx=fluid->h*(.5f+i)-x;
				float dy=fluid->h*(.5f+j)-y;

				if(dx*dx+dy*dy<obstacle_radius*obstacle_radius) {
					fluid->s[j+fluid->f_num_y*i]=0;
					fluid->u[j+fluid->f_num_y*i]=vx;
					fluid->u[j+fluid->f_num_y*(i+1)]=vx;
					fluid->v[j+fluid->f_num_y*i]=vy;
					fluid->v[j+1+fluid->f_num_y*i]=vy;
				}
			}
		}

		obstacle_vel_x=vx;
		obstacle_vel_y=vy;
	}

	bool OnUserDestroy() override {
		delete fluid;
		
		return true;
	}

	void FillCircleDecal(olc::vf2d pos, float rad, olc::Pixel col) {
		olc::vf2d offset(rad, rad);
		olc::vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	bool OnUserUpdate(float dt) override {
		const auto mouse_left=GetMouse(olc::Mouse::LEFT);
		if(mouse_left.bPressed) {
			float x=GetMouseX()/c_scale;
			float y=GetMouseY()/c_scale;
			setObstacle(x, y, true);
		}
		if(mouse_left.bHeld) {
			float x=GetMouseX()/c_scale;
			float y=GetMouseY()/c_scale;
			setObstacle(x, y, false);
		}
		if(mouse_left.bReleased) {
			obstacle_vel_x=0;
			obstacle_vel_y=0;
		}
		
		//ensure similar update across multiple framerates
		update_timer+=dt;
		while(update_timer>time_step) {
			fluid->simulate(
				time_step, 9.81f, .9f, 50, 2, 1.9f, true, true,
				obstacle_x, obstacle_y, obstacle_vel_x, obstacle_vel_y, obstacle_radius
			);

			update_timer-=time_step;
		}

		//render
		Clear(olc::BLACK);
		FillCircleDecal({c_scale*obstacle_x, c_scale*obstacle_y}, c_scale*obstacle_radius, olc::RED);
		for(int i=0; i<fluid->num_particles; i++) {
			//have to remap back into screen space.
			float x=c_scale*fluid->particle_pos[2*i];
			float y=c_scale*fluid->particle_pos[1+2*i];

			FillCircleDecal({x, y}, c_scale*fluid->particle_radius, olc::WHITE);
		}

		return true;
	}
};


int main() {
	FluidUI fui;
	bool vsync=true;
	if(fui.Construct(800, 600, 1, 1, false, vsync)) fui.Start();

	return 0;
}