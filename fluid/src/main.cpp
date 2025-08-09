#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "common/utils.h"

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Fluid Testing";
	}

	int width=0, height=0;
	float* u=nullptr;
	float* v=nullptr;

	int uIX(int i, int j) const {
		return i+(width-1)*j;
	}

	int vIX(int i, int j) const {
		return i+width*j;
	}

	float div_amt=20;

	olc::Sprite* gradient=nullptr;
	bool show_div=true;

	bool OnUserCreate() override {
		srand(time(0));
		
		//detemine sizing
		width=20;
		height=15;
		
		//allocate grid
		u=new float[(width-1)*height];
		v=new float[width*(height-1)];
		
		randomizeComponents();

		gradient=new olc::Sprite("assets/gradient.png");

		return true;
	}

	bool OnUserDestroy() override {
		delete[] u;
		delete[] v;

		return true;
	}

	void randomizeComponents() {
		for(int i=0; i<(width-1)*height; i++) {
			u[i]=cmn::random(-div_amt, div_amt);
		}
		for(int i=0; i<width*(height-1); i++) {
			v[i]=cmn::random(-div_amt, div_amt);
		}
	}

	void DrawArrow(const vf2d& a, const vf2d& b, float sz, const olc::Pixel& col) {
		//main arm
		vf2d sub=b-a;
		vf2d c=b-sz*sub;
		DrawLine(a, c, col);
		//arrow triangle
		vf2d d=.5f*sz*sub.perp();
		vf2d l=c-d, r=c+d;
		DrawLine(l, r, col);
		DrawLine(l, b, col);
		DrawLine(r, b, col);
	}

	bool OnUserUpdate(float dt) override {
		if(GetKey(olc::Key::R).bPressed) randomizeComponents();

		if(GetKey(olc::Key::D).bPressed) show_div^=true;
		
		const float sz_x=float(ScreenWidth())/width;
		const float sz_y=float(ScreenHeight())/height;
		
		//black background
		Clear(olc::BLACK);

		//show components
		{
			//horizontal
			for(int i=0; i<width-1; i++) {
				for(int j=0; j<height; j++) {
					float x=sz_x*(1+i), y=sz_y*(.5f+j);
					vf2d lft(x, y), rgt=lft+vf2d(u[uIX(i, j)], 0);
					DrawArrow(lft, rgt, .4f, olc::RED);
				}
			}

			//vertical
			for(int i=0; i<width; i++) {
				for(int j=0; j<height-1; j++) {
					float x=sz_x*(.5f+i), y=sz_y*(1+j);
					vf2d top(x, y), btm=top+vf2d(0, v[vIX(i, j)]);
					DrawArrow(top, btm, .4f, olc::BLUE);
				}
			}
		}

		//show grid
		{
			//vertical lines
			for(int i=1; i<width; i++) {
				float x=sz_x*i;
				vf2d top(x, 0), btm(x, ScreenHeight());
				DrawLine(top, btm, olc::WHITE);
			}

			//horizontal lines
			for(int j=1; j<height; j++) {
				float y=sz_y*j;
				vf2d lft(0, y), rgt(ScreenWidth(), y);
				DrawLine(lft, rgt, olc::WHITE);
			}
		}

		//show divergence as grid of numbers
		if(show_div) {
			for(int i=0; i<width; i++) {
				for(int j=0; j<height; j++) {
					float x=sz_x*(.5f+i);
					float y=sz_y*(.5f+j);

					float div=0;
					if(i!=width-1) div+=u[uIX(i, j)];//NOT rgt
					if(i!=0) div-=u[uIX(i-1, j)];//NOT left
					if(j!=height-1) div+=v[vIX(i, j)];//NOT BTM
					if(j!=0) div-=v[vIX(i, j-1)];//NOT top

					auto str=std::to_string(int(div));
				
					float tex_u=.5f+.5f*std::tanh(div/div_amt);
					olc::Pixel col=gradient->Sample(tex_u, 0);
				
					DrawStringDecal(vf2d(x-4*str.length(), y-4), str, col);
				}
			}
		}

		return true;
	}
};

int main() {
	Example demo;
	bool vsync=true;
	if(demo.Construct(800, 600, 1, 1, false, vsync)) demo.Start();

	return 0;
}