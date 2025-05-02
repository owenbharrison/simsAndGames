#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;
using olc::vi2d;

#include "common/utils.h"
#include "common/aabb.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

#include "common/stopwatch.h"

struct Rect {
	vi2d ij, wh;
};

//sorted by descending area?
static const std::list<vi2d> allowed_sizes{
	{2, 10}, {2, 8}, {4, 4}, {2, 6},
	{3, 4}, {1, 10}, {3, 3}, {1, 8},
	{2, 4}, {1, 6}, {2, 3}, {1, 4},
	{2, 2}, {1, 3}, {1, 2}, {1, 1}
};

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="wall edit testing";
	}

	//"primitive" render helpers
	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;

	int width=0, height=0, depth=0;
	bool* grid=nullptr;
	int ix(int i, int j, int k) const {
		return i+width*(j+height*k);
	}
	std::list<Rect>* rects=nullptr;
	int list_index=0;

	cmn::AABB box;
	vf2d box_start;

	void reset() {
		delete[] grid;

		//size for each dimension: 10-50
		width=10+rand()%41;
		height=10+rand()%41;
		depth=10+rand()%41;
		grid=new bool[width*height*depth];
		
		float a=.5f*width;
		float b=.5f*height;
		float c=.5f*depth;
		for(int i=0; i<width; i++) {
			float x=i-a;
			for(int j=0; j<height; j++) {
				float y=j-b;
				for(int k=0; k<depth; k++) {
					float z=k-c;
					float lhs=x*x/a/a+y*y/b/b+z*z/c/c;
					grid[ix(i, j, k)]=lhs>.5&&lhs<1;
				}
			}
		}

		reslice();
	}

	void reslice() {
		delete[] rects;
		rects=new std::list<Rect>[height];
		
		list_index=0;

		bool* meshed=new bool[width*depth];
		auto ik_ix=[this] (int i, int k) {
			return i+width*k;
		};

		int dims[2]{width, depth};
		int ik[2];

		for(int j=0; j<height; j++) {
			memset(meshed, false, sizeof(bool)*width*depth);
			//flip prioritized axis each slice
			const int axis_a=j%2, axis_b=(axis_a+1)%2;
			const int dim_a=dims[axis_a], dim_b=dims[axis_b];
			for(int a=0; a<dim_a; a++) {
				for(int b=0; b<dim_b; b++) {
					ik[axis_a]=a, ik[axis_b]=b;

					//skip if air or meshed
					if(!grid[ix(ik[0], j, ik[1])]||meshed[ik_ix(ik[0], ik[1])]) continue;

					//try combine in x
					int sz_a=1;
					for(int a_=1+a; a_<dim_a; a_++, sz_a++) {
						//stop if air or meshed
						ik[axis_a]=a_, ik[axis_b]=b;
						if(!grid[ix(ik[0], j, ik[1])]||meshed[ik_ix(ik[0], ik[1])]) break;
					}

					//try combine combination in z
					int sz_b=1;
					for(int b_=1+b; b_<dim_b; b_++, sz_b++) {
						bool able=true;
						for(int a_=0; a_<sz_a; a_++) {
							//stop if air or meshed
							ik[axis_a]=a+a_, ik[axis_b]=b_;
							if(!grid[ix(ik[0], j, ik[1])]||meshed[ik_ix(ik[0], ik[1])]) {
								able=false;
								break;
							}
						}
						if(!able) break;
					}
					
					//limit size to available blocks
					for(const auto& as:allowed_sizes) {
						if(as.x<=sz_a&&as.y<=sz_b) {
							sz_a=as.x, sz_b=as.y;
							break;
						}
						if(as.x<=sz_b&&as.y<=sz_a) {
							sz_b=as.x, sz_a=as.y;
							break;
						}
					}

					//set meshed
					for(int a_=0; a_<sz_a; a_++) {
						for(int b_=0; b_<sz_b; b_++) {
							ik[axis_a]=a+a_, ik[axis_b]=b+b_;
							meshed[ik_ix(ik[0], ik[1])]=true;
						}
					}

					//append to meshlist
					Rect r;
					ik[axis_a]=a, ik[axis_b]=b;
					r.ij.x=ik[0], r.ij.y=ik[1];
					ik[axis_a]=sz_a, ik[axis_b]=sz_b;
					r.wh.x=ik[0], r.wh.y=ik[1];
					rects[j].push_back(r);
				}
			}
		}

		delete[] meshed;
	}

	bool OnUserCreate() override {
		srand(time(0));

		//make some "primitives" to draw with
		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);

		reset();

		auto margin=23;
		vf2d offset(margin, margin);
		vf2d resolution(ScreenWidth(), ScreenHeight());
		box={offset, resolution-offset};

		return true;
	}

	bool OnUserDestroy() override {
		delete[] grid;

		return true;
	}

	void DrawThickLine(const vf2d& a, const vf2d& b, float rad, const olc::Pixel& col) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=sub.perp()/len;

		float angle=std::atan2f(sub.y, sub.x);
		DrawRotatedDecal(a-rad*tang, prim_rect_dec, angle, {0, 0}, {len, 2*rad}, col);
	}

	void DrawThickRect(const vf2d& tl, const vf2d& size, float rad, const olc::Pixel& col) {
		vf2d br=tl+size;
		vf2d tr(br.x, tl.y);
		vf2d bl(tl.x, br.y);
		DrawThickLine(tl, tr, rad, col);
		DrawThickLine(tr, br, rad, col);
		DrawThickLine(br, bl, rad, col);
		DrawThickLine(bl, tl, rad, col);
	}

	bool OnUserUpdate(float dt) override {
		const vf2d mouse_pos=GetMousePos();

		//reset grid
		if(GetKey(olc::Key::R).bPressed) reset();
		
		//slice thru layers
		if(GetKey(olc::Key::UP).bPressed||GetMouseWheel()>0) list_index++;
		if(GetKey(olc::Key::DOWN).bPressed||GetMouseWheel()<0) list_index--;
		list_index=(list_index+height)%height;

		//grey background, black slice box
		Clear(olc::WHITE);
		vf2d dim=box.max-box.min;
		FillRect(box.min, dim, olc::BLACK);

		//show slice rects
		vf2d sz=dim/vf2d(width, depth);
		for(const auto& r:rects[list_index]) {
			//choose color based on efficiency?
			olc::Pixel col=olc::RED;
			switch((r.wh.x>1)+(r.wh.y>1)) {
				case 1: col=olc::YELLOW; break;
				case 2: col=olc::BLUE; break;
			}

			//show rect with outline
			vf2d xy=box.min+sz*r.ij;
			vf2d wh=sz*r.wh;
			FillRectDecal(xy, wh, olc::GREY);
			DrawThickRect(xy, wh, 1, col);

			//show size on it
			std::string str=std::to_string(r.wh.x)+"x"+std::to_string(r.wh.y);
			vf2d offset(4*str.length(), 4);
			DrawStringDecal(xy+wh/2-offset, str, olc::BLACK);
		}

		//show layer
		DrawString(0, 0, "layer "+std::to_string(list_index));

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(600, 600, 1, 1, false, true)) e.Start();

	return 0;
}