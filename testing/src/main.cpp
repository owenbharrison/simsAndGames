#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

class Example : public olc::PixelGameEngine {
	int width=0;
	int height=0;
	bool* grid=nullptr;

	bool inRange(int i, int j) const {
		return i>=0&&j>=0&&i<width&&j<height;
	}

	vf2d cell_size;

	vf2d player_pos;
	vf2d player_vel;
	vf2d player_acc;
	float player_rad=7;
	vf2d player_up;

	olc::Renderable prim_circ;

public:
	Example() {
		sAppName="ui maths";
	}

public:
	bool OnUserCreate() override {
		//determine spacing
		{
			auto sz=10;
			width=ScreenWidth()/sz;
			height=ScreenHeight()/sz;
			cell_size=GetScreenSize()/vf2d(width, height);
		}

		//allocate & clear grid
		grid=new bool[width*height];
		std::memset(grid, false, sizeof(bool)*width*height);

		//fill grid with circle
		{
			vf2d ctr=GetScreenSize()/2;
			float rad=180;
			for(int i=0; i<width; i++) {
				for(int j=0; j<height; j++) {
					vf2d pos=cell_size*olc::vi2d(i, j);
					vf2d sub=pos-ctr;
					float mag_sq=sub.mag2();
					if(mag_sq<rad*rad) {
						grid[i+width*j]=true;
					}
				}
			}
		}

		{
			int sz=1024;
			prim_circ.Create(sz, sz);
			SetDrawTarget(prim_circ.Sprite());
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2);
			SetDrawTarget(nullptr);
			prim_circ.Decal()->Update();
		}

		return true;
	}

	bool OnUserDestroy() override {
		delete[] grid;
		
		return true;
	}

	void FillCircleDecal(const vf2d& pos, float rad, const olc::Pixel& col) {
		vf2d offset(rad, rad);
		vf2d scale{2*rad/prim_circ.Sprite()->width, 2*rad/prim_circ.Sprite()->height};
		DrawDecal(pos-offset, prim_circ.Decal(), scale, col);
	}

	void DrawThickRect(const vf2d& pos, const vf2d& size, float w, const olc::Pixel& col) {
		FillRectDecal(pos-w/2, vf2d(w+size.x, w), col);
		FillRectDecal(pos+vf2d(-w/2, w/2), vf2d(w, size.y), col);
		FillRectDecal(pos+vf2d(size.x-w/2, w/2), vf2d(w, size.y), col);
		FillRectDecal(pos+vf2d(w/2, size.y-w/2), vf2d(size.x-w, w), col);
	}
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

	void renderGrid(const olc::Pixel& col) {
		for(int i=0; i<=width; i++) {
			float x=cell_size.x*i;
			vf2d top(x, 0), btm(x, ScreenHeight());
			DrawLineDecal(top, btm, col);
		}

		for(int j=0; j<=height; j++) {
			float y=cell_size.y*j;
			vf2d lft(0, y), rgt(ScreenWidth(), y);
			DrawLineDecal(lft, rgt, col);
		}
	}

	void renderCells(const olc::Pixel& col) {
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				if(!grid[i+width*j]) continue;

				vf2d pos=cell_size*olc::vf2d(i, j);
				FillRectDecal(pos, cell_size*olc::vi2d(1, 1), col);
			}
		}
	}

	void checkCollision(float dt) {
		//find grid bounds of player
		int si=(player_pos.x-player_rad)/cell_size.x;
		int sj=(player_pos.y-player_rad)/cell_size.y;
		int ei=(player_pos.x+player_rad)/cell_size.x;
		int ej=(player_pos.y+player_rad)/cell_size.y;
		for(int i=si; i<=ei; i++) {
			for(int j=sj; j<=ej; j++) {
				if(!inRange(i, j)) continue;
				
				if(!grid[i+width*j]) continue;

				//clamp pos to box
				float nx=cell_size.x*i;
				float mx=cell_size.x*(1+i);
				float ny=cell_size.y*j;
				float my=cell_size.y*(1+j);
				float close_x=std::min(std::max(player_pos.x, nx), mx);
				float close_y=std::min(std::max(player_pos.y, ny), my);
				vf2d close_pt(close_x, close_y);
				vf2d sub=player_pos-close_pt;
				float mag_sq=sub.mag2();
				if(mag_sq==0||mag_sq>player_rad*player_rad) continue;

				float mag=std::sqrt(mag_sq);
				vf2d axis=sub/mag;

				//push apart
				player_pos+=(player_rad-mag)*axis;

				//remove component?
				player_vel-=player_vel.dot(player_up)*player_up;
				player_vel*=1-dt;
				return;
			}
		}
	}

	bool OnUserUpdate(float dt) override {
		player_up=(player_pos-GetScreenSize()/2).norm();
		
		//jump
		if(GetKey(olc::Key::W).bPressed) player_vel=player_up*cell_size.x*10;

		vf2d player_rgt(player_up.y, -player_up.x);
		if(GetKey(olc::Key::A).bHeld) player_pos+=8*cell_size.x*player_rgt*dt;
		if(GetKey(olc::Key::D).bHeld) player_pos-=8*cell_size.x*player_rgt*dt;
		
		//accelerate down?
		player_acc+=-6.2f*cell_size.x*player_up;

		//update player
		player_vel+=player_acc*dt;
		player_pos+=player_vel*dt;
		player_acc*=0;
		
		checkCollision(dt);
		
		//white background
		FillRectDecal({0, 0}, GetScreenSize(), olc::BLACK);
		
		renderGrid(olc::DARK_GREY);

		renderCells(olc::WHITE);
		
		//render player
		FillCircleDecal(player_pos, player_rad, olc::CYAN);
		
		DrawArrowDecal(player_pos, player_pos+2*player_rad*player_up, .2f, olc::RED);
		DrawArrowDecal(player_pos, player_pos+player_vel, .2f, olc::GREEN);

		return true;
	}
};

int main() {
	Example demo;
	bool fullscreen=false;
	bool vsync=true;
	if(demo.Construct(600, 600, 1, 1, fullscreen, vsync)) demo.Start();

	return 0;
}