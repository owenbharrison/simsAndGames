#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

static constexpr float Pi=3.1415927f;

float clamp(const float& x, const float& a, const float& b) {
	if(x<a) return a;
	if(x>b) return b;
	return x;
}

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Raycasting";
	}

	float size=0;
	int width=0, height=0;
	bool* grid=nullptr;
	//grid helpers
	int ix(int i, int j) const {
		return i+width*j;
	}
	bool inRangeX(int i) const {
		return i>=0&&i<width;
	}
	bool inRangeY(int j) const {
		return j>=0&&j<height;
	}

	vf2d mouse_pos;

	bool show_grid=true;

	vf2d player_pos;
	float player_rot=0;
	const float player_rad=9;
	float player_fov=Pi*120/180;

	bool OnUserCreate() override {
		size=40;
		width=ScreenWidth()/size;
		height=ScreenHeight()/size;

		grid=new bool[width*height];
		memset(grid, false, width*height*sizeof(bool));

		player_pos=GetScreenSize()/2;

		return true;
	}

#pragma region RENDER HELPERS
	void renderGrid() {
		//vert lines
		for(int i=0; i<width; i++) {
			float x=size*i;
			vf2d top(x, 0), btm(x, ScreenHeight());
			DrawLine(top, btm, olc::BLACK);
		}

		//horiz lines
		for(int j=0; j<height; j++) {
			float y=size*j;
			vf2d lft(0, y), rgt(ScreenWidth(), y);
			DrawLine(lft, rgt, olc::BLACK);
		}
	}

	void renderCells() {
		for(int i=0; i<width; i++) {
			float x=size*i;
			for(int j=0; j<height; j++) {
				if(grid[ix(i, j)]) {
					float y=size*j;
					FillRect(x, y, size, size);
				}
			}
		}
	}
#pragma endregion

	bool OnUserUpdate(float dt) override {
		mouse_pos=GetMousePos();

		//ui toggles
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;

		//where is mouse in grid?
		int mi=mouse_pos.x/size;
		int mj=mouse_pos.y/size;
		if(inRangeX(mi)&&inRangeY(mj)) {
			//toggle cell
			if(GetKey(olc::Key::K1).bHeld) grid[ix(mi, mj)]=true;
			if(GetKey(olc::Key::K2).bHeld) grid[ix(mi, mj)]=false;
		}

		//player movement
		vf2d player_dir(std::cosf(player_rot), std::sinf(player_rot));
		if(GetKey(olc::Key::UP).bHeld) player_pos+=60*dt*player_dir;
		if(GetKey(olc::Key::DOWN).bHeld) player_pos-=45*dt*player_dir;

		if(GetKey(olc::Key::LEFT).bHeld) player_rot-=3*dt;
		if(GetKey(olc::Key::RIGHT).bHeld) player_rot+=3*dt;

		if(GetKey(olc::Key::P).bPressed) player_pos=mouse_pos;

		Clear(olc::GREY);

		if(show_grid) renderGrid();

		renderCells();

		//show hover cell
		if(show_grid) DrawRect(size*mi, size*mj, size, size, olc::GREEN);

		//collisions
		{
			//given player size, determine min area to check
			int si=(player_pos.x-player_rad)/size;
			int ei=(player_pos.x+player_rad)/size;
			int sj=(player_pos.y-player_rad)/size;
			int ej=(player_pos.y+player_rad)/size;
			//for each block
			for(int i=si; i<=ei; i++) {
				if(!inRangeX(i)) continue;
				for(int j=sj; j<=ej; j++) {
					if(!inRangeY(j)) continue;
					//only check solid
					if(!grid[ix(i, j)]) continue;

					//find close edge point
					vf2d close_pt(
					clamp(player_pos.x, size*i, size*(i+1)),
					clamp(player_pos.y, size*j, size*(j+1))
					);
					vf2d sub=player_pos-close_pt;
					float mag=sub.mag();
					if(mag<player_rad) {
						//resolve
						vf2d norm=sub/mag;
						player_pos+=(player_rad-mag)*norm;
					}
				}
			}
		}

		//show rays
		const int num=16;
		for(int i=0; i<num; i++) {
			float angle01=i/(num-1.f);
			float angle55=angle01-.5f;
			float angle=player_rot+player_fov*angle55;
			vf2d dir(std::cosf(angle), std::sinf(angle));
			DrawLine(player_pos, player_pos+75*dir);
		}

		//show "player"
		FillCircle(player_pos, player_rad);
		DrawCircle(player_pos, player_rad, olc::BLUE);
		DrawLine(player_pos, player_pos+player_rad*player_dir, olc::BLUE);

		return true;
	}
};

int main() {
	Example demo;
	bool vsync=true;
	if(demo.Construct(360, 360, 2, 2, false, vsync)) demo.Start();

	return 0;
}