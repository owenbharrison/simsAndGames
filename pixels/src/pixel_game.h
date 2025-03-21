#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_TRANSFORMEDVIEW
#include "common/olcPGEX_TransformedView.h"

#include "pixel_set.h"

struct PixelGame : olc::PixelGameEngine {
	PixelGame() {
		sAppName="Pixels";
	}

	//helpful positions
	vf2d scr_mouse_pos;
	vf2d wld_mouse_pos;
	vf2d prev_wld_mouse_pos;

	//scene data
	std::list<PixelSet*> pixelsets;
	vf2d gravity;

	//UI toggles
	bool show_bounds=false;
	bool show_wireframes=false;
	bool show_grids=false;
	bool show_mass=false;
	bool show_outlines=true;
	bool show_ticks=false;

	bool update_phys=true;

	bool to_time=false;

	//user input things
	vf2d drag_start;
	PixelSet* drag_set=nullptr;

	vf2d spring_offset;
	PixelSet* spring_set=nullptr;

	std::vector<vf2d> addition;
	float addition_timer=0;

	//"primitive" render helpers
	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;
	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;

	olc::TransformedView tv;

	vf2d* ray_orig=nullptr;

	//place pixelsets such that their bounds lie inside the screen
	void placeAllRandomly() {
		for(const auto& p:pixelsets) {
			p->rot=random(2*Pi);
			p->old_rot=p->rot;

			//aabb needs cossin
			p->updateRot();
			AABB box=p->getAABB();

			//easier to just offset based on where it already is
			p->pos.x+=random(-box.min.x, ScreenWidth()-box.max.x);
			p->pos.y+=random(-box.min.y, ScreenHeight()-box.max.y);

			//reset vel
			p->old_pos=p->pos;
		}
	}

	bool OnUserCreate() override {
		srand(time(0));

		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";
		ConsoleCaptureStdOut(true);

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

		//setup transforms
		tv.Initialise(GetScreenSize());
		tv.SetScaleExtents({.33f, .33f}, {60, 60});
		tv.EnableScaleClamp(true);

#pragma region DEFAULT OBJECTS
		{//make floor
			PixelSet* thing=new PixelSet(15, 3);
			for(int i=0; i<thing->getW()*thing->getH(); i++) {
				thing->grid[i]=true;
			}
			thing->updateTypes();
			thing->updateMeshes();
			thing->updateOutlines();

			thing->scale=24;
			thing->updateMass();
			thing->updateInertia();

			pixelsets.push_back(thing);
		}

		{//make ball
			const int sz=19;
			PixelSet* thing=new PixelSet(sz, sz);
			float r=.5f*sz, r_sq=r*r;
			for(int x=0; x<sz; x++) {
				float dx=x-.5f*sz;
				for(int y=0; y<sz; y++) {
					float dy=y-.5f*sz;
					(*thing)(x, y)=dx*dx+dy*dy<r_sq;
				}
			}
			thing->updateTypes();
			thing->updateMeshes();
			thing->updateOutlines();

			thing->scale=6;
			thing->updateMass();
			thing->updateInertia();

			pixelsets.push_back(thing);
		}

		{//make triangle
			float rad=random(120, 240);
			std::vector<vf2d> pts;
			for(int i=0; i<3; i++) {
				float angle=2*Pi*i/3;
				pts.emplace_back(polar(rad, angle));
			}

			PixelSet* thing=new PixelSet(PixelSet::fromPolygon(pts, 10));
			thing->updateTypes();
			thing->updateMeshes();
			thing->updateOutlines();

			thing->updateMass();
			thing->updateInertia();

			pixelsets.push_back(thing);
		}
#pragma endregion

		placeAllRandomly();

		//eventually ill change this when collisions work :D
		gravity={0, 0};

		return true;
	}

	void resetMarkers() {
		spring_set=nullptr;
		drag_set=nullptr;
	}

	void reset() {
		resetMarkers();

		for(const auto& p:pixelsets) delete p;
		pixelsets.clear();
	}

