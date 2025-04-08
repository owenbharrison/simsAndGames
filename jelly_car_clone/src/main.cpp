#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_TRANSFORMEDVIEW
#include "common/olcPGEX_TransformedView.h"

#include "phys/shape.h"

#include <sstream>
#include <fstream>

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
	bool show_wireframes=false;

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

		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";
		//std::cout diverted to console
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
			std::cout<<"  no filename. try using:\n  export <filename>\n";
			return false;
		}

		std::ofstream file_out(filename);
		if(file_out.fail()) {
			std::cout<<"  invalid filename.\n";
			return false;
		}

		//print shapes line by line
		for(const auto& shp:shapes) {
			//shape designation
			//shp <#pts> <#csts> <#sprs>
			file_out<<"shp "<<shp->getNum()<<' '
				<<shp->constraints.size()<<' '
				<<shp->springs.size()<<'\n';
			//print points
			//p <x> <y> <mass> <?locked>
			for(int i=0; i<shp->getNum(); i++) {
				const auto& p=shp->points[i];
				file_out<<"  p "<<
					p.pos.x<<' '<<
					p.pos.y<<' '<<
					p.mass<<' '<<
					p.locked<<'\n';
			}

			//make lookup table
			std::unordered_map<PointMass*, int> indexes;
			for(int i=0; i<shp->getNum(); i++) {
				indexes[&shp->points[i]]=i;
			}

			//print constraints
			//c <a> <b> <len>
			for(const auto& c:shp->constraints) {
				file_out<<"  c "<<
					indexes[c.a]<<' '<<
					indexes[c.b]<<' '<<
					c.rest_len<<'\n';
			}

			//print springs
			//s <a> <b> <len> <stiff> <damp>
			for(const auto& s:shp->springs) {
				file_out<<"  s "<<
					indexes[s.a]<<' '<<
					indexes[s.b]<<' '<<
					s.rest_len<<' '<<
					s.stiffness<<' '<<
					s.damping<<'\n';
			}

			//print anchors
			//a <x> <y>
			for(int i=0; i<shp->getNum(); i++) {
				const auto& a=shp->anchors[i];
				file_out<<"  a "<<
					a.x<<' '<<
					a.y<<'\n';
			}

			//print orientation
			//o <x> <y> <rot> <?anchored>
			file_out<<"  o "<<
				shp->anchor_pos.x<<' '<<
				shp->anchor_pos.y<<' '<<
				shp->anchor_rot<<' '<<
				shp->anchored<<'\n';

			//print tint
			//t <r> <g> <b>
			file_out<<"  t "<<
				int(shp->col.r)<<' '<<
				int(shp->col.g)<<' '<<
				int(shp->col.b)<<'\n';
		}

		std::cout<<"  successfully exported to "<<filename<<'\n';

		file_out.close();
		return true;
	}

	bool importCommand(std::string& filename) {
		if(filename.empty()) {
			std::cout<<"  no filename. try using:\n  import <filename>\n";
			return false;
		}

		std::ifstream file_in(filename);
		if(file_in.fail()) {
			std::cout<<"  invalid filename.\n";
			return false;
		}

		reset();

		//parse file line by line
		std::string line;
		while(std::getline(file_in, line)) {
			//get header
			std::stringstream line_str(line);

			//is this a shape?
			std::string type;
			line_str>>type;
			if(type=="shp") {
				//parse header
				int num_pts=-1, num_csts=-1, num_sprs=-1;
				line_str>>
					num_pts>>
					num_csts>>
					num_sprs;
				if(num_pts<=0||num_csts<0||num_sprs<0) {
					std::cout<<"  invalid header.\n";
					return false;
				}

				//construct
				Shape shp(num_pts);

				//points
				for(int i=0; i<num_pts; i++) {
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="p") {
						std::cout<<"  expected point.\n";
						return false;
					}

					//parse data & construct
					PointMass& p=shp.points[i];
					line_str>>
						p.pos.x>>
						p.pos.y>>
						p.mass>>
						p.locked;
					p.oldpos=p.pos;
				}

				//constraints
				for(int i=0; i<num_csts; i++) {
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="c") {
						std::cout<<"  expected constraint.\n";
						return false;
					}

					//parse data
					int a, b;
					float l;
					line_str>>a>>b>>l;
					if(a<0||b<0||a>=num_pts||b>=num_pts) {
						std::cout<<"  invalid constraint.\n";
						return false;
					}

					//construct
					Constraint c;
					c.a=&shp.points[a];
					c.b=&shp.points[b];
					c.rest_len=l;
					shp.constraints.push_back(c);
				}

				//springs
				for(int i=0; i<num_sprs; i++) {
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="s") {
						std::cout<<"  expected spring.\n";
						return false;
					}

					//parse data
					int a, b;
					float l, k, d;
					line_str>>a>>b>>l>>k>>d;
					if(a<0||b<0||a>=num_pts||b>=num_pts) {
						std::cout<<"  invalid spring.\n";
						return false;
					}

					//construct
					Spring s;
					s.a=&shp.points[a];
					s.b=&shp.points[b];
					s.rest_len=l;
					s.stiffness=k;
					s.damping=d;
					shp.springs.push_back(s);
				}

				//anchors
				for(int i=0; i<num_pts; i++) {
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="a") {
						std::cout<<"  expected anchor.\n";
						return false;
					}

					//parse data
					vf2d& a=shp.anchors[i];
					line_str>>
						a.x>>
						a.y;
				}

				//orientation
				{
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="o") {
						std::cout<<"  expected orientation.\n";
						return false;
					}

					//parse data
					line_str>>
						shp.anchor_pos.x>>
						shp.anchor_pos.y>>
						shp.anchor_rot>>
						shp.anchored;
				}

				//tint
				{
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="t") {
						std::cout<<"  expected tint.\n";
						return false;
					}

					//parse data
					int r, g, b;
					line_str>>r>>g>>b;

					//construct
					shp.col.r=r;
					shp.col.g=g;
					shp.col.b=b;
				}

				//add to list
				shapes.push_back(new Shape(shp));
			}
		}

		std::cout<<"  successfully imported from "<<filename<<'\n';

		file_in.close();
		return true;
	}
