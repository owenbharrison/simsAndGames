#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "fluid.h"

float map(float x, float a, float b, float c, float d) {
	float t=(x-a)/(b-a);
	return c+t*(d-c);
}

struct FluidUI : olc::PixelGameEngine {
	FluidUI() {
		sAppName="Fluid";
	}

	float cell_size=0;
	Fluid fluid;

	bool OnUserCreate() override {
		cell_size=2;
		size_t width=ScreenWidth()/cell_size;
		size_t height=ScreenHeight()/cell_size;
		fluid=Fluid(width, height, cell_size, 1000);

		//wind tunnel
		float in_vel=20;
		for(size_t i=0; i<fluid.num_x; i++) {
			for(size_t j=0; j<fluid.num_y; j++) {
				//set solid tiles(left, top, bottom)
				fluid.solid[fluid.ix(i, j)]=i==0||j==0||j==fluid.num_y-1;

				//set in velocity
				if(i==1) fluid.u[fluid.ix(i, j)]=in_vel;
			}
		}

		//set obstacle
		size_t x=fluid.num_x/3;
		size_t y=fluid.num_y/2;
		size_t rad=fluid.num_x/6;
		for(int i=0; i<fluid.num_x; i++) {
			int dx=i-x;
			for(int j=0; j<fluid.num_y; j++) {
				int dy=j-y;
				if(dx*dx+dy*dy<rad*rad) {
					fluid.solid[fluid.ix(i, j)]=true;
				}
			}
		}

		size_t pipe_h=fluid.num_y/8;
		size_t min_j=.5f*fluid.num_y-.5f*pipe_h;
		size_t max_j=.5f*fluid.num_y+.5f*pipe_h;
		for(size_t j=min_j; j<=max_j; j++) {
			fluid.m[fluid.ix(0, j)]=0;
		}

		return true;
	}

	bool OnUserUpdate(float dt) override {
		//fluid.integrate(dt, -9.81f);

		fluid.solveIncompressibility(40, dt);

		fluid.extrapolate();
		fluid.advectVel(dt);
		fluid.advectSmoke(dt);

		Clear(olc::BLACK);
		for(size_t i=0; i<fluid.num_x; i++) {
			float x=map(i, 0, fluid.num_x, 0, ScreenWidth());
			for(size_t j=0; j<fluid.num_y; j++) {
				if(fluid.solid[fluid.ix(i, j)]) continue;
				
				float y=map(j, 0, fluid.num_y, 0, ScreenHeight());

				float shade=fluid.m[fluid.ix(i, j)];
				olc::Pixel col=olc::PixelF(shade, shade, shade);
				FillRectDecal({x, y}, {cell_size, cell_size}, col);
			}
		}

		return true;
	}
};

int main() {
	FluidUI f;
	bool vsync=true;
	if(f.Construct(320, 240, 1, 1, false, vsync)) f.Start();

	return 0;
}