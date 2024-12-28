#include "common/myPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_TRANSFORMEDVIEW
#include "common/olcPGEX_TransformedView.h"

#include "geom/aabb.h"
#include "phys/point_mass.h"
#include "phys/constraint.h"
#include "phys/spring.h"

#include "phys/shape.h"

struct JellyCarGame : MyPixelGameEngine {
	JellyCarGame() {
		sAppName="Jelly Car";
	}

	olc::TransformedView tv;

	vf2d scr_mouse_pos, wld_mouse_pos;

	bool show_grid=true;

	std::list<Shape*> shapes;

	vf2d gravity;
	AABB phys_bounds;

	vf2d rect_start;
	std::list<Constraint> mouse_constraint;
	PointMass mouse_point;

	bool on_init() override {
		srand(time(0));

		tv.Initialise(GetScreenSize(), {1, 1});
		tv.SetScaleExtents({5, 5}, {1000, 1000});
		tv.EnableScaleClamp(true);

		mouse_point.locked=true;

		gravity={0, 9.8f};

		phys_bounds={{-14, -10}, {14, 10}};

		zoomToFit();

		ConsoleCaptureStdOut(true);

		return true;
	}

	void reset() {
		mouse_constraint.clear();
		for(const auto& s:shapes) delete s;
		shapes.clear();
	}

	void zoomToFit() {
		//find scene bounds
		AABB bounds;
		int num=0;
		for(const auto& s:shapes) {
			for(int i=0; i<s->num_pts; i++) {
				bounds.fitToEnclose(s->points[i].pos);
				num++;
			}
		}
		//default behavior
		if(num<2) bounds=phys_bounds;

		//zoom into view
		const auto margin=30;
		vf2d bounds_size=bounds.max-bounds.min;
		float num_x=ScreenWidth()/bounds_size.x;
		float num_y=ScreenHeight()/bounds_size.y;
		float scale;
		if(num_x>num_y) {//zoom to height
			scale=(ScreenHeight()-2*margin)/bounds_size.y;
		} else {//zoom to width
			scale=(ScreenWidth()-2*margin)/bounds_size.x;
		}
		tv.SetWorldScale({scale, scale});

		//pan into view
		vf2d mid_scr=GetScreenSize()/2;
		vf2d mid_wld=tv.ScreenToWorld(mid_scr);
		tv.MoveWorldOffset(bounds.getCenter()-mid_wld);
	}

	bool on_exit() override {
		return true;
	}

	bool on_update(float dt) override {
		{//make sure simulation doesnt blow up?
			const float max_dt=1/60.f;
			if(dt>max_dt) dt=max_dt;
		}

#pragma region USER INPUT
		scr_mouse_pos=GetMousePos();
		wld_mouse_pos=tv.ScreenToWorld(scr_mouse_pos);

		if(!IsConsoleShowing()) {
		//reset view
			if(GetKey(olc::Key::HOME).bPressed) zoomToFit();

			//zooming
			if(GetMouseWheel()>0) tv.ZoomAtScreenPos(1.07f, scr_mouse_pos);
			if(GetMouseWheel()<0) tv.ZoomAtScreenPos(1/1.07f, scr_mouse_pos);

			//panning
			if(GetMouse(olc::Mouse::LEFT).bPressed) {
				mouse_constraint.clear();

				mouse_point.pos=wld_mouse_pos;
				for(const auto& s:shapes) {
					if(!s->contains(wld_mouse_pos)) continue;

					for(int i=0; i<s->num_pts; i++) {
						mouse_constraint.emplace_back(&mouse_point, &s->points[i]);
					}
				}
				if(!mouse_constraint.size()) tv.StartPan(scr_mouse_pos);
			}
			if(GetMouse(olc::Mouse::LEFT).bHeld) {
				if(mouse_constraint.size()) {
					mouse_point.pos=wld_mouse_pos;

					for(auto& m:mouse_constraint) {
						m.update();
					}
				} else tv.UpdatePan(scr_mouse_pos);
			}
			if(GetMouse(olc::Mouse::LEFT).bReleased) {
				tv.EndPan(scr_mouse_pos);
				mouse_constraint.clear();
			}

			//visual toggles
			if(GetKey(olc::Key::G).bPressed) show_grid^=true;

			//adding stuff
			if(GetKey(olc::Key::A).bPressed) {
				int num=3+rand()%8;
				float rad=random(.5f, 1.5f);
				shapes.emplace_back(new Shape(wld_mouse_pos, rad, num));
			}
			if(GetKey(olc::Key::R).bPressed) rect_start=wld_mouse_pos;
			if(GetKey(olc::Key::R).bReleased) {
				AABB box;
				box.fitToEnclose(rect_start);
				box.fitToEnclose(wld_mouse_pos);
				shapes.emplace_back(new Shape(box));
			}

			//removing stuff
			if(GetKey(olc::Key::X).bHeld) {
				mouse_constraint.clear();
				for(auto sit=shapes.begin(); sit!=shapes.end();) {
					const auto& s=*sit;
					if(s->contains(wld_mouse_pos)) {
						delete s;
						sit=shapes.erase(sit);
					} else sit++;
				}
			}
		}

		//open console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);
#pragma endregion

#pragma region PHYSICS
		//sanitize?
		for(auto it=shapes.begin(); it!=shapes.end();) {
			if((*it)->getArea()<0) it=shapes.erase(it);
			else it++;
		}

		for(const auto& a:shapes) {
			for(const auto& b:shapes) {
				//dont check self
				if(b==a) continue;

				a->collide_ish(*b);
			}
		}

		for(const auto& shp:shapes) {
			for(auto& c:shp->constraints) {
				c.update();
			}

			for(auto& s:shp->springs) {
				s.update(dt);
			}

			for(int i=0; i<shp->num_pts; i++) {
				auto& p=shp->points[i];
				p.applyForce(p.mass*gravity);
				p.update(dt);
				p.keepIn(phys_bounds);
			}
		}
#pragma endregion

		return true;
	}

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="count") {
			std::cout<<"there are "<<shapes.size()<<" shapes\n";
		}

		else if(cmd=="reset") {
			std::cout<<"removed "<<shapes.size()<<" shapes\n";
			reset();
		}

		else if(cmd=="clear") ConsoleClear();
	}