	bool OnUserDestroy() override {
		reset();

		delete prim_rect_spr;
		delete prim_rect_dec;
		delete prim_circ_spr;
		delete prim_circ_dec;

		return true;
	}

#pragma region COMMAND HELPERS
	void resetCommand() {
		//english + logic = grammar :D
		int num=pixelsets.size();
		std::cout<<"removed ";
		std::cout<<num;
		std::cout<<" pixelset";
		if(num!=1) std::cout<<'s';
		std::cout<<'\n';

		reset();
	}

	void countCommand() {
		//english + logic = grammar :D
		int num=pixelsets.size();
		std::cout<<"there ";
		std::cout<<(num==1?"is ":"are ");
		std::cout<<num;
		std::cout<<" pixelset";
		if(num!=1) std::cout<<'s';
		std::cout<<'\n';
	}

	bool usageCommand() {
		if(pixelsets.empty()) {
			std::cout<<"n/a\n";

			return false;
		}

		//how many solid blocks for total allocated?
		int num=0, total=0;
		for(const auto& p:pixelsets) {
			for(int i=0; i<p->getW(); i++) {
				for(int j=0; j<p->getH(); j++) {
					if((*p)(i, j)!=PixelSet::Empty) num++;
					total++;
				}
			}
		}
		int pct=100.f*num/total;
		std::cout<<pct<<"% usage\n";

		return true;
	}

	bool exportCommand(std::stringstream& line_str) {
		std::string filename;
		line_str>>filename;
		if(filename.empty()) {
			std::cout<<"no filename. try using:\n  export <filename>\n";

			return false;
		}

		std::ofstream file_out(filename);
		if(file_out.fail()) {
			std::cout<<"invalid filename.\n";

			return false;
		}

		//print pixelsets line by line
		for(const auto& p:pixelsets) {
			//properties
			file_out<<"p "
				<<p->pos.x<<' '<<p->pos.y<<' '
				<<p->rot<<' '
				<<p->scale<<' '
				<<int(p->col.r)<<' '<<int(p->col.g)<<' '<<int(p->col.b)<<' '
				<<p->getW()<<' '<<p->getH()<<'\n';
			//grid
			for(int j=0; j<p->getH(); j++) {
				for(int i=0; i<p->getW(); i++) {
					file_out<<((*p)(i, j)!=PixelSet::Empty);
				}
				file_out<<'\n';
			}
		}
		file_out.close();

		std::cout<<"successfully exported to: "<<filename<<'\n';

		return true;
	}

	bool importCommand(std::stringstream& line_str) {
		std::string filename;
		line_str>>filename;
		if(filename.empty()) {
			std::cout<<"no filename. try using:\n  import <filename>\n";

			return false;
		}

		std::ifstream file_in(filename);
		if(file_in.fail()) {
			std::cout<<"invalid filename.\n";

			return false;
		}

		reset();

		//parse file line by line
		std::string line;
		while(std::getline(file_in, line)) {
			std::stringstream line_str(line);
			std::string type; line_str>>type;
			if(type=="p") {
				//read in data
				vf2d pos;
				line_str>>pos.x>>pos.y;
				float rot, scale;
				line_str>>rot>>scale;
				int r, g, b;
				line_str>>r>>g>>b;
				int w, h;
				line_str>>w>>h;

				//set transforms
				PixelSet p(w, h);
				p.pos=pos, p.old_pos=p.pos;
				p.rot=rot, p.old_rot=p.rot;
				p.updateRot();
				p.scale=scale;
				p.col=olc::Pixel(r, g, b);

				//read h lines
				for(int j=0; j<h; j++) {
					std::getline(file_in, line);
					line_str.clear(), line_str<<line;
					//read w "bits"
					for(int i=0; i<w; i++) {
						p(i, j)=line[i]=='1';
					}
				}

				p.updateTypes();
				p.updateMeshes();
				p.updateOutlines();

				p.updateMass();
				p.updateInertia();

				pixelsets.emplace_back(new PixelSet(p));
			}
		}

		std::cout<<"successfully imported from: "<<filename<<'\n';

		return true;
	}
#pragma endregion

	//not sure how olc::PixelGameEngine handles this return value...
	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		//if only there was a string switch statement :(

