#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_TRANSFORMEDVIEW
#include "common/olcPGEX_TransformedView.h"

#include "phys/shape.h"

#include "scene.h"

#include "common/stopwatch.h"

//https://www.rapidtables.com/convert/color/hsv-to-rgb.html
olc::Pixel hsv2rgb(int h, float s, float v) {
	float c=v*s;
	float x=c*(1-std::fabsf(1-std::fmodf(h/60.f, 2.f)));
	float m=v-c;
	float r=0, g=0, b=0;
	switch(h/60) {
		case 0: r=c, g=x, b=0; break;
		case 1: r=x, g=c, b=0; break;
		case 2: r=0, g=c, b=x; break;
		case 3: r=0, g=x, b=c; break;
		case 4: r=x, g=0, b=c; break;
		case 5: r=c, g=0, b=x; break;
	}
	return olc::PixelF(m+r, m+g, m+b);
}

olc::Pixel randomPastel() {
	//hue=pure color
	int h=rand()%360;
	//saturation=intensity
	float s=cmn::random(.1f, .4f);
	//value=brightness
	float v=cmn::random(.75f, 1);
	return hsv2rgb(h, s, v);
}

struct JellyCarGame : olc::PixelGameEngine {
	JellyCarGame() {
		sAppName="Jelly Car Clone";
	}

	//rendering "primitives"
	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;
	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;

	//user input stuff
	olc::TransformedView tv;

	vf2d scr_mouse_pos, wld_mouse_pos;

	vf2d* rect_start=nullptr;

	int drag_section=-1;

	std::list<Constraint> mouse_constraint;
	PointMass mouse_point;

	//simulation stuff
	Scene scene;

	//graphics stuff
	bool show_grid=true;
	bool show_springs=false;
	bool show_mass=false;
	bool show_wireframes=false;
	bool show_bounds=false;
	bool show_vertexes=false;
	bool to_time=false;

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

		//zoom to phys_bounds
		scene.phys_bounds={{-14, -10}, {14, 10}};
		zoomToFit();

		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";
		//std::cout diverted to console
		ConsoleCaptureStdOut(true);

		return true;
	}

	void reset() {
		mouse_constraint.clear();

		scene.clear();
	}

	void zoomToFit(bool fit_shapes=true) {
		//find scene bounds
		cmn::AABB bounds;
		if(fit_shapes&&scene.shapes.size()) {
			//wrap aabb around points
			for(const auto& s:scene.shapes) {
				cmn::AABB box=s->getAABB();
				bounds.fitToEnclose(box.min);
				bounds.fitToEnclose(box.max);
			}
		}
		//default behavior
		else bounds=scene.phys_bounds;

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

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") {
			ConsoleClear();

			return true;
		}

		if(cmd=="reset") {
			//english+logic=grammar :D
			int num=scene.shapes.size();
			std::cout<<"  removed "<<num<<" shape";
			if(num!=1) std::cout<<'s';
			std::cout<<'\n';
			reset();

			return true;
		}

		if(cmd=="count") {
			//english + logic = grammar :D
			int num=scene.shapes.size();
			std::cout<<"  there ";
			std::cout<<(num==1?"is":"are");
			std::cout<<' '<<num<<" shape";
			if(num!=1) std::cout<<'s';
			std::cout<<'\n';

			return true;
		}

		if(cmd=="time") {
			to_time=true;

			return true;
		}

		if(cmd=="export") {
			std::string filename;
			line_str>>filename;
			try {
				scene.save(filename);

				std::cout<<"  successfully exported to: "<<filename<<'\n';

				return true;
			} catch(const std::exception& e) {
				std::cout<<"  "<<e.what()<<'\n';
				return false;
			}
		}

		if(cmd=="import") {
			std::string filename;
			line_str>>filename;
			try {
				scene=Scene::load(filename);

				zoomToFit(false);

				std::cout<<"  successfully imported from: "<<filename<<'\n';

				return true;
			} catch(const std::exception& e) {
				std::cout<<"  "<<e.what()<<'\n';
				return false;
			}
		}

		if(cmd=="keybinds") {
			std::cout<<
				"  R     drag to add rect\n"
				"  A     anchor shape\n"
				"  X     remove shape\n"
				"  G     toggle grid\n"
				"  S     toggle spring view\n"
				"  W     toggle wireframe view\n"
				"  B     toggle bounds view\n"
				"  M     toggle mass view\n"
				"  V     toggle vertex view\n"
				"  Z     zoom to fit\n"
				"  ESC   toggle integrated console\n";

			return true;
		}

		if(cmd=="mousebinds") {
			std::cout<<
				"  LEFT     drag to move shapes or edit bounds\n"
				"  RIGHT    add random ngon\n"
				"  MIDDLE   scroll to zoom, drag to pan\n";

			return true;
		}

		if(cmd=="help") {
			std::cout<<
				"  clear        clears the console\n"
				"  reset        removes all shapes\n"
				"  count        how many shapes are there?\n"
				"  time         times update and render cycle\n"
				"  export       exports scene to specified file\n"
				"  import       imports scene from specified file\n"
				"  keybinds     which keys to press for this program?\n"
				"  mousebinds   which buttons to press for this program?\n";

			return true;
		}

		std::cout<<"  unknown command. type help for list of commands.\n";

		return false;
	}