#pragma region RENDER HELPERS
	void tvDrawThickLine(const vf2d& a, const vf2d& b, float rad, const olc::Pixel& col=olc::WHITE) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=(sub/len).perp();

		float angle=atan2f(sub.y, sub.x);
		tv.DrawRotatedDecal(a-rad*tang, getRectDecal(), angle, {0, 0}, {len, 2*rad}, col);
	}

	void tvFillCircle(const vf2d& pos, float rad, const olc::Pixel& col=olc::WHITE) {
		vf2d offset(rad, rad);
		auto c=getCircleDecal();
		tv.DrawDecal(pos-offset, c, {2*rad/c->sprite->width, 2*rad/c->sprite->height}, col);
	}

	void tvDrawString(const vf2d& pos, const std::string& str, const olc::Pixel& col=olc::WHITE, float scale=1) {
		vf2d offset(4*str.length(), 4);
		tv.DrawStringDecal(pos-scale*offset, str, col, {scale, scale});
	}

	void tvDrawAABB(const AABB& aabb, float rad, const olc::Pixel& col=olc::WHITE) {
		vf2d tr(aabb.max.x, aabb.min.y), bl(aabb.min.x, aabb.max.y);
		tvDrawThickLine(aabb.min, tr, rad, col), tvFillCircle(aabb.min, rad, col);
		tvDrawThickLine(tr, aabb.max, rad, col), tvFillCircle(tr, rad, col);
		tvDrawThickLine(aabb.max, bl, rad, col), tvFillCircle(aabb.max, rad, col);
		tvDrawThickLine(bl, aabb.min, rad, col), tvFillCircle(bl, rad, col);
	}
#pragma endregion

	bool on_render() override {
		Clear(olc::Pixel(0, 100, 255));
		//debug?
		//SetDecalMode(olc::DecalMode::WIREFRAME);

		if(show_grid) {
			const auto grid_spacing=1;

			//screen bounds in world space, snap to nearest
			vf2d tl=tv.GetWorldTL(), br=tv.GetWorldBR();
			int i_s=std::floor(tl.x/grid_spacing), j_s=std::floor(tl.y/grid_spacing);
			int i_e=std::ceil(br.x/grid_spacing), j_e=std::ceil(br.y/grid_spacing);

			//vert
			for(int i=i_s; i<=i_e; i++) {
				float x=grid_spacing*i;
				vf2d top{x, tl.y}, btm{x, br.y};
				if(i%10==0) tvDrawThickLine(top, btm, .1f, olc::GREY);
				else tv.DrawLineDecal(top, btm, olc::GREY);
			}

			//horiz
			for(int j=j_s; j<=j_e; j++) {
				float y=grid_spacing*j;
				vf2d lft{tl.x, y}, rgt{br.x, y};
				if(j%10==0) tvDrawThickLine(lft, rgt, .1f, olc::GREY);
				else tv.DrawLineDecal(lft, rgt, olc::GREY);
			}
		}

		tvDrawAABB(phys_bounds, .1f, olc::RED);

		//draw shapes
		const bool show_constraints=GetKey(olc::Key::C).bHeld;
		const bool show_springs=GetKey(olc::Key::S).bHeld;
		const bool show_mass=GetKey(olc::Key::M).bHeld;
		for(const auto& shp:shapes) {
			//initialize indexes
			std::list<int> indexes;
			for(int i=0; i<shp->num_pts; i++) indexes.emplace_back(i);

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
					tv.DrawWarpedDecal(getRectDecal(), {pt_p, pt_c, pt_n, pt_n});

					//remove this index and start over
					indexes.erase(curr);
					curr=indexes.begin();
				} else curr++;
			}

			//shape outlines
			for(int i=0; i<shp->num_pts; i++) {
				vf2d a=shp->points[i].pos;
				vf2d b=shp->points[(i+1)%shp->num_pts].pos;

				tvDrawThickLine(a, b, .1f, olc::BLACK);
				tvFillCircle(a, .1f, olc::BLACK);
			}

			//constraints
			if(show_constraints) {
				for(const auto& c:shp->constraints) {
					tv.DrawLineDecal(c.a->pos, c.b->pos, olc::GREEN);
				}
			}

			//springs
			if(show_springs) {
				for(const auto& s:shp->springs) {
					tv.DrawLineDecal(s.a->pos, s.b->pos, olc::RED);
				}
			}

			//mass
			if(show_mass) {
				float mass=0;
				vf2d ctr=shp->getCOM();
				float r_sum=0;
				for(int i=0; i<shp->num_pts; i++) {
					r_sum+=(shp->points[i].pos-ctr).mag();
					mass+=shp->points[i].mass;
				}
				float rad=r_sum/shp->num_pts, scale=rad/8;
				std::string mass_str=std::to_string(mass);
				tvDrawString(ctr, mass_str, olc::DARK_GREY, scale/mass_str.length());
			}
		}

		return true;
	}
};

int main() {
	JellyCarGame jcg;
	if(jcg.Construct(640, 480, 1, 1, false, true)) jcg.Start();

	return 0;
}