		if(cmd=="clear") {
			ConsoleClear();

			return true;
		}

		if(cmd=="reset") {
			resetCommand();

			return true;
		}

		if(cmd=="count") {
			countCommand();

			return true;
		}

		if(cmd=="usage") return usageCommand();

		if(cmd=="time") {
			to_time=true;

			return true;
		}

		if(cmd=="cast") {
			if(ray_orig) {
				delete ray_orig;
				ray_orig=nullptr;
				std::cout<<"raycasting off\n";
			} else {
				ray_orig=new vf2d(wld_mouse_pos);
				std::cout<<"raycasting on\n";
			}

			return true;
		}

		if(cmd=="export") return exportCommand(line_str);

		if(cmd=="import") return importCommand(line_str);

		if(cmd=="keybinds") {
			std::cout<<"useful keybinds:\n"
				"  A      drag while making a shape to add a new pixelset\n"
				"  S      drag to slice thru pixelsets\n"
				"  X      delete pixelset\n"
				"  R      place all pixelsets randomly\n"
				"  B      toggle bounding box view\n"
				"  W      toggle wireframe view\n"
				"  G      toggle local grids view\n"
				"  M      toggle mass view\n"
				"  O      toggle outline view\n"
				"  T      toggle grid ticks\n"
				"  P      toggle physics update\n"
				"  V      view relative pixel velocity\n"
				"  ESC    toggle integrated console\n"
				"  HOME   reset zoom and pan\n";

			return true;
		}

		if(cmd=="mousebinds") {
			std::cout<<"useful mousebinds:\n"
				"  LEFT     drag to move pixelsets\n"
				"  RIGHT    apply spring force to pixelset\n"
				"  MIDDLE   scroll to zoom, drag to pan\n";

			return true;
		}

		if(cmd=="help") {
			std::cout<<"useful commands:\n"
				"  clear        clears the screen\n"
				"  reset        removes all pixelsets\n"
				"  count        how many pixelsets are there?\n"
				"  usage        % space used for all allocated pixelsets\n"
				"  time         times immediate next update and render loop\n"
				"  cast         casts ray from chosen position to mouse\n"
				"  export       exports pixelsets to specified file\n"
				"  import       imports pixelsets from specified file\n"
				"  keybinds     which keys to press for this program?\n"
				"  mousebinds   which buttons to press for this program?\n";

			return true;
		}

		std::cout<<"unknown command. type help for list of commands.\n";

		return false;
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
		if(GetKey(olc::Key::HOME).bPressed) {
			tv.SetWorldOffset({0, 0});
			tv.SetWorldScale({1, 1});
		}

		//mouse grabbing
		const auto left_mouse=GetMouse(olc::Mouse::LEFT);
		if(left_mouse.bPressed) {
			drag_start=wld_mouse_pos;
			//which dragset is the mouse on?
			drag_set=nullptr;
			for(const auto& p:pixelsets) {
				//is mouse in bounds?
				if(!p->getAABB().contains(wld_mouse_pos)) continue;

				//are local coords valid?
				vf2d ij=p->worldToLocal(wld_mouse_pos);
				int i=std::floor(ij.x), j=std::floor(ij.y);
				if(!p->inRangeX(i)||!p->inRangeY(j)) continue;

				//is the block solid?
				if((*p)(i, j)!=PixelSet::Empty) {
					drag_set=p;
					break;
				}
			}
		}
		if(left_mouse.bHeld) {
			//update dragset pos
			if(drag_set) {
				drag_set->pos+=wld_mouse_pos-drag_start;
				//reset vel?
				drag_set->old_pos=drag_set->pos;
			}
			drag_start=wld_mouse_pos;
		}
		if(left_mouse.bReleased) {
			//let go of that sucker
			drag_set=nullptr;
		}

