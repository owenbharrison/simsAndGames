#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include <cmath>

#include "fluid.h"

float map(float x, float a, float b, float c, float d) {
	float t=(x-a)/(b-a);
	return c+t*(d-c);
}

constexpr float Pi=3.1415927f;

struct FluidUI : olc::PixelGameEngine {
	FluidUI() {
		sAppName="Fluid";
	}

	float cell_size=0;
	Fluid fluid;

	int obstacle_x=0, obstacle_y=0;

	bool show_pressure=false;
	bool show_streamlines=false;

	//int is used for overflow reasons
	void setObstacle(int x, int y, int r, bool reset=true, float dt=1) {
		memset(fluid.solid, false, sizeof(bool)*fluid.num_x*fluid.num_y);

		//left
		for(size_t j=0; j<fluid.num_y; j++) {
			fluid.solid[fluid.ix(0, j)]=true;
		}

		//top, bottom
		for(size_t i=0; i<fluid.num_x; i++) {
			fluid.solid[fluid.ix(i, 0)]=true;
			fluid.solid[fluid.ix(i, fluid.num_y-1)]=true;
		}

		float vx=0.f, vy=0.f;
		if(!reset) {
			vx=(x-obstacle_x)/dt;
			vy=(y-obstacle_y)/dt;
		}

		obstacle_x=x;
		obstacle_y=y;

		//fill circle
		for(int i=1; i<fluid.num_x-1; i++) {
			int dx=i-x;
			for(int j=1; j<fluid.num_y-1; j++) {
				int dy=j-y;
				if(dx*dx+dy*dy<r*r) {
					fluid.solid[fluid.ix(i, j)]=true;
					fluid.m[fluid.ix(i, j)]=1.f;
					fluid.u[fluid.ix(i, j)]=vx;
					fluid.v[fluid.ix(i, j)]=vy;
				}
			}
		}
	}

	bool OnUserCreate() override {
		cell_size=5;
		size_t width=ScreenWidth()/cell_size;
		size_t height=ScreenHeight()/cell_size;
		//what is h?
		fluid=Fluid(width, height, 1000, 1/100.f);

		//wind tunnel
		float in_vel=2;
		for(size_t j=0; j<fluid.num_y; j++) {
			fluid.u[fluid.ix(1, j)]=in_vel;
		}

		//set obstacle
		setObstacle(fluid.num_x/3, fluid.num_y/2, fluid.num_x/8);

		size_t pipe_h=fluid.num_y/6;
		size_t min_j=fluid.num_y/2-pipe_h/2;
		size_t max_j=fluid.num_y/2+pipe_h/2;
		for(size_t j=min_j; j<=max_j; j++) {
			fluid.m[fluid.ix(0, j)]=0;
		}

		return true;
	}


	bool OnUserUpdate(float dt) override {
		if(dt>1/60.f) dt=1/60.f;

		if(GetMouse(olc::Mouse::LEFT).bHeld) {
			int x=map(GetMouseX(), 0, ScreenWidth(), 0, fluid.num_x);
			int y=map(GetMouseY(), 0, ScreenHeight(), 0, fluid.num_y);
			setObstacle(x, y, fluid.num_x/8);
		}

		if(GetKey(olc::Key::P).bPressed) show_pressure^=true;
		if(GetKey(olc::Key::S).bPressed) show_streamlines^=true;

		fluid.solveIncompressibility(40, dt);

		fluid.extrapolate();
		fluid.advectVel(dt);
		fluid.advectSmoke(dt);

		float p_min=INFINITY, p_max=-p_min;
		if(show_pressure) {
			for(size_t i=0; i<fluid.num_x; i++) {
				for(size_t j=0; j<fluid.num_y; j++) {
					float p=fluid.pressure[fluid.ix(i, j)];
					if(p<p_min) p_min=p;
					if(p>p_max) p_max=p;
				}
			}
		}

		Clear(olc::BLACK);
		for(size_t i=0; i<fluid.num_x; i++) {
			float x=map(i, 0, fluid.num_x, 0, ScreenWidth());
			for(size_t j=0; j<fluid.num_y; j++) {
				if(fluid.solid[fluid.ix(i, j)]) continue;

				float y=map(j, 0, fluid.num_y, 0, ScreenHeight());
				olc::Pixel col;
				if(show_pressure) {
					float p=fluid.pressure[fluid.ix(i, j)];
					float angle=map(p, p_min, p_max, 0, 2*Pi);
					float r=.5f+.5f*std::cosf(angle);
					float g=.5f+.5f*std::cosf(angle-2*Pi/3);
					float b=.5f+.5f*std::cosf(angle+2*Pi/3);
					col=olc::PixelF(r, g, b);
				} else {
					float shade=fluid.m[fluid.ix(i, j)];
					col=olc::PixelF(shade, shade, shade);
				}
				FillRectDecal({x, y}, {cell_size, cell_size}, col);
			}
		}

		if(show_streamlines) {
			for(size_t i=0; i<fluid.num_x; i+=5) {
				for(size_t j=0; j<fluid.num_y; j+=5) {
					if(fluid.solid[fluid.ix(i, j)]) continue;

					float xh=fluid.h*(.5f+i), yh=fluid.h*(.5f+j);
					float pxh, pyh;
					for(size_t k=0; k<5; k++) {
						float u=fluid.sampleField(xh, yh, Fluid::U_FIELD);
						float v=fluid.sampleField(xh, yh, Fluid::V_FIELD);
						pxh=xh, pyh=yh;
						xh+=dt*u, yh+=dt*v;

						float x=map(fluid.h1*xh, 0, fluid.num_x, 0, ScreenWidth());
						float y=map(fluid.h1*yh, 0, fluid.num_y, 0, ScreenHeight());
						float px=map(fluid.h1*pxh, 0, fluid.num_x, 0, ScreenWidth());
						float py=map(fluid.h1*pyh, 0, fluid.num_y, 0, ScreenHeight());
						DrawLineDecal({x, y}, {px, py}, olc::BLUE);
					}
				}
			}
		}

		return true;
	}
};