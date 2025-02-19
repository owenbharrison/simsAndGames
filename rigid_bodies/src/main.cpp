/*todo
	store graphics meshes on init
	store convex collision meshes on init
		use ear clipping triangulation?
		how to break concave -> list<convex>?
*/

#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "shape.h"

struct RigidBodyDemo : olc::PixelGameEngine {
	RigidBodyDemo() {
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
	void placeAllRandomly() {
		for(const auto& s:shapes) {
			//random rotation
			s->rot=random(2*Pi);
			//reset rot vel
			s->old_rot=s->rot;

			//aabb needs cossin
			s->updateRot();
			AABB box=s->getAABB();

			//easier to just offset based on where it already is
			s->pos.x+=random(-box.min.x, ScreenWidth()-box.max.x);
			s->pos.y+=random(-box.min.y, ScreenHeight()-box.max.y);
			//reset vel
			s->old_pos=s->pos;
		}
	}

	bool OnUserCreate() override {
		srand(time(0));

		shapes.emplace_back(new Shape({0, 0}, 100, 16));
		shapes.emplace_back(new Shape({0, 0}, 50, 3));
		shapes.emplace_back(new Shape({0, 0}, 75, 4));

		placeAllRandomly();

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

	bool OnUserUpdate(float dt) override {
#pragma region USER_INPUT
		mouse_pos=GetMousePos();

		//graphics toggles
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
		if(GetKey(olc::Key::M).bPressed) show_masses^=true;
		if(GetKey(olc::Key::V).bPressed) show_vertexes^=true;
		if(GetKey(olc::Key::R).bPressed) placeAllRandomly();

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
				if(!exists) addition.emplace_back(mouse_pos);
			}
		}
		addition_timer-=dt;
		if(a_key.bReleased) {
			//add shape IF at least a triangle.
			if(addition.size()>2) shapes.emplace_back(new Shape(addition));
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
#pragma endregion

#pragma region PHYSICS
#if 0
		//seperating axis theorem
		for(const auto& s:shapes) s->col=olc::BLACK;
		
		//for every shape pair
		for(auto ait=shapes.begin(); ait!=shapes.end(); ait++) {
			auto& sa=*ait;
			for(auto bit=std::next(ait); bit!=shapes.end(); bit++) {
				auto& sb=*bit;

				//get combined list of axes
				std::list<vf2d> axes;
				for(int j=0; j<2; j++) {
					const auto& s=j?sa:sb;
					//find tangents of shape's edges
					for(int i=0; i<s->getNum(); i++) {
						const auto& a=s->localToWorld(s->points[i]);
						const auto& b=s->localToWorld(s->points[(i+1)%s->getNum()]);
						axes.emplace_back((b-a).perp().norm());
					}
				}
				
				float record=INFINITY;

				//for all axes
				bool collide=true;
				for(const auto& axis:axes) {
					float a_min=INFINITY, a_max=-a_min;
					float b_min=a_min, b_max=a_max;
					int a_min_ix, a_max_ix;
					int b_min_ix, b_max_ix;
					//for each shape
					for(int j=0; j<2; j++) {
						const auto& s=j?sa:sb;
						float& min=j?a_min:b_min;
						float& max=j?a_max:b_max;
						int& min_ix=j?a_min_ix:b_min_ix;
						int& max_ix=j?a_min_ix:b_max_ix;
						//project points onto said axis
						for(int i=0; i<s->getNum(); i++) {
							vf2d pt=s->localToWorld(s->points[i]);
							float proj=pt.dot(axis);
							if(proj<min) min=proj, min_ix=i;
							if(proj>max) max=proj, max_ix=i;
						}
					}

					//if no separation, objects DONT collide
					if(a_max<b_min||b_max<a_min) {
						collide=false;
						break;
					}

					float dist=std::min(std::abs(a_max-b_min), std::abs(b_max-a_min));
					if(dist<record) {
						record=dist;
					}
				}
			
				//display whether colliding
				if(collide) {
					sa->col=olc::RED;
					sb->col=olc::RED;
				}
			}
		}
#endif
		{
			struct Contact {
				Shape* s_a, *s_b;
				vf2d pt_a, pt_b;
				vf2d norm;
				float dist;
			};
			for(const auto& sa:shapes) {
				for(const auto& sb:shapes) {
					//dont check self
					if(sb==sa) continue;

					//list of both shapes' edge norms
					std::list<vf2d> axes;
					for(int j=0; j<2; j++) {
						//for both shapes
						Shape* s=j?sa:sb;
						for(int i=0; i<s->getNum(); i++) {
							//perpendicular vector on edge
							const auto& a=s->localToWorld(s->points[i]);
							const auto& b=s->localToWorld(s->points[(i+1)%s->getNum()]);
							axes.emplace_back((a-b).norm().perp());
						}
					}


				}
			}
		}

		for(const auto& s:shapes) {
			//s->applyForce(s->mass*gravity, s->pos);
			s->update(dt);
		}
#pragma endregion

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
				AABB box=s->getAABB();
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
					s->col
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
	RigidBodyDemo rbd;
	if(rbd.Construct(600, 800, 1, 1, false, true)) rbd.Start();

	return 0;
}