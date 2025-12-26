/*todo
	saving/loading
	store convex collision meshes on init
		use ear clipping triangulation?
		how to break concave -> list<convex>?
*/

#define OLC_PGE_APPLICATION
#include "olc/include/olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_TRANSFORMEDVIEW
#include "olc/include/olcPGEX_TransformedView.h"

#include "shape.h"

struct RigidBodyUI : olc::PixelGameEngine {
	RigidBodyUI() {
		sAppName="Rigid Body Simulation";
	}

	//physics stuff
	std::list<Shape*> shapes;
	cmn::AABB phys_bounds;

	//user input stuff
	vf2d scr_mouse_pos;
	vf2d wld_mouse_pos;
	vf2d prev_wld_mouse_pos;

	std::vector<vf2d> addition;
	float addition_timer=0;

	Shape* spring_shape=nullptr;
	vf2d spring_offset;

	//graphics toggles
	bool show_wireframes=false;
	bool show_bounds=false;
	bool show_masses=false;
	bool show_vertexes=false;
	bool show_norms=false;

	olc::TransformedView tv;

	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;
	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;

	//place shapes s.t. their bounds lie in box
	void placeShapesWithin(const cmn::AABB& box) {
		//start by avoiding locked shapes
		std::list<Shape*> to_check_against;
		for(const auto& s:shapes) {
			if(s->locked) to_check_against.push_back(s);
		}
		//place all non locked shapes
		for(auto& s:shapes) {
			if(s->locked) continue;

			//random rotation
			s->rot=cmn::randFloat(2*cmn::Pi);
			s->updateRot();
			//reset rot vel
			s->old_rot=s->rot;

			//get shape bounds
			cmn::AABB s_box=s->getAABB();

			//try a finite number of times...
			for(int i=0; i<100; i++) {
				//which way can we move it so its in box
				vf2d delta_min=box.min-s_box.min;
				vf2d delta_max=box.max-s_box.max;
				vf2d delta(
					cmn::randFloat(delta_min.x, delta_max.x),
					cmn::randFloat(delta_min.y, delta_max.y)
				);
				s->pos+=delta;
				s_box.min+=delta, s_box.max+=delta;

				//don't place ontop of other shapes
				bool overlaps=false;
				for(const auto& o:to_check_against) {
					if(s_box.overlaps(o->getAABB())) {
						overlaps=true;
						break;
					}
				}
				if(!overlaps) break;
			}

			//reset vel
			s->old_pos=s->pos;

			//we also now try avoid this shape
			to_check_against.push_back(s);
		}
	}

	void zoomToFit(const cmn::AABB& box) {
		//zoom into view
		const auto margin=15;
		vf2d box_sz=box.max-box.min;
		float num_x=ScreenWidth()/box_sz.x;
		float num_y=ScreenHeight()/box_sz.y;
		float scale;
		//zoom to height
		if(num_x>num_y) scale=(ScreenHeight()-2*margin)/box_sz.y;
		//zoom to width
		else scale=(ScreenWidth()-2*margin)/box_sz.x;
		tv.SetWorldScale({scale, scale});

		//pan into view
		vf2d mid_scr=GetScreenSize()/2;
		vf2d mid_wld=tv.ScreenToWorld(mid_scr);
		tv.MoveWorldOffset(box.getCenter()-mid_wld);
	}

	bool OnUserCreate() override {
		std::srand(std::time(0));

		//init bounds
		phys_bounds={{-4, -3}, {4, 3}};

		//add walls
		{
			const float margin=.1f;
			Shape* top=new Shape(cmn::AABB{
				vf2d(phys_bounds.min.x-margin, phys_bounds.min.y-margin),
				vf2d(phys_bounds.max.x+margin, phys_bounds.min.y+margin)
				}); top->locked=true;
			Shape* right=new Shape(cmn::AABB{
				vf2d(phys_bounds.max.x-margin, phys_bounds.min.y-margin),
				vf2d(phys_bounds.max.x+margin, phys_bounds.max.y+margin)
				}); right->locked=true;
			Shape* bottom=new Shape(cmn::AABB{
				vf2d(phys_bounds.min.x-margin, phys_bounds.max.y-margin),
				vf2d(phys_bounds.max.x+margin, phys_bounds.max.y+margin)
				}); bottom->locked=true;
			Shape* left=new Shape(cmn::AABB{
				vf2d(phys_bounds.min.x-margin, phys_bounds.min.y-margin),
				vf2d(phys_bounds.min.x+margin, phys_bounds.max.y+margin)
				}); left->locked=true;
			shapes.push_back(top);
			shapes.push_back(right);
			shapes.push_back(bottom);
			shapes.push_back(left);
		}

		//random assortment of shapes
		for(int i=0; i<12; i++) {
			float rad=cmn::randFloat(.2f, .5f);
			int num=3+std::rand()%7;
			shapes.push_back(new Shape({0, 0}, rad, num));
		}

		placeShapesWithin(phys_bounds);

		//color shapes randomly
		for(auto& s:shapes) {
			s->col=olc::Pixel(
				std::rand()%256,
				std::rand()%256,
				std::rand()%256
			);
		}

		//setup transforms
		tv.Initialise(GetScreenSize());

		zoomToFit(phys_bounds);

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

		return true;
	}

