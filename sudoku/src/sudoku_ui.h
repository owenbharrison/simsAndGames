#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "sudoku.h"

struct SudokuUI : olc::PixelGameEngine {
	SudokuUI() {
		sAppName="Sudoku";
	}

	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;

	Sudoku game;
	float cell_size=0;

	int cursor_i=0, cursor_j=0;

	bool show_highlight=true;

	bool OnUserCreate() override {
		//primitive to draw with
		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);

		//load game from save
		if(!game.loadFromFile("assets/save.txt")) return false;

		//collapse possibilities
		for(int i=0; i<9; i++) {
			for(int j=0; j<9; j++) {
				game.collapseKnown(i, j);
				game.collapseRow(i, j);
				game.collapseColumn(i, j);
				game.collapseBox(i, j);
			}
		}

		cell_size=ScreenWidth()/9.f;

		return true;
	}

	bool OnUserDestroy() override {
		delete prim_rect_dec;
		delete prim_rect_spr;

		return true;
	}

	void DrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float rad, olc::Pixel col) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=(sub/len).perp();

		float angle=std::atan2f(sub.y, sub.x);
		DrawRotatedDecal(a-rad*tang, prim_rect_dec, angle, {0, 0}, {len, 2*rad}, col);
	}

	bool OnUserUpdate(float dt) override {
		//graphics toggles
		if(GetKey(olc::Key::H).bPressed) show_highlight^=true;
		
		//cursor movement
		if(GetKey(olc::Key::LEFT).bPressed) cursor_j--;
		if(GetKey(olc::Key::RIGHT).bPressed) cursor_j++;
		cursor_j=(cursor_j+9)%9;
		if(GetKey(olc::Key::UP).bPressed) cursor_i--;
		if(GetKey(olc::Key::DOWN).bPressed) cursor_i++;
		cursor_i=(cursor_i+9)%9;

		//debug
		if(GetKey(olc::Key::SPACE).bPressed) {
			struct Move {
				//position
				int i, j;
				//value
				int num;
			};

			//use next hints?
			std::list<Move> moves;
			for(int i=0; i<9; i++) {
				for(int j=0; j<9; j++) {
					int num=0, said;
					for(int k=0; k<9; k++) {
						if(game.possible[i][j][k]) {
							num++;
							said=1+k;
						}
					}
					if(num==1) moves.push_back({i, j, said});
				}
			}
			for(const auto& m:moves) {
				game.grid[m.i][m.j]=m.num;
			}

			//collapse possibilities
			for(int i=0; i<9; i++) {
				for(int j=0; j<9; j++) {
					game.collapseKnown(i, j);
					game.collapseRow(i, j);
					game.collapseColumn(i, j);
					game.collapseBox(i, j);
				}
			}
		}

		//rendering
		Clear(olc::WHITE);

		//draw cursor highlight
		{
			vf2d box_sz(cell_size, cell_size);
			if(show_highlight) {
				//which row is it in?
				const olc::Pixel light_blue(184, 211, 255);
				vf2d top(cell_size*cursor_j, 0);
				vf2d vert_sz(cell_size, ScreenHeight());
				FillRectDecal(top, vert_sz, light_blue);

				//which column is it in?
				vf2d lft(0, cell_size*cursor_i);
				vf2d horiz_sz(ScreenWidth(), cell_size);
				FillRectDecal(lft, horiz_sz, light_blue);

				//which box is it in?
				vf2d box_tl=cell_size*vf2d(cursor_j/3*3, cursor_i/3*3);
				FillRectDecal(box_tl, 3*box_sz, light_blue);
			}

			//actual cursor spot
			vf2d tl=cell_size*vf2d(cursor_j, cursor_i);
			FillRectDecal(tl, box_sz, olc::Pixel(110, 165, 255));
		}

		//vertical lines
		for(int i=0; i<=9; i++) {
			float x=cell_size*i;
			vf2d top(x, 0);
			vf2d btm(x, ScreenHeight());
			if(i%3==0) DrawThickLine(top, btm, 4, olc::BLACK);
			else DrawLineDecal(top, btm, olc::BLACK);
		}

		//horizontal lines
		for(int j=0; j<=9; j++) {
			float y=cell_size*j;
			vf2d lft(0, y);
			vf2d rgt(ScreenWidth(), y);
			if(j%3==0) DrawThickLine(lft, rgt, 4, olc::BLACK);
			else DrawLineDecal(lft, rgt, olc::BLACK);
		}

		const vf2d scl(3.4f, 3.4f);
		for(int i=0; i<9; i++) {
			float y=cell_size*(.5f+i);
			for(int j=0; j<9; j++) {
				float x=cell_size*(.5f+j);

				//show num
				if(game.grid[i][j]) {
					std::string str=std::to_string(game.grid[i][j]);
					DrawStringDecal(vf2d(x, y)-4*scl, str, olc::BLACK, scl);
				}
				
				//show possibilities
				else {
					std::string str="";
					for(int k=0; k<9; k++) {
						if(game.possible[i][j][k]) {
							str+=char('1'+k);
						}
					}
					int sz=str.length();
					if(sz==0||sz>3)  continue;
					
					str="\n\n"+str;
					vf2d offset=4*scl/3*vf2d(sz, 2);
					DrawStringDecal(vf2d(x, y)-offset, str, olc::DARK_GREY, scl/3);
				}
			}
		}

		return true;
	}
};