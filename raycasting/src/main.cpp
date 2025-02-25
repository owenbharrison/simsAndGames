#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

static constexpr float Pi=3.1415927f;

float clamp(const float& x, const float& a, const float& b) {
	if(x<a) return a;
	if(x>b) return b;
	return x;
}

float map(const float& x, const float& a, const float& b, const float& c, const float& d) {
	float t=(x-a)/(b-a);
	return c+t*(d-c);
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

	bool show_grid=false;
	bool show_player=false;

	vf2d player_pos;
	float player_rot=0;
	vf2d player_dir;
	const float player_rad=6;
	float player_fov=Pi*105/180;

	vf2d light_pos;
	bool hover_light=true;

	//primitive render helpers
	olc::Sprite* rect_sprite=nullptr;
	olc::Decal* rect_decal=nullptr;
	olc::Sprite* circle_sprite=nullptr;
	olc::Decal* circle_decal=nullptr;

	bool OnUserCreate() override {
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
				clamp(pos.x, cell_size*i, cell_size*(i+1)),
				clamp(pos.y, cell_size*j, cell_size*(j+1))
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
		while(true) {
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
	}
#pragma endregion

	void update(float dt) {
		mouse_pos=GetMousePos();

		//ui toggles
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;
		if(GetKey(olc::Key::P).bPressed) show_player^=true;
		if(GetKey(olc::Key::L).bPressed) hover_light^=true;

		//where is mouse in grid?
		mi=mouse_pos.x/cell_size;
		mj=mouse_pos.y/cell_size;
		if(inRangeX(mi)&&inRangeY(mj)) {
			//toggle cell
			if(GetKey(olc::Key::K1).bHeld) grid[ix(mi, mj)]=true;
			if(GetKey(olc::Key::K2).bHeld) grid[ix(mi, mj)]=false;
		}

		//player movement
		player_dir.x=std::cosf(player_rot);
		player_dir.y=std::sinf(player_rot);
		if(GetKey(olc::Key::UP).bHeld) player_pos+=65*dt*player_dir;
		if(GetKey(olc::Key::DOWN).bHeld) player_pos-=50*dt*player_dir;

		if(GetKey(olc::Key::LEFT).bHeld) player_rot-=3*dt;
		if(GetKey(olc::Key::RIGHT).bHeld) player_rot+=3*dt;

		//move towards mouse
		if(hover_light) light_pos=mouse_pos;

		handleCollisions(player_pos, player_rad);
	}

#pragma region RENDER HELPERS
	void DrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float w, olc::Pixel col) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=(sub/len).perp();

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
		for(float x=1; x>=0; x-=.033f) {
			float r=250*x;
			float shade=1-x;
			FillCircleDecal(light_pos, r, olc::PixelF(shade, shade, shade));
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
		auto triangulate=[this, &shadow]() {
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
			float angle=2*Pi*i/num_shadows;
			vf2d dir(std::cosf(angle), std::sinf(angle));
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

	void drawPlayerFOV() {
		//draw fov lines
		const int num_fov=24;
		const float max_dist=4.f;
		for(int i=0; i<num_fov; i++) {
			//setup ray dir
			float angle01=i/(num_fov-1.f);
			float angle55=angle01-.5f;
			float angle=player_rot+player_fov*angle55;
			vf2d dir(std::cosf(angle), std::sinf(angle));

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
#pragma endregion

	void render(float dt) {
		Clear(olc::BLACK);

		renderLight();

		renderShadows();

		if(show_grid) renderGrid();

		if(show_player) drawPlayerFOV();

		renderCells();

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
	bool vsync=false;
	if(demo.Construct(600, 400, 1, 1, false, vsync)) demo.Start();

	return 0;
}