	void removeMarkers() {
		spring_shape=nullptr;
	}

	void reset() {
		removeMarkers();

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

#pragma region UPDATE HELPERS
	void handleUserInput(float dt) {
		//zooming
		if(GetMouseWheel()>0) tv.ZoomAtScreenPos(1.07f, scr_mouse_pos);
		if(GetMouseWheel()<0) tv.ZoomAtScreenPos(1/1.07f, scr_mouse_pos);

		//panning
		const auto middle_mouse=GetMouse(olc::Mouse::MIDDLE);
		if(middle_mouse.bPressed) tv.StartPan(scr_mouse_pos);
		if(middle_mouse.bHeld) tv.UpdatePan(scr_mouse_pos);
		if(middle_mouse.bReleased) tv.EndPan(scr_mouse_pos);
		if(GetKey(olc::Key::HOME).bPressed) zoomToFit(phys_bounds);

		//get shape under mouse
		Shape* mouse_shape=nullptr;
		for(const auto& s:shapes) {
			if(s->contains(wld_mouse_pos)) {
				mouse_shape=s;
				break;
			}
		}

		//spring interactivity
		const auto l_mouse=GetMouse(olc::Mouse::LEFT);
		if(l_mouse.bPressed) {
			spring_shape=mouse_shape;
			if(mouse_shape) {
				spring_offset=mouse_shape->worldToLocal(wld_mouse_pos);
			}
		}
		if(l_mouse.bHeld) {
			if(spring_shape) {
				//F=kx
				vf2d spr_pos=spring_shape->localToWorld(spring_offset);
				vf2d sub=wld_mouse_pos-spr_pos;
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
					if((a-wld_mouse_pos).mag()<.05f) {
						exists=true;
						break;
					}
				}
				//add mouse point
				if(!exists) addition.push_back(wld_mouse_pos);
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
			removeMarkers();

			for(auto it=shapes.begin(); it!=shapes.end();) {
				const auto& s=*it;
				if(s->contains(wld_mouse_pos)) {
					delete s;
					it=shapes.erase(it);
				} else it++;
			}
		}

		//graphics toggles
		if(GetKey(olc::Key::W).bPressed) show_wireframes^=true;
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
		if(GetKey(olc::Key::M).bPressed) show_masses^=true;
		if(GetKey(olc::Key::V).bPressed) show_vertexes^=true;
		if(GetKey(olc::Key::N).bPressed) show_norms^=true;
		if(GetKey(olc::Key::R).bPressed) placeShapesWithin(phys_bounds);
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
			s->update(dt);
		}
	}
#pragma endregion

	void update(float dt) {
		prev_wld_mouse_pos=wld_mouse_pos;
		scr_mouse_pos=GetMousePos();
		wld_mouse_pos=tv.ScreenToWorld(scr_mouse_pos);

		handleUserInput(dt);

		handlePhysics(dt);
	}

#pragma region RENDER HELPERS
	void tvDrawThickLineDecal(const vf2d& a, const vf2d& b, float w, const olc::Pixel& col) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=(sub/len).perp();

