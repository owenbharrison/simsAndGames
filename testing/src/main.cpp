#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "common/utils.h"
#include "common/aabb.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

#include "common/stopwatch.h"

struct Triangle {
	vf2d a, b, c;
};

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="wall edit testing";
	}

	bool editing=false;
	bool edits[4], prev_edits[4];
	int prev_mouse=-1;
	int ix(int i, int j) const {
		return i+2*j;
	}

	cmn::AABB edit_box;
	vf2d edit_box_start;

	cmn::AABB bounds[4];

	std::list<Triangle> triangles;

	void recalculateBounds() {
		static const float margin=10;
		vf2d wh=(edit_box.max-edit_box.min-3*margin)/2;
		for(int i=0; i<2; i++) {
			for(int j=0; j<2; j++) {
				vf2d ij(i, j);
				vf2d xy=edit_box.min+margin*(1+ij)+wh*ij;
				bounds[ix(i, j)]={xy, xy+wh};
			}
		}
	}

	bool OnUserCreate() override {
		for(int i=0; i<4; i++) {
			edits[i]=true;
			prev_mouse=-1;
		}

		edit_box={{0, 0}, vf2d(ScreenWidth(), ScreenHeight())};

		recalculateBounds();

		return true;
	}

	bool OnUserUpdate(float dt) override {
		const vf2d mouse_pos=GetMousePos();

		if(GetMouse(olc::Mouse::MIDDLE).bPressed) {
			edit_box_start=mouse_pos;
		}
		if(GetMouse(olc::Mouse::MIDDLE).bReleased) {
			edit_box=cmn::AABB();
			edit_box.fitToEnclose(edit_box_start);
			edit_box.fitToEnclose(mouse_pos);
			recalculateBounds();
		}

		if(editing) {
			if(GetMouse(olc::Mouse::RIGHT).bPressed) {
				for(int i=0; i<4; i++) edits[i]=true;
			}

			if(GetMouse(olc::Mouse::LEFT).bHeld) {
				//find position
				int curr_mouse=-1;
				for(int i=0; i<4; i++) {
					if(bounds[i].contains(mouse_pos)) {
						curr_mouse=i;
						break;
					}
				}
				//flip solids along swipe
				if(curr_mouse!=-1&&curr_mouse!=prev_mouse) edits[curr_mouse]^=true;
				prev_mouse=curr_mouse;
			}
			if(GetMouse(olc::Mouse::LEFT).bReleased) prev_mouse=-1;
		}

		if(GetKey(olc::Key::SHIFT).bPressed) {
			if(editing) {
				//evaluate edit
				int state=edits[0]+2*edits[1]+4*edits[2]+8*edits[3];
				int type=0, rot=0;
				//1=square, 2=bar, 3=L, 4=fill
				switch(state) {
					case 1: type=1; break;
					case 2: type=1, rot=1; break;
					case 8: type=1, rot=2; break;
					case 4: type=1, rot=3; break;
					case 3: type=2; break;
					case 10: type=2, rot=1; break;
					case 12: type=2, rot=2; break;
					case 5: type=2, rot=3; break;
					case 7: type=3; break;
					case 11: type=3, rot=1; break;
					case 14: type=3, rot=2; break;
					case 13: type=3, rot=3; break;
					case 15: type=4; break;
				}

				if(type==0) {
					//invalid edit
					for(int i=0; i<4; i++) edits[i]=prev_edits[i];
				} else {
					//calc vertex positions
					float xl=edit_box.min.x, xr=edit_box.max.x, xm=(xl+xr)/2;
					float yt=edit_box.min.y, yb=edit_box.max.y, ym=(yt+yb)/2;
					vf2d v[9];
					v[0]={xl, yt}, v[1]={xm, yt}, v[2]={xr, yt};
					v[7]={xl, ym}, v[8]={xm, ym}, v[3]={xr, ym};
					v[6]={xl, yb}, v[5]={xm, yb}, v[4]={xr, yb};
					//rotate vertexes
					for(int r=0; r<2*rot; r++) {
						vf2d temp=v[0];
						for(int i=0; i<8; i++) v[i]=v[i+1];
						v[7]=temp;
					}
					//tesselate
					switch(type) {
						case 1:
							triangles={
								{v[0], v[1], v[7]},
								{v[1], v[8], v[7]}
							};
							break;
						case 2:
							triangles={
								{v[0], v[2], v[7]},
								{v[2], v[3], v[7]}
							};
							break;
						case 3:
							triangles={
								{v[0], v[2], v[6]},
								{v[2], v[3], v[8]},
								{v[8], v[5], v[6]}
							};
							break;
						case 4:
							triangles={
								{v[0], v[2], v[6]},
								{v[2], v[4], v[6]}
							};
							break;
					}
				}
				prev_mouse=-1;
			} else {
				//store edits
				for(int i=0; i<4; i++) prev_edits[i]=edits[i];
			}
			editing^=true;
		}

		Clear(olc::BLACK);
		const olc::Pixel light_grey(230, 230, 230);

		//show boxes
		if(editing) {
			for(int i=0; i<2; i++) {
				for(int j=0; j<2; j++) {
					const auto& box=bounds[ix(i, j)];
					if(edits[ix(i, j)]) {
						FillRect(box.min, box.max-box.min, olc::Pixel(0, 100, 255));
						DrawRect(box.min, box.max-box.min, olc::BLUE);
					} else {
						DrawRect(box.min, box.max-box.min, light_grey);
					}
					DrawString(box.getCenter()-vf2d(8, 8), std::to_string(ix(i, j)), olc::BLACK, 2);
				}
			}
		} else {
			for(const auto& t:triangles) {
				FillTriangle(t.a, t.b, t.c, light_grey);
			}
			for(const auto& t:triangles) {
				DrawTriangle(t.a, t.b, t.c, olc::GREY);
			}
		}

		DrawRect(edit_box.min, edit_box.max-edit_box.min, olc::GREEN);

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(200, 200, 2, 2, false, true)) e.Start();

	return 0;
}