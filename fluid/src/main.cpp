#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "common/utils.h"

#include "fluid.h"

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Fluid Testing";
	}

	Fluid fluid;

	float div_amt=20;

	olc::Sprite* gradient=nullptr;
	bool show_div=true;
	bool show_grid=true;

	bool OnUserCreate() override {
		std::srand(std::time(0));
		
		//detemine sizing
		float cell_sz=30;
		fluid=Fluid(1+ScreenWidth()/cell_sz, 1+ScreenHeight()/cell_sz);
		
		randomizeComponents();

		gradient=new olc::Sprite("assets/gradient.png");

		return true;
	}

#pragma region UPDATE HELPERS
	void randomizeComponents() {
		for(int i=0; i<fluid.getSizeU(); i++) {
			fluid.u[i]=cmn::randFloat(-div_amt, div_amt);
		}
		for(int i=0; i<fluid.getSizeV(); i++) {
			fluid.v[i]=cmn::randFloat(-div_amt, div_amt);
		}
	}
#pragma endregion

	void update(float dt) {
		if(GetKey(olc::Key::D).bPressed) show_div^=true;

		if(GetKey(olc::Key::G).bPressed) show_grid^=true;

		if(GetKey(olc::Key::R).bPressed) randomizeComponents();

		if(GetKey(olc::Key::SPACE).bHeld) fluid.solveIncompressibility();
	}

#pragma region RENDER HELPERS
	void DrawArrowDecal(const vf2d& a, const vf2d& b, float sz, const olc::Pixel& col) {
		//main arm
		vf2d sub=b-a;
		vf2d c=b-sz*sub;
		DrawLineDecal(a, c, col);
		//arrow triangle
		vf2d d=.5f*sz*sub.perp();
		vf2d l=c-d, r=c+d;
		DrawLineDecal(l, r, col);
		DrawLineDecal(l, b, col);
		DrawLineDecal(r, b, col);
	}

	void renderComponents(const vf2d& cell_sz, const olc::Pixel& col) {
		//horizontal
		for(int i=0; i<fluid.getWidth()-1; i++) {
			for(int j=0; j<fluid.getHeight(); j++) {
				vf2d lft=cell_sz*vf2d(1+i, .5f+j);
				vf2d rgt=lft+vf2d(fluid.u[fluid.uIX(i, j)], 0);
				DrawArrowDecal(lft, rgt, .4f, col);
			}
		}

		//vertical
		for(int i=0; i<fluid.getWidth(); i++) {
			for(int j=0; j<fluid.getHeight()-1; j++) {
				vf2d top=cell_sz*vf2d(.5f+i, 1+j);
				vf2d btm=top+vf2d(0, fluid.v[fluid.vIX(i, j)]);
				DrawArrowDecal(top, btm, .4f, col);
			}
		}
	}

	void renderGrid(const vf2d& cell_sz, const olc::Pixel& col) {
		//vertical lines
		for(int i=0; i<=fluid.getWidth(); i++) {
			float x=cell_sz.x*i;
			vf2d top(x, 0), btm(x, ScreenHeight());
			DrawLineDecal(top, btm, col);
		}

		//horizontal lines
		for(int j=0; j<=fluid.getHeight(); j++) {
			float y=cell_sz.y*j;
			vf2d lft(0, y), rgt(ScreenWidth(), y);
			DrawLineDecal(lft, rgt, col);
		}
	}

	//show divergence as grid of numbers
	void renderDivergence(const vf2d& cell_sz) {
		for(int i=0; i<fluid.getWidth(); i++) {
			for(int j=0; j<fluid.getHeight(); j++) {
				float div=0;
				if(i!=fluid.getWidth()-1) div+=fluid.u[fluid.uIX(i, j)];//NOT rgt
				if(i!=0) div-=fluid.u[fluid.uIX(i-1, j)];//NOT left
				if(j!=fluid.getHeight()-1) div+=fluid.v[fluid.vIX(i, j)];//NOT BTM
				if(j!=0) div-=fluid.v[fluid.vIX(i, j-1)];//NOT top

				auto div_str=std::to_string(int(div));

				vf2d xy=cell_sz*olc::vi2d(i, j);

				float tex_u=.5f+.5f*std::tanh(div/div_amt);
				olc::Pixel col=gradient->Sample(tex_u, 0);
				FillRectDecal(xy, xy+cell_sz, col);

				vf2d ctr=.5f*cell_sz+xy;
				DrawStringDecal(vf2d(ctr.x-4*div_str.length(), ctr.y-4), div_str, olc::BLACK);
			}
		}
	}
#pragma endregion

	void render() {
		const vf2d screen_sz=GetScreenSize();
		const vf2d cell_sz=screen_sz/vf2d(fluid.getWidth(), fluid.getHeight());

		//black background
		FillRectDecal({0, 0}, screen_sz, olc::WHITE);

		if(show_div) renderDivergence(cell_sz);

		if(show_grid) renderGrid(cell_sz, olc::GREY);

		renderComponents(cell_sz, olc::BLACK);
	}

	bool OnUserUpdate(float dt) override {
		update(dt);

		render();

		return true;
	}
};

int main() {
	Example demo;
	bool vsync=true;
	if(demo.Construct(800, 600, 1, 1, false, vsync)) demo.Start();

	return 0;
}