		//spring interactivity
		const auto right_mouse=GetMouse(olc::Mouse::RIGHT);
		if(right_mouse.bPressed) {
			spring_set=nullptr;
			for(const auto& p:pixelsets) {
				//is mouse in pixelset bounds?
				if(!p->getAABB().contains(wld_mouse_pos)) continue;

				//is local point in bounds?
				vf2d ij=p->worldToLocal(wld_mouse_pos);
				int i=std::floor(ij.x), j=std::floor(ij.y);
				if(!p->inRangeX(i)||!p->inRangeY(j)) continue;

				//ensure local coordinate a solid block
				if((*p)(i, j)==PixelSet::Empty) continue;

				//store the offset and the pixelset
				spring_offset=ij;
				spring_set=p;
				break;
			}
		}
		if(right_mouse.bHeld) {
			if(spring_set) {
				//F=kx
				vf2d spr_pos=spring_set->localToWorld(spring_offset);
				vf2d sub=wld_mouse_pos-spr_pos;
				//spring force proportional to pixelset mass
				spring_set->applyForce(spring_set->total_mass*sub, spr_pos);
			}
		}
		if(right_mouse.bReleased) spring_set=nullptr;

		//add objects
		const auto a_key=GetKey(olc::Key::A);
		if(a_key.bPressed) addition.clear();
		//every now and then...
		if(addition_timer<0) {
			addition_timer=.05f;

			if(a_key.bHeld) {
				//dont put points too close
				bool exist=false;
				for(const auto& a:addition) {
					if((a-wld_mouse_pos).mag()<1) {
						exist=true;
						break;
					}
				}
				//add point to addition
				if(!exist) addition.emplace_back(wld_mouse_pos);
			}
		}
		addition_timer-=dt;
		if(a_key.bReleased) {
			//ensure its a polygon
			if(addition.size()>=3) {
				//choose random scale
				float scale=random(1, 10);
				PixelSet* thing=new PixelSet(PixelSet::fromPolygon(addition, scale));

				pixelsets.emplace_back(thing);
			}
			addition.clear();
		}

		//slice objects
		const auto s_key=GetKey(olc::Key::S);
		if(s_key.bHeld) {
			resetMarkers();

			//check every pixelset
			for(auto it=pixelsets.begin(); it!=pixelsets.end();) {
				const auto& p=*it;
				if(p->slice(prev_wld_mouse_pos, wld_mouse_pos)) {
					//get new pixelsets
					auto split=p->floodfill();

					//deallocate and remove
					delete p;
					it=pixelsets.erase(it);

					//allocate all new
					for(const auto& s:split) {
						pixelsets.emplace_front(new PixelSet(s));
					}
				} else it++;
			}
		}

		//remove objects
		if(GetKey(olc::Key::X).bPressed) {
			resetMarkers();

			for(auto it=pixelsets.begin(); it!=pixelsets.end();) {

				//is mouse in bounds?
				const auto& p=*it;
				if(p->getAABB().contains(wld_mouse_pos)) {

					//are local coords valid?
					vf2d ij=p->worldToLocal(wld_mouse_pos);
					int i=ij.x, j=ij.y;
					if(p->inRangeX(i)&&p->inRangeY(j)) {

						//is the block solid?
						if((*p)(i, j)!=PixelSet::Empty) {
							//deallocate and remove
							delete p;
							it=pixelsets.erase(it);
							continue;
						}
					}
				}

				it++;
			}
		}

		//randomize rotations
		if(GetKey(olc::Key::R).bPressed) placeAllRandomly();

		//visual toggles with xor_equal
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
		if(GetKey(olc::Key::W).bPressed) show_wireframes^=true;
		if(GetKey(olc::Key::G).bPressed) show_grids^=true;
		if(GetKey(olc::Key::M).bPressed) show_mass^=true;
		if(GetKey(olc::Key::O).bPressed) show_outlines^=true;
		if(GetKey(olc::Key::P).bPressed) update_phys^=true;
		if(GetKey(olc::Key::T).bPressed) show_ticks^=true;
	}

	void handlePhysics(float dt) {
		//dynamics
		for(const auto& p:pixelsets) {
			p->applyForce(p->total_mass*gravity, p->localToWorld(p->center_of_mass));
			p->update(dt);
		}

		//for every pixelset
		for(const auto& p:pixelsets) {
			//check every other
			for(const auto& o:pixelsets) {
				//dont check self
				if(o==p) continue;

				p->collide(*o);
			}
		}
	}
