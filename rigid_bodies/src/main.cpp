#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "shape.h"

struct RigidBodyDemo : olc::PixelGameEngine {
	RigidBodyDemo() {
		sAppName="Rigid Body Simulation";
	}

	std::list<Shape*> shapes;

	vf2d mouse_pos;

	bool show_bounds=false;

	bool OnUserCreate() override {
		shapes.emplace_back(new Shape(.5f*vf2d(ScreenWidth(), ScreenHeight()), 50, 12));

		return true;
	}

	void reset() {
		for(const auto& s:shapes) delete s;
		shapes.clear();
	}

	bool OnUserDestroy() override {
		reset();

		return true;
	}

	bool OnUserUpdate(float dt) override {
#pragma region USER_INPUT
		mouse_pos=GetMousePos();

		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
#pragma endregion

#pragma region PHYSICS
		for(const auto& s:shapes) {
			//s->applyForce(s->mass*gravity, s->pos);
			//s->update(dt);
		}
#pragma endregion

#pragma region RENDER
		Clear(olc::GREY);

		//show all shapes
		for(const auto& s:shapes) {
			if(show_bounds) {
				AABB box=s->getAABB();
				vf2d tr(box.max.x, box.min.y), bl(box.min.x, box.max.y);
				DrawLineDecal(box.min, tr, olc::RED);
				DrawLineDecal(tr, box.max, olc::RED);
				DrawLineDecal(box.max, bl, olc::RED);
				DrawLineDecal(bl, box.min, olc::RED);
			}

			//fill
			{
				//initialize indexes
				std::list<int> indexes;
				for(int i=0; i<s->getNum(); i++) indexes.emplace_back(i);

				//ear clipping triangulation
				for(auto curr=indexes.begin(); curr!=indexes.end();) {
					//get previous and next points
					auto prev=std::prev(curr==indexes.begin()?indexes.end():curr);
					auto next=std::next(curr); if(next==indexes.end()) next=indexes.begin();
					const auto& pt_p=s->points[*prev];
					const auto& pt_c=s->points[*curr];
					const auto& pt_n=s->points[*next];

					//make sure this is a convex angle
					if((pt_p-pt_c).cross(pt_n-pt_c)>0) {
						curr++;
						continue;
					}

					//make sure this triangle doesnt contain any pts
					bool contains=false;
					for(auto other=std::next(curr); other!=indexes.end(); other++) {
						//dont check self
						if(other==next) continue;

						//is this point to the left/right of all trilines
						const auto& pt_o=s->points[*other];
						bool side1=(pt_c-pt_p).cross(pt_o-pt_p)>0;
						bool side2=(pt_n-pt_c).cross(pt_o-pt_c)>0;
						bool side3=(pt_p-pt_n).cross(pt_o-pt_n)>0;
						if(side1==side2&&side2==side3) {
							contains=true;
							break;
						}
					}

					//this is an ear!
					if(!contains) {
						FillTriangleDecal(
							s->localToWorld(pt_p),
							s->localToWorld(pt_c),
							s->localToWorld(pt_n)
						);

						//remove this index and start over
						indexes.erase(curr);
						curr=indexes.begin();
					} else curr++;
				}
			}

			//outlines
			for(int i=0; i<s->getNum(); i++) {
				const auto& a=s->points[i];
				const auto& b=s->points[(i+1)%s->getNum()];
				DrawLineDecal(
					s->localToWorld(a),
					s->localToWorld(b),
					olc::BLACK
				);
			}
		}
#pragma endregion

		return true;
	}
};

int main() {
	RigidBodyDemo rbd;
	if(rbd.Construct(600, 800, 1, 1, false, true)) rbd.Start();

	return 0;
}