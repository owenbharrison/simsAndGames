#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;
using olc::vi2d;

#include <stack>

#include "common/utils.h"
namespace cmn {
	vf2d polar(float rad, float angle) {
		return polar_generic<vf2d>(rad, angle);
	}
}

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Raycasting";
	}

	float cell_size=0;
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
	int mi=0, mj=0;
	float total_dt=0;

	bool show_grid=false;
	bool show_player=false;
	bool show_corners=false;
	bool show_flooded=false;

	vf2d player_pos;
	float player_rot=0;
	vf2d player_dir;
	const float player_rad=6;
	float player_fov=105*cmn::Pi/180;

	vf2d light_pos;
	bool hover_light=true;

	//primitive render helpers
	olc::Sprite* rect_sprite=nullptr;
	olc::Decal* rect_decal=nullptr;
	olc::Sprite* circle_sprite=nullptr;
	olc::Decal* circle_decal=nullptr;

	bool OnUserCreate() override {
		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";
		ConsoleCaptureStdOut(true);

		//setup grid spacing
		cell_size=40;
		width=ScreenWidth()/cell_size;
		height=ScreenHeight()/cell_size;

		//add cave gen cellular automata
		grid=new bool[width*height];
		memset(grid, false, width*height*sizeof(bool));

		//put player in middle
		player_pos=GetScreenSize()/2;

		//put light in middle
		light_pos=GetScreenSize()/2;

		//make some primitives to draw with
		{
			rect_sprite=new olc::Sprite(1, 1);
			rect_sprite->SetPixel(0, 0, olc::WHITE);
			rect_decal=new olc::Decal(rect_sprite);

			int circle_width=1024;
			const unsigned int rad_sq=circle_width*circle_width/4;
			circle_sprite=new olc::Sprite(circle_width, circle_width);
			for(int x=0; x<circle_width; x++) {
				int dx=x-circle_width/2;
				for(int y=0; y<circle_width; y++) {
					int dy=y-circle_width/2;
					circle_sprite->SetPixel(x, y, dx*dx+dy*dy<rad_sq?olc::WHITE:olc::BLANK);
				}
			}
			circle_decal=new olc::Decal(circle_sprite);
		}

		return true;
	}

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") {
			ConsoleClear();

			return true;
		}

		if(cmd=="reset") {
			std::cout<<"resetting grid\n";
			memset(grid, false, sizeof(bool)*width*height);

			return true;
		}

		if(cmd=="keybinds") {
			std::cout<<"useful keybinds:\n"
				"  P      toggle player\n"
				"  L      toggle mouse light\n"
				"  G      toggle grid lines\n"
				"  F      toggle flooded view\n"
				"  C      toggle corners view\n"
				" 1/2     grid set solid/air\n"
				"ARROWS   move player\n";

			return true;
		}

		if(cmd=="help") {
			std::cout<<"useful commands:\n"
				"  clear     clears the screen\n"
				"  reset     resets grid\n"
				" keybinds   which keys to press for this program?\n";

			return true;
		}

		std::cout<<"unknown command. type help for list of commands.\n";

		return false;
	}

