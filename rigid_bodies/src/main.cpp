/*todo
	store graphics meshes on init
	store convex collision meshes on init
		use ear clipping triangulation?
		how to break concave -> list<convex>?
*/

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "shape.h"

struct RigidBodyUI : olc::PixelGameEngine {
	RigidBodyUI() {
		sAppName="Rigid Body Simulation";
	}

	//main shape list
	std::list<Shape*> shapes;

	//user input stuff
	vf2d mouse_pos;

	std::list<vf2d> addition;
	float addition_timer=0;

	Shape* spring_shape=nullptr;
	vf2d spring_offset;

	//graphics toggles
	bool show_bounds=false;
	bool show_masses=false;
	bool show_vertexes=false;

	//place shapes such that their bounds lie inside the screen
	void placeShapesRandomly() {
		//dont place ontop of other shapes.
		std::list<Shape*> to_check_against;
		for(const auto& s:shapes) {
			if(s->locked) to_check_against.push_back(s);
		}
		//place all non locked shapes
		for(const auto& s:shapes) {
			if(s->locked) continue;

			//random rotation
			s->rot=cmn::random(2*cmn::Pi);
			s->updateRot();
			//reset rot vel
			s->old_rot=s->rot;

			//assume erroneous position
			bool overlaps=true;
			cmn::AABB box=s->getAABB();
			do {
				//easier to just offset based on where it already is
				s->pos.x+=cmn::random(-box.min.x, ScreenWidth()-box.max.x);
				s->pos.y+=cmn::random(-box.min.y, ScreenHeight()-box.max.y);
				box=s->getAABB();

				//check all prior shapes
				overlaps=false;
				for(const auto& o:to_check_against) {
					if(box.overlaps(o->getAABB())) {
						overlaps=true;
						break;
					}
				}
			} while(overlaps);
			//reset vel
			s->old_pos=s->pos;

			//we now have to also check against this.
			to_check_against.push_back(s);
		}
	}

	bool OnUserCreate() override {
		std::srand(std::time(0));

		//add walls
		{
			const float sz=5;
			Shape* top=new Shape(cmn::AABB{vf2d(-sz, -sz), vf2d(ScreenWidth()+sz, sz)});
			top->locked=true;
			Shape* right=new Shape(cmn::AABB{vf2d(ScreenWidth()-sz, -sz), vf2d(ScreenWidth()+sz, ScreenHeight()+sz)});
			right->locked=true;
			Shape* bottom=new Shape(cmn::AABB{vf2d(-sz, ScreenHeight()-sz), vf2d(ScreenWidth()+sz, ScreenHeight()+sz)});
			bottom->locked=true;
			Shape* left=new Shape(cmn::AABB{vf2d(-sz, -sz), vf2d(sz, ScreenHeight()+sz)});
			left->locked=true;
			shapes.push_back(top);
			shapes.push_back(right);
			shapes.push_back(bottom);
			shapes.push_back(left);
		}

		shapes.push_back(new Shape({0, 0}, 100, 16));
		shapes.push_back(new Shape({0, 0}, 50, 3));
		shapes.push_back(new Shape({0, 0}, 75, 4));

		placeShapesRandomly();

		return true;
	}

	void reset() {
		spring_shape=nullptr;

		for(const auto& s:shapes) delete s;
		shapes.clear();
	}

	bool OnUserDestroy() override {
		reset();

		return true;
	}

