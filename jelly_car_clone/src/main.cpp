#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_TRANSFORMEDVIEW
#include "common/olcPGEX_TransformedView.h"

#include "phys/shape.h"

#include <sstream>
#include <fstream>

struct JellyCarGame : olc::PixelGameEngine {
	JellyCarGame() {
		sAppName="Jelly Car";
	}

	//rendering "primitives"
	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;
	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;

	//user input stuff
	olc::TransformedView tv;

	vf2d scr_mouse_pos, wld_mouse_pos;

	vf2d rect_start;

	//simulation stuff
	std::list<Shape*> shapes;
	cmn::AABB phys_bounds;
	vf2d gravity;

	std::list<Constraint> mouse_constraint;
	PointMass mouse_point;

	//graphics stuff
	bool show_grid=true;
	bool show_springs=false;
	bool show_mass=false;
	bool show_anchors=false;

	bool OnUserCreate() override {
		srand(time(0));

		//setup some primitives to draw with
		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);
		{
			const unsigned int sz=1024;
			prim_circ_spr=new olc::Sprite(sz, sz);
			SetDrawTarget(prim_circ_spr);
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2);
			SetDrawTarget(nullptr);
			prim_circ_dec=new olc::Decal(prim_circ_spr);
		}

		//initialize transformed view
		tv.Initialise(GetScreenSize(), {1, 1});
		tv.SetScaleExtents({5, 5}, {1000, 1000});
		tv.EnableScaleClamp(true);

		//initialize physics stuff
		mouse_point.locked=true;
		gravity={0, 9.8f};

		//zoom to phys_bounds
		phys_bounds={{-14, -10}, {14, 10}};
		zoomToFit();

		//std::cout goes into console
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
		cmn::AABB bounds;
		int num=0;
		for(const auto& s:shapes) {
			for(int i=0; i<s->getNum(); i++) {
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

	bool OnUserDestroy() override {
		reset();

		delete prim_rect_dec;
		delete prim_rect_spr;
		delete prim_circ_dec;
		delete prim_circ_spr;

		return true;
	}

#pragma region COMMAND HELPERS
	bool exportCommand(std::string& filename) {
		if(filename.empty()) {
			std::cout<<"no filename. try using:\n  export <filename>\n";
			return false;
		}

		std::ofstream file_out(filename);
		if(file_out.fail()) {
			std::cout<<"invalid filename.\n";
			return false;
		}

		//print shapes line by line
		for(const auto& shp:shapes) {
			//shape designation
			//s <#pts> <#csts> <#sprs>
			file_out<<"s "<<shp->getNum()<<' '
				<<shp->constraints.size()<<' '
				<<shp->springs.size()<<'\n';
			//print points
			//<x> <y> <mass> <?locked>
			for(int i=0; i<shp->getNum(); i++) {
				const auto& p=shp->points[i];
				file_out<<p.pos.x<<' '<<p.pos.y<<' '<<p.mass<<' '<<p.locked<<'\n';
			}

			//print anchors
			//<x> <y>
			for(int i=0; i<shp->getNum(); i++) {
				const auto& a=shp->anchors[i];
				file_out<<a.x<<' '<<a.y<<'\n';
			}

			//print orientation
			//<x> <y> <rot> <?anchored>
			file_out<<shp->anchor_pos.x<<' '
				<<shp->anchor_pos.y<<' '
				<<shp->anchor_rot<<' '
				<<shp->anchored<<'\n';

			//make lookup table
			std::unordered_map<PointMass*, int> indexes;
			for(int i=0; i<shp->getNum(); i++) {
				indexes[&shp->points[i]]=i;
			}

			//print constraints
			//<a> <b> <len>
			for(const auto& c:shp->constraints) {
				file_out<<indexes[c.a]<<' '
					<<indexes[c.b]<<' '
					<<c.rest_len<<'\n';
			}

			//print springs
			//<a> <b> <len> <stiff> <damp>
			for(const auto& s:shp->springs) {
				file_out<<indexes[s.a]<<' '
					<<indexes[s.b]<<' '
					<<s.rest_len<<' '
					<<s.stiffness<<' '
					<<s.damping<<'\n';
			}
		}

		std::cout<<"  successfully exported to "<<filename<<'\n';

		file_out.close();
		return true;
	}
#pragma endregion

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") ConsoleClear();

		else if(cmd=="count") {
			std::cout<<"there are "<<shapes.size()<<" shapes\n";
		}

		else if(cmd=="reset") {
			std::cout<<"removed "<<shapes.size()<<" shapes\n";
			reset();
		}

		if(cmd=="export") {
			std::string filename;
			line_str>>filename;
			return exportCommand(filename);
		}

		return true;
	}

#pragma region UPDATE HELPERS
	void handleUserInput() {
		//adding ngons
		if(GetMouse(olc::Mouse::RIGHT).bPressed) {
			int num=4+rand()%9;
			float rad=cmn::random(1, 2.5f);
			shapes.emplace_back(new Shape(wld_mouse_pos, rad, num));
		}

		//adding rectangles
		const auto rect_action=GetKey(olc::Key::R);
		if(rect_action.bPressed) rect_start=wld_mouse_pos;
		if(rect_action.bReleased) {
			cmn::AABB box;
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

		//mouse constraint
		const auto grab_action=GetMouse(olc::Mouse::LEFT);
		if(grab_action.bPressed) {
			//reset
			mouse_constraint.clear();

			//for every shape under mouse
			mouse_point.pos=wld_mouse_pos;
			for(const auto& s:shapes) {
				if(!s->contains(wld_mouse_pos)) continue;

				//add all of its points to the list
				for(int i=0; i<s->getNum(); i++) {
					mouse_constraint.emplace_back(&mouse_point, &s->points[i]);
				}
			}
		}
		if(grab_action.bReleased) {
			//reset
			mouse_constraint.clear();
		}

		//anchoring
		if(GetKey(olc::Key::A).bPressed) {
			for(const auto& s:shapes) {
				//toggle anchor status
				if(s->contains(wld_mouse_pos)) {
					s->anchored^=true;
				}
			}
		}

		//reset view
		if(GetKey(olc::Key::Z).bHeld) zoomToFit();

		//zooming
		if(GetMouseWheel()>0) tv.ZoomAtScreenPos(1.07f, scr_mouse_pos);
		if(GetMouseWheel()<0) tv.ZoomAtScreenPos(1/1.07f, scr_mouse_pos);

		//panning
		const auto pan_action=GetMouse(olc::Mouse::MIDDLE);
		if(pan_action.bPressed) tv.StartPan(scr_mouse_pos);
		if(pan_action.bHeld) tv.UpdatePan(scr_mouse_pos);
		if(pan_action.bReleased) tv.EndPan(scr_mouse_pos);

		//visual toggles
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;
		if(GetKey(olc::Key::S).bPressed) show_springs^=true;
		if(GetKey(olc::Key::M).bPressed) show_mass^=true;
	}

	void handlePhysics(float dt) {
		//sanitize?
		for(auto it=shapes.begin(); it!=shapes.end();) {
			if((*it)->getArea()<0) it=shapes.erase(it);
			else it++;
		}

		//update mouse constraint
		mouse_point.pos=wld_mouse_pos;
		for(auto& m:mouse_constraint) {
			m.update();
		}

		//shape collision
		for(const auto& a:shapes) {
			for(const auto& b:shapes) {
				if(b==a) continue;

				a->collide(*b);
			}
		}

		//gravity, update, check bounds
		for(const auto& shp:shapes) {
			shp->accelerate(gravity);

			shp->update(dt);

			shp->keepIn(phys_bounds);
		}
	}
#pragma endregion

	void update(float dt) {
		//make sure simulation doesnt blow up?
		dt=cmn::clamp(dt, 1/165.f, 1/60.f);

		scr_mouse_pos=GetMousePos();
		wld_mouse_pos=tv.ScreenToWorld(scr_mouse_pos);

		//only allow input when console closed
		if(!IsConsoleShowing()) handleUserInput();

		//open integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		//only update physics when console closed
		if(!IsConsoleShowing()) handlePhysics(dt);
	}

#pragma region RENDER HELPERS
	void tvDrawThickLine(const vf2d& a, const vf2d& b, float rad, const olc::Pixel& col=olc::WHITE) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=sub.perp()/len;

		float angle=atan2f(sub.y, sub.x);
		tv.DrawRotatedDecal(a-rad*tang, prim_rect_dec, angle, {0, 0}, {len, 2*rad}, col);
	}

	void tvFillCircle(const vf2d& pos, float rad, const olc::Pixel& col=olc::WHITE) {
		vf2d offset(rad, rad);
		tv.DrawDecal(pos-offset, prim_circ_dec, {2*rad/prim_circ_dec->sprite->width, 2*rad/prim_circ_dec->sprite->height}, col);
	}

	void tvDrawString(const vf2d& pos, const std::string& str, const olc::Pixel& col=olc::WHITE, float scale=1) {
		vf2d offset(4*str.length(), 4);
		tv.DrawStringDecal(pos-scale*offset, str, col, {scale, scale});
	}

	void tvDrawAABB(const cmn::AABB& aabb, float rad, const olc::Pixel& col=olc::WHITE) {
		vf2d tr(aabb.max.x, aabb.min.y), bl(aabb.min.x, aabb.max.y);
		tvDrawThickLine(aabb.min, tr, rad, col), tvFillCircle(aabb.min, rad, col);
		tvDrawThickLine(tr, aabb.max, rad, col), tvFillCircle(tr, rad, col);
		tvDrawThickLine(aabb.max, bl, rad, col), tvFillCircle(aabb.max, rad, col);
		tvDrawThickLine(bl, aabb.min, rad, col), tvFillCircle(bl, rad, col);
	}
#pragma endregion

	void render() {
		Clear(olc::Pixel(0, 100, 255));

		if(show_grid) {
			const auto grid_spacing=1;
			const olc::Pixel grid_col(0, 255, 255);

			//screen bounds in world space, snap to nearest
			vf2d tl=tv.GetWorldTL(), br=tv.GetWorldBR();
			int i_s=std::floor(tl.x/grid_spacing), j_s=std::floor(tl.y/grid_spacing);
			int i_e=std::ceil(br.x/grid_spacing), j_e=std::ceil(br.y/grid_spacing);

			//vert
			for(int i=i_s; i<=i_e; i++) {
				float x=grid_spacing*i;
				vf2d top{x, tl.y}, btm{x, br.y};
				if(i%10==0) tvDrawThickLine(top, btm, .1f, grid_col);
				else tv.DrawLineDecal(top, btm, grid_col);
			}

			//horiz
			for(int j=j_s; j<=j_e; j++) {
				float y=grid_spacing*j;
				vf2d lft{tl.x, y}, rgt{br.x, y};
				if(j%10==0) tvDrawThickLine(lft, rgt, .1f, grid_col);
				else tv.DrawLineDecal(lft, rgt, grid_col);
			}
		}

		//show phys bounds
		tvDrawAABB(phys_bounds, .1f, olc::RED);

		//draw shapes
		for(const auto& shp:shapes) {
			//show anchors
			if(shp->anchored) {
				//rotation matrix
				vf2d cosin=cmn::polar(1, shp->anchor_rot);
				auto rotate=[cosin] (const vf2d& p) {
					return vf2d(
						p.x*cosin.x-p.y*cosin.y,
						p.x*cosin.y+p.y*cosin.x
					);
				};

				//outlines
				for(int i=0; i<shp->getNum(); i++) {
					vf2d a=shp->anchor_pos+rotate(shp->anchors[i]);
					vf2d b=shp->anchor_pos+rotate(shp->anchors[(i+1)%shp->getNum()]);
					tvDrawThickLine(a, b, .1f, olc::GREY);
					tvFillCircle(a, .1f, olc::GREY);
				}
			}

			//fill
			{
				//initialize indexes
				std::list<int> indexes;
				for(int i=0; i<shp->getNum(); i++) indexes.emplace_back(i);

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
						tv.DrawWarpedDecal(prim_rect_dec, {pt_p, pt_c, pt_n, pt_n});

						//remove this index and start over
						indexes.erase(curr);
						curr=indexes.begin();
					} else curr++;
				}
			}

			//constraint outlines
			for(const auto& c:shp->constraints) {
				tvDrawThickLine(c.a->pos, c.b->pos, .1f, olc::BLACK);
				tvFillCircle(c.a->pos, .1f, olc::BLACK);
			}

			if(show_springs) {
				for(const auto& s:shp->springs) {
					tv.DrawLineDecal(s.a->pos, s.b->pos, olc::GREEN);
				}
			}

			if(show_mass) {
				float mass=0;
				vf2d ctr=shp->getCOM();
				float r_sum=0;
				for(int i=0; i<shp->getNum(); i++) {
					r_sum+=(shp->points[i].pos-ctr).mag();
					mass+=shp->points[i].mass;
				}
				float rad=r_sum/shp->getNum(), scale=rad/8;
				std::string mass_str=std::to_string(int(mass));
				tvDrawString(ctr, mass_str, olc::DARK_GREY, scale/mass_str.length());
			}
		}
	}

	bool OnUserUpdate(float dt) override {
		update(dt);

		render();

		return true;
	}
};


int main() {
	JellyCarGame jcg;
	bool vsync=true;
	if(jcg.Construct(640, 480, 1, 1, false, vsync)) jcg.Start();

	return 0;
}