#pragma region UPDATE HELPERS
	void handleCollisions(vf2d& pos, float rad) {
		//if player inside block, do nothing?
		//push to nearest edge somehow.
		int pi=pos.x/cell_size;
		int pj=pos.y/cell_size;
		if(inRangeX(pi)&&inRangeY(pj)&&grid[ix(pi, pj)]) return;

		//given player size, determine min area to check
		int si=(pos.x-rad)/cell_size;
		int ei=(pos.x+rad)/cell_size;
		int sj=(pos.y-rad)/cell_size;
		int ej=(pos.y+rad)/cell_size;

		//for each block
		for(int i=si; i<=ei; i++) {
			if(!inRangeX(i)) continue;
			for(int j=sj; j<=ej; j++) {
				if(!inRangeY(j)) continue;
				//only check solid
				if(!grid[ix(i, j)]) continue;

				//find close edge point
				vf2d close_pt(
					cmn::clamp(pos.x, cell_size*i, cell_size*(i+1)),
					cmn::clamp(pos.y, cell_size*j, cell_size*(j+1))
				);
				vf2d sub=pos-close_pt;
				float mag=sub.mag();
				if(mag<rad) {
					//resolve
					vf2d norm=sub/mag;
					pos+=(rad-mag)*norm;
				}
			}
		}
	}

	//dda raycasting
	//this is in grid space
	float traceRay(const vf2d& orig, const vf2d& dir) {
		//which tile am i checking
		int ci=orig.x, cj=orig.y;
		if(inRangeX(ci)&&inRangeY(cj)&&grid[ix(ci, cj)]) return 1e-6f;

		//find hypot scale factors
		vf2d hypot(
			std::sqrtf(1+dir.y*dir.y/dir.x/dir.x),
			std::sqrtf(1+dir.x*dir.x/dir.y/dir.y)
		);

		//which way am i going
		vf2d stepped;
		int step_i=-1, step_j=-1;

		//start conditions
		if(dir.x<0) stepped.x=hypot.x*(orig.x-ci);
		else step_i=1, stepped.x=hypot.x*(1+ci-orig.x);
		if(dir.y<0) stepped.y=hypot.y*(orig.y-cj);
		else step_j=1, stepped.y=hypot.y*(1+cj-orig.y);

		//walk
		for(int iter=0; iter<1000; iter++) {
			float dist;
			if(stepped.x<stepped.y) {
				ci+=step_i;
				dist=stepped.x;
				stepped.x+=hypot.x;
			} else {
				cj+=step_j;
				dist=stepped.y;
				stepped.y+=hypot.y;
			}

			//hit wall
			if(!inRangeX(ci)||!inRangeY(cj)) return -1;

			//hit block
			if(grid[ix(ci, cj)]) return dist;
		}

		return -1;
	}
#pragma endregion

	void update(float dt) {
		mouse_pos=GetMousePos();

		//player "update"
		player_dir=cmn::polar(1, player_rot);

		//where is mouse in grid?
		mi=mouse_pos.x/cell_size;
		mj=mouse_pos.y/cell_size;

		//open integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		//dont allow inputs in console
		if(!IsConsoleShowing()) {
			//ui toggles
			if(GetKey(olc::Key::P).bPressed) show_player^=true;
			if(GetKey(olc::Key::L).bPressed) hover_light^=true;
			if(GetKey(olc::Key::G).bPressed) show_grid^=true;
			if(GetKey(olc::Key::C).bPressed) show_corners^=true;
			if(GetKey(olc::Key::F).bPressed) show_flooded^=true;

			//set solid/air
			if(inRangeX(mi)&&inRangeY(mj)) {
				if(GetKey(olc::Key::K1).bHeld) grid[ix(mi, mj)]=true;
				if(GetKey(olc::Key::K2).bHeld) grid[ix(mi, mj)]=false;
			}

			//player movement
			if(GetKey(olc::Key::UP).bHeld) player_pos+=65*dt*player_dir;
			if(GetKey(olc::Key::DOWN).bHeld) player_pos-=50*dt*player_dir;
			if(GetKey(olc::Key::LEFT).bHeld) player_rot-=3*dt;
			if(GetKey(olc::Key::RIGHT).bHeld) player_rot+=3*dt;

			//move towards mouse
			if(hover_light) light_pos=mouse_pos;
		}

		handleCollisions(player_pos, player_rad);

		total_dt+=dt;
	}