#pragma endregion

	void update(float dt) {
		prev_wld_mouse_pos=wld_mouse_pos;
		scr_mouse_pos=GetMousePos();
		wld_mouse_pos=tv.ScreenToWorld(scr_mouse_pos);

		//open and close the integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		//only allow input when console NOT open
		if(!IsConsoleShowing()) handleUserInput(dt);

		//clear colliding displays
		for(const auto& p:pixelsets) {
			memset(p->colliding, false, sizeof(bool)*p->getW()*p->getH());
		}

		//after import, something looks wrong...
		//are meshes not updated?
		if(update_phys) handlePhysics(dt);
	}

#pragma region RENDER HELPERS
	void DrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float rad, olc::Pixel col) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=perp(sub/len);

		float angle=std::atan2f(sub.y, sub.x);
		DrawRotatedDecal(a-rad*tang, prim_rect_dec, angle, {0, 0}, {len, 2*rad}, col);
	}

	void tvDrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float rad, olc::Pixel col) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=perp(sub/len);

		float angle=std::atan2f(sub.y, sub.x);
		tv.DrawRotatedDecal(a-rad*tang, prim_rect_dec, angle, {0, 0}, {len, 2*rad}, col);
	}

	void tvFillCircleDecal(const olc::vf2d& pos, float rad, olc::Pixel col) {
		olc::vf2d offset(rad, rad);
		olc::vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		tv.DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	void renderWorldGrid(olc::Pixel col) {
		//how can i make these adaptive?
		float grid_spacing=25;

		//screen bounds in world space, snap to nearest
		vf2d tl=tv.GetWorldTL(), br=tv.GetWorldBR();
		int i_s=std::floor(tl.x/grid_spacing), j_s=std::floor(tl.y/grid_spacing);
		int i_e=std::ceil(br.x/grid_spacing), j_e=std::ceil(br.y/grid_spacing);

		//vert
		for(int i=i_s; i<=i_e; i++) {
			float x=grid_spacing*i;
			vf2d top{x, tl.y}, btm{x, br.y};
			//major and minor lines
			if(i%5==0) tvDrawThickLine(top, btm, 2.2f, col);
			else tv.DrawLineDecal(top, btm, col);
		}

		//horiz
		for(int j=j_s; j<=j_e; j++) {
			float y=grid_spacing*j;
			vf2d lft{tl.x, y}, rgt{br.x, y};
			//major and minor lines
			if(j%5==0) tvDrawThickLine(lft, rgt, 2.2f, col);
			else tv.DrawLineDecal(lft, rgt, col);
		}

		//show x & y labels
		if(show_ticks) {
			//x
			for(int i=i_s; i<=i_e; i++) {
				float x=grid_spacing*i;
				vf2d top{x, tl.y}, btm{x, br.y};
				if(i%5==0) {//major tick
					auto str=std::to_string(int(x));
					vf2d offset(4*str.length(), 0);
					tv.FillRectDecal(top-offset-vf2d(1, 1), vf2d(1+8*str.length(), 9));
					tv.DrawStringDecal(top-offset, str, olc::BLACK);
				}
			}

			//y
			for(int j=j_s; j<=j_e; j++) {
				float y=grid_spacing*j;
				vf2d lft{tl.x, y}, rgt{br.x, y};
				if(j%5==0) {//major ticks
					auto str=std::to_string(int(y));
					vf2d offset(0, 4);
					tv.FillRectDecal(lft-offset-vf2d(1, 1), vf2d(1+8*str.length(), 9));
					tv.DrawStringDecal(lft-offset, str, olc::BLACK);
				}
			}
		}
	}

	void renderPixelSet(const PixelSet& p) {
		//draw rectangle bounding box
		if(show_bounds) {
			//color if it overlaps
			AABB box=p.getAABB();
			olc::Pixel col=olc::GREEN;
			for(const auto& o:pixelsets) {
				//dont check self
				if(o==&p) continue;

				if(box.overlaps(o->getAABB())) {
					col=olc::RED;
					break;
				}
			}

			const float rad=.25f*p.scale;
			vf2d v1(box.max.x, box.min.y);
			vf2d v2(box.min.x, box.max.y);
			tvDrawThickLine(box.min, v1, rad, col);
			tvFillCircleDecal(box.min, rad, col);
			tvDrawThickLine(v1, box.max, rad, col);
			tvFillCircleDecal(v1, rad, col);
			tvDrawThickLine(box.max, v2, rad, col);
			tvFillCircleDecal(box.max, rad, col);
			tvDrawThickLine(v2, box.min, rad, col);
			tvFillCircleDecal(v2, rad, col);
		}

		//show local grids
		if(show_grids) {
			//vert lines
			for(int i=0; i<=p.getW(); i++) {
				vf2d btm=p.localToWorld(vf2d(i, 0));
				vf2d top=p.localToWorld(vf2d(i, p.getH()));
				tv.DrawLineDecal(btm, top, olc::DARK_GREY);
			}

			//horiz lines
			for(int j=0; j<=p.getH(); j++) {
				vf2d lft=p.localToWorld(vf2d(0, j));
				vf2d rgt=p.localToWorld(vf2d(p.getW(), j));
				tv.DrawLineDecal(lft, rgt, olc::DARK_GREY);
			}

			//draw x & y axes
			vf2d xs=p.localToWorld(vf2d(-1, 0)), xe=p.localToWorld(vf2d(p.getW()+1, 0));
			vf2d ys=p.localToWorld(vf2d(0, -1)), ye=p.localToWorld(vf2d(0, p.getH()+1));
			tvDrawThickLine(xs, xe, .13f*p.scale, olc::BLACK);
			tvDrawThickLine(ys, ye, .13f*p.scale, olc::BLACK);
		}

		//render meshes
		for(const auto& m:p.meshes) {
			vf2d pos=p.localToWorld(vf2d(m.i, m.j));
			vf2d size=p.scale*vf2d(m.w, m.h);
			tv.DrawRotatedDecal(pos, prim_rect_dec, p.rot, {0, 0}, size, m.col);
		}

		//render outlines
		if(show_outlines) {
			const float amt=.17f;
			for(const auto& o:p.outlines) {
				vf2d st(o.i, o.j), en=st;
				if(o.vert) st.y-=amt, en.y+=amt+o.sz;
				else st.x-=amt, en.x+=amt+o.sz;
				tvDrawThickLine(p.localToWorld(st), p.localToWorld(en), amt*p.scale, o.col);
			}
		}
	}
#pragma endregion

	void render(float dt) {
		//draw grey background
		Clear(olc::Pixel(143, 201, 255));
		SetDecalMode(show_wireframes?olc::DecalMode::WIREFRAME:olc::DecalMode::NORMAL);
		
		renderWorldGrid(olc::Pixel(25, 131, 230));

		//show addition
		if(addition.size()) {
			vf2d prev;
			for(int i=0; i<addition.size(); i++) {
				const auto& a=addition[i];
				const auto& b=addition[(i+1)%addition.size()];
				tvDrawThickLine(a, b, 1, olc::WHITE);
			}
		}

		//show all pixelsets
		for(const auto& p:pixelsets) {
			renderPixelSet(*p);

#pragma region EXPERIMENTAL
			//render colliding pixels
			for(int i=0; i<p->getW(); i++) {
				for(int j=0; j<p->getH(); j++) {
					if(!p->colliding[p->ix(i, j)]) continue;

					vf2d pos=p->localToWorld(vf2d(i, j));
					vf2d size(p->scale, p->scale);
					tv.DrawRotatedDecal(pos, prim_rect_dec, p->rot, {0, 0}, size, olc::RED);
				}
			}

			//show hover block
			//is mouse in bounds?
			if(p->getAABB().contains(wld_mouse_pos)) {
				//are local coords valid?
				vf2d ij=p->worldToLocal(wld_mouse_pos);
				int i=std::floor(ij.x), j=std::floor(ij.y);
				if(p->inRangeX(i)&&p->inRangeY(j)) {
					//is the block solid?
					if((*p)(i, j)!=PixelSet::Empty) {
						vf2d pos=p->localToWorld(vf2d(i, j));
						vf2d size(p->scale, p->scale);
						tv.DrawRotatedDecal(pos, prim_rect_dec, p->rot, {0, 0}, size, olc::GREEN);

						//show relative velocity of hover block
						if(GetKey(olc::Key::V).bHeld) {
							tvDrawThickLine(wld_mouse_pos, p->pos, 1, olc::BLACK);
							tvFillCircleDecal(p->pos, 2.f, olc::BLUE);
							vf2d vel=p->getRelativeVel(wld_mouse_pos)/dt;
							tvDrawThickLine(wld_mouse_pos, wld_mouse_pos+vel, 1, olc::GREEN);
							tvFillCircleDecal(wld_mouse_pos, 2.f, olc::RED);
						}
					}
				}
			}

			//show center of mass
			if(show_mass) {
				vf2d pos=p->localToWorld(p->center_of_mass);
				tvFillCircleDecal(pos, .33f*p->scale, olc::MAGENTA);

				const float scl=p->scale/8;

				//show mass string
				auto mass_str="M="+std::to_string(int(p->total_mass));
				vf2d mass_offset(4*mass_str.length(), 8);
				tv.DrawStringDecal(pos-scl*mass_offset, mass_str, olc::BLACK, {scl, scl});

				//show inertia string
				auto inertia_str="I="+std::to_string(int(p->moment_of_inertia));
				vf2d inertia_offset(4*inertia_str.length(), 0);
				tv.DrawStringDecal(pos-scl*inertia_offset, inertia_str, olc::BLACK, {scl, scl});
			}
		}

		//raycast test
		if(ray_orig) {
			vf2d orig=*ray_orig;
			vf2d dir=wld_mouse_pos-orig;
			float mag=dir.mag();
			if(mag>1e-6f) dir/=mag;

			//find closest shape
			float record=mag;
			PixelSet* closest=nullptr;
			for(const auto& p:pixelsets) {
				float dist=p->raycast(orig, dir);
				if(dist>0&&dist<record) closest=p, record=dist;
			}

			//draw said color ray to shape
			if(closest) {
				vf2d ix=orig+record*dir;
				tvDrawThickLine(orig, ix, 1, closest->col);
				tvFillCircleDecal(ix, 3, closest->col);
			} else tvDrawThickLine(orig, wld_mouse_pos, 1, olc::WHITE);
			tvFillCircleDecal(orig, 3, olc::BLACK);
		}
#pragma endregion

		//show spring set
		if(spring_set) {
			vf2d spr_pos=spring_set->localToWorld(spring_offset);
			tvDrawThickLine(wld_mouse_pos, spr_pos, 1, olc::YELLOW);
			tvFillCircleDecal(spr_pos, .33f*spring_set->scale, olc::YELLOW);
		}

		if(!update_phys) {
			vf2d tl(0, 0), br=GetScreenSize();
			vf2d tr(br.x, 0), bl(0, br.y);
			DrawThickLine(tl, tr, 3, olc::RED);
			DrawThickLine(tr, br, 3, olc::RED);
			DrawThickLine(br, bl, 3, olc::RED);
			DrawThickLine(bl, tl, 3, olc::RED);
		}
	}

	//just a wrapper for timing and logic separation
	bool OnUserUpdate(float dt) override {
		auto update_start=std::chrono::steady_clock::now();
		update(dt);
		if(to_time) {
			auto end=std::chrono::steady_clock::now();
			auto dur=std::chrono::duration_cast<std::chrono::microseconds>(end-update_start);
			auto us=dur.count();
			auto ms=us/1000.f;
			std::cout<<"update: "<<us<<"us ("<<ms<<"ms)\n";
		}

		auto render_start=std::chrono::steady_clock::now();
		render(dt);
		if(to_time) {
			auto end=std::chrono::steady_clock::now();
			auto dur=std::chrono::duration_cast<std::chrono::microseconds>(end-render_start);
			auto us=dur.count();
			auto ms=us/1000.f;
			std::cout<<"render: "<<us<<"us ("<<ms<<"ms)\n";

			to_time=false;
		}

		return true;
	}
};