#pragma endregion

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") {
			ConsoleClear();

			return true;
		}

		else if(cmd=="reset") {
			//english+logic=grammar :D
			int num=shapes.size();
			std::cout<<"  removed "<<num<<" shape";
			if(num!=1) std::cout<<'s';
			std::cout<<'\n';
			reset();

			return true;
		}

		else if(cmd=="count") {
			//english + logic = grammar :D
			int num=shapes.size();
			std::cout<<"  there ";
			std::cout<<(num==1?"is":"are");
			std::cout<<' '<<num<<" shape";
			if(num!=1) std::cout<<'s';
			std::cout<<'\n';

			return true;
		}

		if(cmd=="export") {
			std::string filename;
			line_str>>filename;
			return exportCommand(filename);
		}

		if(cmd=="import") {
			std::string filename;
			line_str>>filename;
			return importCommand(filename);
		}

		if(cmd=="keybinds") {
			std::cout<<"useful keybinds:\n"
				"  R     drag to add rect\n"
				"  A     anchor shape\n"
				"  X     remove shape\n"
				"  G     toggle grid\n"
				"  S     toggle spring view\n"
				"  W     toggle wireframe view\n"
				"  M     toggle mass view\n"
				"  Z     zoom to fit\n"
				"  ESC   toggle integrated console\n";

			return true;
		}

		if(cmd=="mousebinds") {
			std::cout<<"useful mousebinds:\n"
				"  LEFT     drag to move shapes\n"
				"  RIGHT    add random ngon\n"
				"  MIDDLE   scroll to zoom, drag to pan\n";

			return true;
		}

		if(cmd=="help") {
			std::cout<<"useful commands:\n"
				"  clear        clears the console\n"
				"  reset        removes all shapes\n"
				"  count        how many shapes are there?\n"
				"  export       exports shapes to specified file\n"
				"  import       imports shapes from specified file\n"
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
			shapes.emplace_back(s);
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
		if(GetKey(olc::Key::W).bPressed) show_wireframes^=true;
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
		Clear(olc::WHITE);
		SetDecalMode(show_wireframes?olc::DecalMode::WIREFRAME:olc::DecalMode::NORMAL);

		if(show_grid) {
			const auto grid_spacing=1;
			const olc::Pixel grid_col(0, 187, 255);

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
		tvDrawAABB(phys_bounds, .1f, olc::Pixel(255, 33, 122));

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
					tvDrawThickLine(a, b, .05f, olc::GREY);
					tvFillCircle(a, .05f, olc::GREY);
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