	void handleUserInput(float dt) {
		//spring interactivity
		const auto l_mouse=GetMouse(olc::Mouse::LEFT);
		if(l_mouse.bPressed) {
			spring_shape=nullptr;
			for(const auto& s:shapes) {
				if(s->contains(mouse_pos)) {
					spring_shape=s;
					spring_offset=s->worldToLocal(mouse_pos);
					break;
				}
			}
		}
		if(l_mouse.bHeld) {
			if(spring_shape) {
				//F=kx
				vf2d spr_pos=spring_shape->localToWorld(spring_offset);
				vf2d sub=mouse_pos-spr_pos;
				//spring force proportional to shape mass
				spring_shape->applyForce(spring_shape->mass*sub, spr_pos);
			}
		}
		if(l_mouse.bReleased) spring_shape=nullptr;

		//add objects
		const auto a_key=GetKey(olc::Key::A);
		if(a_key.bPressed) {
			addition.clear();
		}
		//every now and then
		if(addition_timer<0) {
			addition_timer=.05f;

			if(a_key.bHeld) {
				//make sure not too close
				bool exists=false;
				for(const auto& a:addition) {
					if((a-mouse_pos).mag()<5) {
						exists=true;
						break;
					}
				}
				//add mouse point
				if(!exists) addition.push_back(mouse_pos);
			}
		}
		addition_timer-=dt;
		if(a_key.bReleased) {
			//add shape IF at least a triangle.
			if(addition.size()>2) shapes.push_back(new Shape(addition));
			addition.clear();
		}

		//remove objects
		if(GetKey(olc::Key::X).bHeld) {
			spring_shape=nullptr;

			for(auto it=shapes.begin(); it!=shapes.end();) {
				const auto& s=*it;
				if(s->contains(mouse_pos)) {
					delete s;
					it=shapes.erase(it);
				} else it++;
			}
		}

		//graphics toggles
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
		if(GetKey(olc::Key::M).bPressed) show_masses^=true;
		if(GetKey(olc::Key::V).bPressed) show_vertexes^=true;
		if(GetKey(olc::Key::R).bPressed) placeShapesRandomly();
	}

	void handlePhysics(float dt) {
		//collisions
		for(auto ait=shapes.begin(); ait!=shapes.end(); ait++) {
			for(auto bit=std::next(ait); bit!=shapes.end(); bit++) {
				Shape::collide(**ait, **bit, dt);
			}
		}

		//updating
		for(const auto& s:shapes) {
			//s->applyForce(s->mass*gravity, s->pos);
			s->update(dt);
		}
	}

	bool OnUserUpdate(float dt) override {
		mouse_pos=GetMousePos();
		
		handleUserInput(dt);

		handlePhysics(dt);

#pragma region RENDER
		Clear(olc::GREY);

		//show addition
		if(addition.size()) {
			bool is_first=true;
			vf2d first, prev;
			for(const auto& a:addition) {
				if(is_first) first=a, is_first=false;
				else DrawLineDecal(prev, a, olc::BLUE);
				prev=a;
			}
			DrawLineDecal(first, prev, olc::BLUE);
		}

		//show all shapes
		for(const auto& s:shapes) {
			if(show_bounds) {
				cmn::AABB box=s->getAABB();
				bool overlaps=false;
				for(const auto& o:shapes) {
					if(o==s) continue;

					if(box.overlaps(o->getAABB())) {
						overlaps=true;
						break;
					}
				}

				vf2d tr(box.max.x, box.min.y), bl(box.min.x, box.max.y);
				olc::Pixel col=overlaps?olc::RED:olc::GREEN;
				DrawLineDecal(box.min, tr, col);
				DrawLineDecal(tr, box.max, col);
				DrawLineDecal(box.max, bl, col);
				DrawLineDecal(bl, box.min, col);
			}

			//fill
			{
				//initialize indexes
				std::list<int> indexes;
				for(int i=0; i<s->getNum(); i++) indexes.push_back(i);

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
							s->localToWorld(pt_n),
							s->col
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

			//vertexes
			if(show_vertexes) for(int i=0; i<s->getNum(); i++) {
				const auto& a=s->points[i];
				auto str=std::to_string(i);
				vf2d offset(4*str.size(), 4);
				DrawStringDecal(
					s->localToWorld(a)-offset,
					str,
					olc::BLUE
				);
			}

			if(show_masses) {
				DrawLineDecal(vf2d(s->pos.x-15, s->pos.y), vf2d(s->pos.x+15, s->pos.y), olc::BLACK);
				DrawLineDecal(vf2d(s->pos.x, s->pos.y-15), vf2d(s->pos.x, s->pos.y+15), olc::BLACK);
			}
		}

		//show spring shape
		if(spring_shape) {
			vf2d spr_pos=spring_shape->localToWorld(spring_offset);
			DrawLineDecal(spr_pos, mouse_pos, olc::MAGENTA);
		}
#pragma endregion

		return true;
	}
};

int main() {
	RigidBodyUI rbui;
	if(rbui.Construct(600, 800, 1, 1, false, true)) rbui.Start();

	return 0;
}