#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "point_mass.h"
#include "shape.h"

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Example";
	}

	//ui stuff
	vf2d mouse_pos;

	std::list<vf2d> addition;
	float addition_timer=0;

	PointMass mouse_point;
	std::list<Constraint> mouse_constraint;

	//simulation stuff
	std::list<Shape*> shapes;

	//graphics stuff
	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;
	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;

	bool show_vertexes=false;
	bool show_springs=false;

	bool OnUserCreate() override {
		//make some "primitives" to draw with
		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);
		{
			int sz=1024;
			prim_circ_spr=new olc::Sprite(sz, sz);
			SetDrawTarget(prim_circ_spr);
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2);
			SetDrawTarget(nullptr);
			prim_circ_dec=new olc::Decal(prim_circ_spr);
		}

		mouse_point.locked=true;

		return true;
	}

	void resetMarkers() {
		mouse_constraint.clear();
	}

	void reset() {
		resetMarkers();

		for(const auto& s:shapes) delete s;
		shapes.clear();
	}

	bool OnUserDestroy() override {
		reset();

		delete prim_rect_dec;
		delete prim_rect_spr;
		delete prim_circ_dec;
		delete prim_circ_spr;

		return true;
	}

	void update(float dt) {
		mouse_pos=GetMousePos();

		//mouse constraint
		const auto mouse_constraint_action=GetMouse(olc::Mouse::LEFT);
		if(mouse_constraint_action.bPressed) {
			mouse_constraint.clear();

			Shape* said_shape=nullptr;
			for(const auto& s:shapes) {
				if(s->contains(mouse_pos)) {
					said_shape=s;
					break;
				}
			}
			if(said_shape) {
				for(size_t i=0; i<said_shape->getNum(); i++) {
					mouse_constraint.emplace_back(&mouse_point, &said_shape->points[i]);
				}
			}
		}
		if(mouse_constraint_action.bReleased) {
			mouse_constraint.clear();
		}

		//shape addition
		const auto add_action=GetKey(olc::Key::A);
		if(add_action.bPressed) {
			addition.clear();
		}
		//every now and then,
		if(add_action.bHeld) {
			//ensure points not "overlapping"
			bool unique=true;
			for(const auto& a:addition) {
				if((mouse_pos-a).mag()<10) {
					unique=false;
					break;
				}
			}
			//add if "unique"
			if(unique) addition.push_back(mouse_pos);
		}
		if(add_action.bReleased) {
			if(addition.size()>=3) {
				//check later for self intersection?
				//or we could do that in the sanitize step
				shapes.push_back(new Shape(addition));
			}
			addition.clear();
		}

		//shape removal
		if(GetKey(olc::Key::X).bHeld) {
			resetMarkers();

			//remove shape if mouse inside polygon
			for(auto it=shapes.begin(); it!=shapes.end();) {
				auto& s=*it;
				if(s->contains(mouse_pos)) {
					delete s;
					it=shapes.erase(it);
				} else it++;
			}
		}

		//ui toggles
		if(GetKey(olc::Key::V).bPressed) show_vertexes^=true;
		if(GetKey(olc::Key::S).bPressed) show_springs^=true;

		//physics
		mouse_point.pos=mouse_pos;
		mouse_point.old_pos=mouse_pos;

		//update mouse_constraint
		for(auto& m:mouse_constraint) {
			m.update();
		}

		//update shapes
		for(auto& s:shapes) {
			s->update(dt);
		}
	}

	void DrawThickLineDecal(const olc::vf2d& a, const olc::vf2d& b, float w, olc::Pixel col) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=sub.perp()/len;

		float angle=std::atan2f(sub.y, sub.x);
		DrawRotatedDecal(a-w/2*tang, prim_rect_dec, angle, {0, 0}, {len, w}, col);
	}

	void FillCircleDecal(const olc::vf2d& pos, float rad, olc::Pixel col) {
		olc::vf2d offset(rad, rad);
		olc::vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	void render() {
		Clear(olc::GREY);

		//show addition
		for(const auto& a:addition) {
			FillRectDecal(a, {2, 2});
		}

		//show all shapes
		for(const auto& shp:shapes) {
			//fill
			//initialize indexes
			std::list<size_t> indexes;
			for(size_t i=0; i<shp->getNum(); i++) indexes.push_back(i);

			//ear clipping triangulation
			for(auto curr=indexes.begin(); curr!=indexes.end();) {
				//get previous and next points
				auto prev=std::prev(curr==indexes.begin()?indexes.end():curr);
				auto next=std::next(curr); if(next==indexes.end()) next=indexes.begin();
				const auto& pt_p=shp->points[*prev].pos;
				const auto& pt_c=shp->points[*curr].pos;
				const auto& pt_n=shp->points[*next].pos;

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
					const auto& pt_o=shp->points[*other].pos;
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
					FillTriangleDecal(pt_p, pt_c, pt_n);

					//remove this index and start over
					indexes.erase(curr);
					curr=indexes.begin();
				} else curr++;
			}

			if(show_springs) {
				for(const auto& s:shp->springs) {
					DrawLineDecal(s.a->pos, s.b->pos, olc::GREEN);
				}
			}

			//constraints
			for(const auto& c:shp->constraints) {
				DrawThickLineDecal(c.a->pos, c.b->pos, 4, olc::BLACK);
				FillCircleDecal(c.a->pos, 2, olc::BLACK);
			}

			if(show_vertexes) {
				for(size_t i=0; i<shp->getNum(); i++) {
					auto str=std::to_string(i);
					vf2d offset(4*str.length(), 4);
					DrawStringDecal(shp->points[i].pos-offset, str, olc::BLACK);
				}
			}
		}

		//show mouse_constraint
		for(const auto& m:mouse_constraint) {
			DrawLineDecal(m.a->pos, m.b->pos, olc::CYAN);
		}

		//show mass
		for(const auto& s:shapes) {
			vf2d pos=s->getCOM();
			float mass=s->getMass();
			DrawStringDecal(pos, std::to_string(int(s->getMass())), olc::BLACK);
		}
	}

	bool OnUserUpdate(float dt) override {
		update(dt);

		render();

		return true;
	}
};

int main() {
	Example demo;
	bool vsync=true;
	if(demo.Construct(400, 400, 1, 1, false, vsync)) demo.Start();

	return 0;
}