		float angle=std::atan2(sub.y, sub.x);
		tv.DrawRotatedDecal(a-w/2*tang, prim_rect_dec, angle, {0, 0}, {len, w}, col);
	}

	void tvFillCircleDecal(const vf2d& pos, float rad, const olc::Pixel& col) {
		vf2d offset{rad, rad};
		vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		tv.DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	void tvDrawArrowDecal(const vf2d& a, const vf2d& b, float t, const olc::Pixel& col) {
		//main arm
		vf2d sub=b-a;
		vf2d c=b-t*sub;
		tv.DrawLineDecal(a, c, col);
		//arrow triangle
		vf2d d=.5f*t*sub.perp();
		vf2d l=c-d, r=c+d;
		tv.DrawLineDecal(l, r, col);
		tv.DrawLineDecal(l, b, col);
		tv.DrawLineDecal(r, b, col);
	}

	void renderShapeBounds(const Shape& shp) {
		//color based on overlapping
		auto box=shp.getAABB();
		bool overlaps=false;
		for(const auto& o:shapes) {
			if(o==&shp) continue;

			if(box.overlaps(o->getAABB())) {
				overlaps=true;
				break;
			}
		}

		vf2d tr(box.max.x, box.min.y), bl(box.min.x, box.max.y);
		olc::Pixel col=overlaps?olc::RED:olc::GREEN;
		tv.DrawLineDecal(box.min, tr, col);
		tv.DrawLineDecal(tr, box.max, col);
		tv.DrawLineDecal(box.max, bl, col);
		tv.DrawLineDecal(bl, box.min, col);
	}

	void renderShapeFill(const Shape& shp) {
		//initialize indexes
		std::list<int> indexes;
		for(int i=0; i<shp.getNum(); i++) indexes.push_back(i);

		//ear clipping triangulation
		for(auto curr=indexes.begin(); curr!=indexes.end();) {
			//get previous and next points
			auto prev=std::prev(curr==indexes.begin()?indexes.end():curr);
			auto next=std::next(curr); if(next==indexes.end()) next=indexes.begin();
			const auto& pt_p=shp.points[*prev];
			const auto& pt_c=shp.points[*curr];
			const auto& pt_n=shp.points[*next];

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
				const auto& pt_o=shp.points[*other];
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
					tv.WorldToScreen(shp.localToWorld(pt_p)),
					tv.WorldToScreen(shp.localToWorld(pt_c)),
					tv.WorldToScreen(shp.localToWorld(pt_n)),
					shp.col
				);

				//remove this index and start over
				indexes.erase(curr);
				curr=indexes.begin();
			} else curr++;
		}
	}

	void renderShapeOutline(const Shape& s, float w, const olc::Pixel& col) {
		for(int i=0; i<s.getNum(); i++) {
			const auto& a=s.localToWorld(s.points[i]);
			const auto& b=s.localToWorld(s.points[(i+1)%s.getNum()]);
			tvFillCircleDecal(a, w/2, col);
			tvDrawThickLineDecal(a, b, w, col);
		}
	}

	void renderShapeVerts(const Shape& s, float scl, const olc::Pixel& col) {
		for(int i=0; i<s.getNum(); i++) {
			const auto& a=s.points[i];
			auto str=std::to_string(i);
			vf2d offset(4*str.size(), 4);
			tv.DrawStringDecal(
				s.localToWorld(a)-scl*offset,
				str,
				olc::BLUE,
				{scl, scl}
			);
		}
	}

	void renderShapeNorms(const Shape& s, float h, const olc::Pixel& col) {
		for(int i=0; i<s.getNum(); i++) {
			vf2d a=s.localToWorld(s.points[i]);
			vf2d b=s.localToWorld(s.points[(1+i)%s.getNum()]);
			vf2d mid=(a+b)/2;
			vf2d tang=(b-a).norm();
			vf2d norm(tang.y, -tang.x);
			tvDrawArrowDecal(mid, mid+h*norm, .2f, col);
		}
	}
#pragma endregion

	void render() {
		Clear(olc::DARK_GREY);

		SetDecalMode(show_wireframes?olc::DecalMode::WIREFRAME:olc::DecalMode::NORMAL);

		//show addition
		if(addition.size()) {
			bool is_first=true;
			vf2d first, prev;
			for(const auto& a:addition) {
				if(is_first) first=a, is_first=false;
				else tv.DrawLineDecal(prev, a, olc::BLUE);
				prev=a;
			}
			tv.DrawLineDecal(first, prev, olc::BLUE);
		}

		//show shapes
		for(const auto& shp:shapes) {
			if(show_bounds) renderShapeBounds(*shp);

			renderShapeFill(*shp);

			renderShapeOutline(*shp, .025f, shp->locked?olc::WHITE:olc::BLACK);

			if(show_vertexes) renderShapeVerts(*shp, .005f, olc::RED);

			if(show_norms) renderShapeNorms(*shp, .13f, shp->col);

			if(show_masses) {
				tv.DrawLineDecal(vf2d(shp->pos.x-.1f, shp->pos.y), vf2d(shp->pos.x+.1f, shp->pos.y), olc::BLACK);
				tv.DrawLineDecal(vf2d(shp->pos.x, shp->pos.y-.1f), vf2d(shp->pos.x, shp->pos.y+.1f), olc::BLACK);
			}
		}

		//show spring shape
		if(spring_shape) {
			vf2d spr_pos=spring_shape->localToWorld(spring_offset);
			tv.DrawLineDecal(spr_pos, wld_mouse_pos, olc::CYAN);
		}

		//show phys_bounds
		if(show_bounds) {
			tv.DrawRectDecal(phys_bounds.min, phys_bounds.max-phys_bounds.min, olc::RED);
		}

		return;
	}

	bool OnUserUpdate(float dt) override {
		update(dt);

		render();

		return true;
	}
};

int main() {
	RigidBodyUI rbui;
	if(rbui.Construct(600, 480, 1, 1, false, true)) rbui.Start();

	return 0;
}