#pragma region UPDATE HELPERS
	void handleUserInput() {
		//adding ngons
		if(GetMouse(olc::Mouse::RIGHT).bPressed) {
			int num=3+rand()%9;
			float rad=cmn::random(1, 3);
			Shape* s=new Shape(wld_mouse_pos, rad, num);
			s->col=randomPastel();
			scene.shapes.push_back(s);
		}

		//adding rectangles
		const auto rect_action=GetKey(olc::Key::R);
		if(rect_action.bPressed) {
			rect_start=new vf2d(wld_mouse_pos);
		}
		if(rect_action.bReleased) {
			//get bounds
			cmn::AABB box;
			box.fitToEnclose(*rect_start);
			box.fitToEnclose(wld_mouse_pos);

			//determine sizing
			float size=cmn::random(1, 2.5f);
			const int w=1+(box.max.x-box.min.x)/size;
			const int h=1+(box.max.y-box.min.y)/size;
			auto ix=[&w] (int i, int j) {
				return i+w*j;
			};
			Shape* shp=new Shape(w*h);

			//fill point grid
			for(int i=0; i<w; i++) {
				for(int j=0; j<h; j++) {
					vf2d pos=box.min+size*vf2d(i, j);
					shp->points[ix(i, j)]=PointMass(pos);
				}
			}

			//attach constraints
			for(int i=1; i<w; i++) {
				shp->constraints.emplace_back(&shp->points[ix(i-1, 0)], &shp->points[ix(i, 0)]);
			}
			for(int j=1; j<h; j++) {
				shp->constraints.emplace_back(&shp->points[ix(w-1, j-1)], &shp->points[ix(w-1, j)]);
			}
			for(int i=w-1; i>=1; i--) {
				shp->constraints.emplace_back(&shp->points[ix(i, h-1)], &shp->points[ix(i-1, h-1)]);
			}
			for(int j=h-1; j>=1; j--) {
				shp->constraints.emplace_back(&shp->points[ix(0, j)], &shp->points[ix(0, j-1)]);
			}

			shp->initMass();

			//attach springs
			for(int i=1; i<w; i++) {
				for(int j=1; j<h; j++) {
					//rows
					if(j<h-1) shp->springs.emplace_back(&shp->points[ix(i-1, j)], &shp->points[ix(i, j)]);
					//columns
					if(i<w-1) shp->springs.emplace_back(&shp->points[ix(i, j-1)], &shp->points[ix(i, j)]);
					//boxes
					shp->springs.emplace_back(&shp->points[ix(i-1, j-1)], &shp->points[ix(i, j)]);
					shp->springs.emplace_back(&shp->points[ix(i-1, j)], &shp->points[ix(i, j-1)]);
				}
			}

			//proportional to perim/area
			float coeff=w*h/float(w+h);
			for(auto& s:shp->springs) {
				s.stiffness*=coeff;
				s.damping*=coeff;
			}
			shp->initAnchors();

			//add
			scene.shapes.push_back(shp);

			//reset
			delete rect_start;
			rect_start=nullptr;
		}

		//removing stuff
		if(GetKey(olc::Key::X).bHeld) {
			mouse_constraint.clear();
			for(auto sit=scene.shapes.begin(); sit!=scene.shapes.end();) {
				const auto& s=*sit;
				if(s->contains(wld_mouse_pos)) {
					delete s;
					sit=scene.shapes.erase(sit);
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
			for(const auto& s:scene.shapes) {
				if(!s->contains(wld_mouse_pos)) continue;

				//add all of its points to the list
				for(int i=0; i<s->getNum(); i++) {
					mouse_constraint.emplace_back(&mouse_point, &s->points[i]);
				}
			}

			//otherwise,
			if(mouse_constraint.empty()) {
				//which edge of phys bounds is mouse on?
				vf2d tl=tv.WorldToScreen(scene.phys_bounds.min);
				vf2d br=tv.WorldToScreen(scene.phys_bounds.max);
				vf2d tr(br.x, tl.y);
				vf2d bl(tl.x, br.y);
				for(int i=0; i<4; i++) {
					vf2d a, b;
					switch(i) {
						case 0: a=tl, b=tr; break;
						case 1: a=tr, b=br; break;
						case 2: a=br; b=bl; break;
						case 3: a=bl, b=tl; break;
					}
					//get close pt on segment
					vf2d pa=scr_mouse_pos-a, ba=b-a;
					float t=pa.dot(ba)/ba.dot(ba);
					//corner index logic
					int s=1+2*i;
					if(t<0) t=0, s--;
					if(t>1) t=1, s++;
					vf2d close_pt=a+t*ba;
					//if mouse close enough, select section
					if((scr_mouse_pos-close_pt).mag()<15) {
						drag_section=s;
						break;
					}
				}
			}
		}
		if(grab_action.bHeld) {
			//corner/edge update logic
			vf2d& tl=scene.phys_bounds.min;
			vf2d& br=scene.phys_bounds.max;
			switch(drag_section) {
				case 0: tl=wld_mouse_pos; break;
				case 1: tl.y=wld_mouse_pos.y; break;
				case 2: tl.y=wld_mouse_pos.y, br.x=wld_mouse_pos.x; break;
				case 3: br.x=wld_mouse_pos.x; break;
				case 4: br=wld_mouse_pos; break;
				case 5: br.y=wld_mouse_pos.y; break;
				case 6: tl.x=wld_mouse_pos.x, br.y=wld_mouse_pos.y; break;
				case 7: tl.x=wld_mouse_pos.x; break;
			}
		}
		if(grab_action.bReleased) {
			//reset
			mouse_constraint.clear();
			drag_section=-1;
		}

		//anchoring
		if(GetKey(olc::Key::A).bPressed) {
			for(const auto& s:scene.shapes) {
				//toggle anchor status
				if(s->contains(wld_mouse_pos)) {
					s->anchored^=true;
				}
			}
		}

		//zooming
		if(GetMouseWheel()>0) tv.ZoomAtScreenPos(1.07f, scr_mouse_pos);
		if(GetMouseWheel()<0) tv.ZoomAtScreenPos(1/1.07f, scr_mouse_pos);

		//panning
		const auto pan_action=GetMouse(olc::Mouse::MIDDLE);
		if(pan_action.bPressed) tv.StartPan(scr_mouse_pos);
		if(pan_action.bHeld) tv.UpdatePan(scr_mouse_pos);
		if(pan_action.bReleased) tv.EndPan(scr_mouse_pos);

		//reset view
		if(GetKey(olc::Key::Z).bHeld) zoomToFit();

		//visual toggles
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;
		if(GetKey(olc::Key::S).bPressed) show_springs^=true;
		if(GetKey(olc::Key::W).bPressed) show_wireframes^=true;
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
		if(GetKey(olc::Key::M).bPressed) show_mass^=true;
		if(GetKey(olc::Key::V).bPressed) show_vertexes^=true;
	}

	void handlePhysics(float dt) {
		//update mouse constraint
		mouse_point.pos=wld_mouse_pos;
		for(auto& m:mouse_constraint) {
			m.update();
		}

		const int sub_steps=4;
		float sub_dt=dt/sub_steps;
		for(int i=0; i<sub_steps; i++) {
			scene.update(sub_dt);
		}
	}
#pragma endregion

	void update(float dt) {
		//make sure simulation doesnt blow up?
		dt=cmn::clamp(dt, 1/165.f, 1/30.f);

		scr_mouse_pos=GetMousePos();
		wld_mouse_pos=tv.ScreenToWorld(scr_mouse_pos);

		//only allow input when console closed
		if(!IsConsoleShowing()) handleUserInput();

		//open integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		//update physics when console closed or timing
		if(!IsConsoleShowing()||to_time) handlePhysics(dt);
	}

#pragma region RENDER HELPERS
	void tvDrawThickLine(const vf2d& a, const vf2d& b, float rad, const olc::Pixel& col=olc::WHITE) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=sub.perp()/len;

		float angle=std::atan2f(sub.y, sub.x);
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
		Clear(olc::Pixel(255, 254, 235));
		SetDecalMode(show_wireframes?olc::DecalMode::WIREFRAME:olc::DecalMode::NORMAL);

		if(show_grid) {
			const auto grid_spacing=1;
			const olc::Pixel grid_col(140, 222, 255);

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
		tvDrawAABB(scene.phys_bounds, .1f, olc::Pixel(255, 33, 122));

		//draw shapes
		for(const auto& shp:scene.shapes) {
			//draw bounds
			if(show_bounds) {
				cmn::AABB box=shp->getAABB();
				bool overlaps=false;
				for(const auto& s:scene.shapes) {
					if(s==shp) continue;
					if(box.overlaps(s->getAABB())) {
						overlaps=true;
						break;
					}
				}
				tvDrawAABB(box, .075f, overlaps?olc::RED:olc::GREEN);
			}

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
					tvDrawThickLine(a, b, .05f, olc::GREY);
					tvFillCircle(a, .05f, olc::GREY);
				}
			}

			//fill
			{
				//initialize indexes
				std::unordered_map<PointMass*, int> idx;
				for(int i=0; i<shp->getNum(); i++) {
					idx[&shp->points[i]]=i;
				}
				std::list<int> indexes;
				for(const auto& c:shp->constraints) {
					indexes.push_back(idx[c.a]);
				}

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
						tv.DrawWarpedDecal(prim_rect_dec, {pt_p, pt_c, pt_n, pt_n}, shp->col);

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

			if(show_vertexes) {
				for(int i=0; i<shp->getNum(); i++) {
					auto str=std::to_string(i);
					vf2d size(8*str.length(), 8);
					float scale=.05f;
					tv.FillRectDecal(shp->points[i].pos-scale*(size+1)/2, scale*size, olc::WHITE);
					tv.DrawStringDecal(shp->points[i].pos-scale*size/2, str, olc::BLACK, {scale, scale});
				}
			}
		}

		//show mouse constriant
		for(const auto& m:mouse_constraint) {
			tv.DrawLineDecal(m.a->pos, m.b->pos, olc::MAGENTA);
		}

		//show add rect
		if(rect_start) {
			cmn::AABB box;
			box.fitToEnclose(*rect_start);
			box.fitToEnclose(wld_mouse_pos);
			tvDrawAABB(box, .05f, olc::DARK_GREY);
		}
	}

	bool OnUserUpdate(float dt) override {
		cmn::Stopwatch update_watch;
		update_watch.start();
		update(dt);
		if(to_time) {
			update_watch.stop();
			auto us=update_watch.getMicros();
			std::cout<<"  update: "<<us<<"us ("<<(us/1000.f)<<"ms)\n";
		}

		cmn::Stopwatch render_watch;
		render_watch.start();
		render();
		if(to_time) {
			render_watch.stop();
			auto us=render_watch.getMicros();
			std::cout<<"  render: "<<us<<"us ("<<(us/1000.f)<<"ms)\n";

			to_time=false;
		}

		return true;
	}
};


int main() {
	JellyCarGame jcg;
	bool vsync=true;
	if(jcg.Construct(640, 480, 1, 1, false, vsync)) jcg.Start();

	return 0;
}