#pragma region RENDER HELPERS
	void DrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float w, olc::Pixel col) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=sub.perp()/len;

		float angle=std::atan2f(sub.y, sub.x);
		DrawRotatedDecal(a-w/2*tang, rect_decal, angle, {0, 0}, {len, w}, col);
	}

	void FillCircleDecal(const olc::vf2d& pos, float rad, olc::Pixel col) {
		olc::vf2d offset(rad, rad);
		olc::vf2d scale{2*rad/circle_sprite->width, 2*rad/circle_sprite->width};
		DrawDecal(pos-offset, circle_decal, scale, col);
	}

	void renderGrid() {
		//vert lines
		for(int i=0; i<width; i++) {
			float x=cell_size*i;
			vf2d top(x, 0), btm(x, ScreenHeight());
			DrawLineDecal(top, btm, olc::GREY);
		}

		//horiz lines
		for(int j=0; j<height; j++) {
			float y=cell_size*j;
			vf2d lft(0, y), rgt(ScreenWidth(), y);
			DrawLineDecal(lft, rgt, olc::GREY);
		}
	}

	void renderCells() {
		for(int i=0; i<width; i++) {
			float x=cell_size*i;
			for(int j=0; j<height; j++) {
				if(grid[ix(i, j)]) {
					float y=cell_size*j;
					FillRectDecal({x, y}, {cell_size, cell_size}, olc::DARK_GREY);
				}
			}
		}
	}

	void renderLight() {
		//draw circles of decreasing brightness eminating from light_pos
		const int num=25;
		for(int i=0; i<num; i++) {
			float rad=cmn::map(i, 0, num-1, 250, 5);
			float shade=cmn::map(i, 0, num-1, 0, 1);
			FillCircleDecal(light_pos, rad, olc::PixelF(shade, shade, shade));
		}
	}

	//how to make faster:
	//find all shape edges
	//polar sort them
	//raycast to those
	//WAY less points
	void renderShadows() {
		//draw previous section
		std::vector<vf2d> shadow;
		auto triangulate=[this, &shadow] () {
			for(int i=3; i<shadow.size(); i+=2) {
				FillTriangleDecal(shadow[i-3], shadow[i-2], shadow[i-1], olc::BLACK);
				FillTriangleDecal(shadow[i-2], shadow[i-1], shadow[i], olc::BLACK);
			}
			shadow.clear();
		};

		//cast full circle of rays eminating from light_pos
		const int num_shadows=256;
		for(int i=0; i<=num_shadows; i++) {
			const vf2d orig=light_pos/cell_size;
			vf2d dir=cmn::polar(1, 2*cmn::Pi*i/num_shadows);
			float dist=traceRay(orig, dir);
			if(dist>0) {
				//point on cell
				vf2d st=orig+dist*dir;
				shadow.emplace_back(cell_size*st);
				//shadow extension
				vf2d en=st+500*dir;
				shadow.emplace_back(cell_size*en);
				continue;
			}

			//if we hit a gap:
			triangulate();
		}

		//could be fragmented due to initial angle
		triangulate();
	}

	void renderPlayerFOV() {
		//draw fov lines
		const int num_fov=24;
		const float max_dist=4.f;
		for(int i=0; i<num_fov; i++) {
			//setup ray dir
			float angle01=i/(num_fov-1.f);
			float angle55=angle01-.5f;
			float angle=player_rot+player_fov*angle55;
			vf2d dir=cmn::polar(1, angle);

			//trace ray
			float dist=traceRay(player_pos/cell_size, dir);
			bool hit=false;
			if(dist>0&&dist<max_dist) hit=true;
			else dist=max_dist;
			dist*=cell_size;

			//draw ray
			vf2d ix_pt=player_pos+dir*dist;
			DrawLineDecal(player_pos, ix_pt, hit?olc::RED:olc::GREY);
		}
	}

	void renderPlayer() {
		FillCircleDecal(player_pos, player_rad+2, olc::BLUE);
		FillCircleDecal(player_pos, player_rad, olc::WHITE);
		DrawThickLine(player_pos, player_pos+player_rad*player_dir, 2, olc::BLUE);
	}

	void renderCorners() {
		typedef unsigned char byte;

		std::vector<vi2d> corners;
		//for each block
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				//dont check air
				if(!grid[ix(i, j)]) continue;

				//for each neighbor
				byte state=0;
				for(int s=0; s<8; s++) {
					int ci=i, cj=j;
					switch(s) {
						case 0: cj--; break;//top
						case 1: cj--, ci--; break;//top left
						case 2: ci--; break;//left
						case 3: cj++, ci--; break;//bottom left
						case 4: cj++; break;//bottom
						case 5: cj++, ci++; break;//bottom right
						case 6: ci++; break;//right
						case 7: cj--, ci++; break;//top right
					}
					//bitbang if out of range or air
					if(!inRangeX(ci)||!inRangeY(cj)||!grid[ix(ci, cj)]) state|=1<<s;
				}

				//mask out what we want
				byte tl=0b00000111&state;
				byte bl=0b00011100&state;
				byte br=0b01110000&state;
				byte tr=0b11000001&state;

				//surrounded by: 3 air, 1 air, or 2 air
				if(tl==0b00000111||tl==0b00000010||tl==0b00000101) corners.emplace_back(i, j);
				if(bl==0b00011100||bl==0b00001000||bl==0b00010100) corners.emplace_back(i, j+1);
				if(br==0b01110000||br==0b00100000||br==0b01010000) corners.emplace_back(i+1, j+1);
				if(tr==0b11000001||tr==0b10000000||tr==0b01000001) corners.emplace_back(i+1, j);
			}
		}

		//show lines to corners
		for(const auto& c:corners) {
			DrawLineDecal(light_pos, cell_size*c, olc::CYAN);
		}

		//polarsort based about light_pos
		//left->right
		vf2d ref=light_pos/cell_size;
		std::sort(corners.begin(), corners.end(), [ref] (const vi2d& a, const vi2d& b) {
			//cross product
			vf2d la=a-ref, lb=b-ref;
			return la.x*lb.y>la.y*lb.x;
		});

		//show each corner as a red circle
		for(int i=0; i<corners.size(); i++) {
			auto str=std::to_string(i);
			vf2d offset(4*str.length(), 4);
			DrawStringDecal(cell_size*corners[i]-offset, str, olc::RED);
		}
	}

	//show each shape in a random color
	void renderShapes() {
		bool* filled=new bool[width*height];
		memset(filled, false, sizeof(bool)*width*height);

		typedef std::vector<vi2d> shape;

		//parts detection algorithm
		shape curr;
		std::vector<shape> shapes;
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				curr.clear();

				//iterative floodfill
				std::stack<vi2d> queue;
				queue.push({i, j});
				while(queue.size()) {
					vi2d c=queue.top();
					queue.pop();

					//dont fill air or refill
					int k=ix(c.x, c.y);
					if(!grid[k]||filled[k]) continue;

					//update fill grid
					filled[k]=true;
					//store it
					curr.emplace_back(c);

					//update neighbors if in range
					if(c.x>0) queue.push({c.x-1, c.y});
					if(c.y>0) queue.push({c.x, c.y-1});
					if(c.x<width-1) queue.push({c.x+1, c.y});
					if(c.y<height-1) queue.push({c.x, c.y+1});
				}

				//just making sure...
				if(!curr.empty()) shapes.emplace_back(curr);
			}
		}

		delete[] filled;

		//for every shape
		for(const auto& shp:shapes) {
			olc::Pixel col(rand()%255, rand()%255, rand()%255);
			//show all of its blocks
			for(const auto& b:shp) {
				FillRectDecal(cell_size*b, {cell_size, cell_size}, col);
			}
		}
	}
#pragma endregion

	void render(float dt) {
		Clear(olc::BLACK);

		renderLight();

		renderShadows();

		if(show_grid) renderGrid();

		if(show_player) renderPlayerFOV();

		if(show_flooded) {
			srand(2*total_dt);
			renderShapes();
		} else renderCells();

		//show corners?
		if(show_corners) renderCorners();

		//show hover cell
		if(show_grid) DrawRectDecal(cell_size*vf2d(mi, mj), cell_size*vf2d(1, 1), olc::GREEN);

		//show "player"
		if(show_player) renderPlayer();
	}

	bool OnUserUpdate(float dt) override {
		update(dt);

		render(dt);

		return true;
	}
};

int main() {
	Example demo;
	bool vsync=true;
	if(demo.Construct(600, 400, 1, 1, false, vsync)) demo.Start();

	return 0;
}