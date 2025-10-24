//dont use exceptions? std::string& msg
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_TRANSFORMEDVIEW
#include "olcPGEX_TransformedView.h"

#include "phys/shape.h"

#include "scene.h"

#include "common/stopwatch.h"

#include <deque>

//https://www.rapidtables.com/convert/color/hsv-to-rgb.html
static olc::Pixel hsv2rgb(int h, float s, float v) {
	float c=v*s;
	float x=c*(1-std::abs(1-std::fmod(h/60.f, 2)));
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

static olc::Pixel randomPastel() {
	//hue=pure color
	int h=std::rand()%360;
	//saturation=intensity
	float s=cmn::randFloat(.1f, .4f);
	//value=brightness
	float v=cmn::randFloat(.75f, 1);
	return hsv2rgb(h, s, v);
}

class JellyCarGame : public olc::PixelGameEngine {
	//rendering "primitives"
	olc::Renderable prim_rect;
	olc::Renderable prim_circ;

	//user input stuff
	olc::TransformedView tv;

	vf2d scr_mouse_pos, wld_mouse_pos;

	vf2d* rect_start=nullptr;

	int drag_section=-1;

	std::list<Spring> mouse_springs;
	PointMass mouse_point;

	std::deque<vf2d> cut;

	float cut_timer=0;
	bool cut_anim=false;

	//simulation stuff
	Scene scene;
	const int num_sub_steps=4;
	const float time_step=1/60.f;
	const float sub_time_step=time_step/num_sub_steps;
	float update_timer=0;

	//graphics toggles
	bool show_grid=true;
	bool show_springs=false;
	bool show_mass=false;
	bool show_wireframes=false;
	bool show_bounds=false;
	bool show_vertexes=false;
	bool to_time=false;

	//cut animation
	float smooth_dx=0, smooth_dy=0;
	olc::Renderable cut_tex;

	bool OnUserCreate() override {
		std::srand(std::time(0));

		//init render "primitives"
		prim_rect.Create(1, 1);
		prim_rect.Sprite()->SetPixel(0, 0, olc::WHITE);
		prim_rect.Decal()->Update();
		{
			const unsigned int sz=1024;
			prim_circ.Create(sz, sz);
			SetDrawTarget(prim_circ.Sprite());
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2, olc::WHITE);
			SetDrawTarget(nullptr);
			prim_circ.Decal()->Update();
		}

		//init transformed view
		tv.Initialise(GetScreenSize(), {1, 1});
		tv.SetScaleExtents({5, 5}, {1000, 1000});
		tv.EnableScaleClamp(true);

		//init physics stuff
		mouse_point.locked=true;

		//init& zoom to phys_bounds
		scene.phys_bounds={{-14, -10}, {14, 10}};
		zoomToFit();

		if(!cut_tex.Load("assets/scissor.png")) {
			std::cout<<"  unable to load cut tex\n";
			return false;
		}
		
		std::cout<<"Press ESC for integrated console.\n"
			"  then type help for help.\n";

		//divert std::cout to console
		//ConsoleCaptureStdOut(true);

		return true;
	}

	void reset() {
		handleGrabActionEnd();

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

		return true;
	}
	
#pragma region COMMAND HELPERS
	bool callResetCommand() {
		//english+logic=grammar :D
		int num=scene.shapes.size();
		std::cout<<"  removed "<<num<<" shape";
		if(num!=1) std::cout<<'s';
		std::cout<<'\n';
		reset();

		return true;
	}

	bool callCountCommand() {
		//english + logic = grammar :D
		int num=scene.shapes.size();
		std::cout<<"  there ";
		std::cout<<(num==1?"is":"are");
		std::cout<<' '<<num<<" shape";
		if(num!=1) std::cout<<'s';
		std::cout<<'\n';

		return true;
	}

	bool callExportCommand(std::stringstream& args) {
		std::string filename;
		args>>filename;
		try {
			scene.save(filename);

			std::cout<<"  successfully exported to: "<<filename<<'\n';

			return true;
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}
	}

	bool callImportCommand(std::stringstream& args) {
		std::string filename;
		args>>filename;
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
#pragma endregion

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") {
			ConsoleClear();

			return true;
		}

		if(cmd=="reset") {
			return callResetCommand();
		}

		if(cmd=="count") {
			return callCountCommand();
		}

		if(cmd=="time") {
			to_time=true;

			return true;
		}

		if(cmd=="export") {
			return callExportCommand(line_str);
		}

		if(cmd=="import") {
			return callImportCommand(line_str);
		}

		if(cmd=="keybinds") {
			std::cout<<
				"  R     drag to add rect\n"
				"  A     anchor shape\n"
				"  C     cut shape by curve\n"
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
	void handleResizeActionBegin() {
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

	void handleResizeActionUpdate() {
		vf2d& tl=scene.phys_bounds.min;
		vf2d& br=scene.phys_bounds.max;
		//corner/edge update logic
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

	void handleResizeActionEnd() {
		drag_section=-1;
	}

	//make sure points are far enough away
	void handleCutActionUpdate() {
		bool unique=true;
		for(const auto& c:cut) {
			if((c-wld_mouse_pos).mag()<.75f) {
				unique=false;
				break;
			}
		}
		if(unique) cut.push_back(wld_mouse_pos);
	}

	void handleCutActionEnd() {
		cut.clear();
	}

	void handleGrabActionEnd() {
		mouse_springs.clear();
	}

	bool handleGrabActionBegin() {
		handleGrabActionEnd();

		//update for len_rest calculation
		mouse_point.pos=wld_mouse_pos;

		//for every shape under mouse
		bool added=false;
		for(const auto& shp:scene.shapes) {
			if(!shp->contains(wld_mouse_pos)) continue;

			//add all of its points to the list
			for(int i=0; i<shp->getNum(); i++) {
				Spring s(&mouse_point, &shp->points[i]);
				mouse_springs.push_back(s);
				added=true;
			}
		}

		return added;
	}

	void handleRectActionBegin() {
		rect_start=new vf2d(wld_mouse_pos);
	}

	void handleRectActionEnd() {
		//get bounds
		cmn::AABB box;
		box.fitToEnclose(*rect_start);
		box.fitToEnclose(wld_mouse_pos);

		//add
		float res=cmn::randFloat(.75f, 2);
		scene.shapes.push_back(new Shape(box, res));

		//reset
		delete rect_start;
		rect_start=nullptr;
	}

	void handleUserInput() {
		//adding ngons
		if(GetMouse(olc::Mouse::RIGHT).bPressed) {
			int num=cmn::randInt(4, 15);
			float rad=cmn::randFloat(1, 3);
			Shape* s=new Shape(wld_mouse_pos, rad, num);
			s->col=randomPastel();
			scene.shapes.push_back(s);
		}

		//adding rectangles
		const auto rect_action=GetKey(olc::Key::R);
		if(rect_action.bPressed) handleRectActionBegin();
		if(rect_action.bReleased) handleRectActionEnd();

		//every now and then, add cut point
		const auto cut_action=GetKey(olc::Key::C);
		if(cut_action.bHeld) handleCutActionUpdate();
		if(cut_action.bReleased) handleCutActionEnd();

		//removing stuff
		if(GetKey(olc::Key::X).bHeld) {
			handleGrabActionEnd();

			scene.removeShapeAt(wld_mouse_pos);
		}

		//mouse springs
		const auto grab_action=GetMouse(olc::Mouse::LEFT);
		if(grab_action.bPressed) {
			if(!handleGrabActionBegin()) {
				handleResizeActionBegin();
			}
		}
		if(grab_action.bHeld) handleResizeActionUpdate();
		if(grab_action.bReleased) {
			handleGrabActionEnd();
			handleResizeActionEnd();
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
		//update mouse point
		mouse_point.pos=wld_mouse_pos;

		//sanitize cut
		{
			float length=0;
			for(int i=1; i<cut.size(); i++) {
				length+=(cut[i]-cut[i-1]).mag();
			}
			if(length>20) cut.pop_front();
		}

		//cut animation
		if(cut_timer<0) {
			cut_timer+=.5f;

			cut_anim^=true;
		}
		cut_timer-=dt;

		//ensure similar update across multiple framerates
		update_timer+=dt; 
		while(update_timer>time_step) {
			for(int i=0; i<num_sub_steps; i++) {
				//update mouse springs
				for(auto& s:mouse_springs) {
					s.update();
					std::cout<<s.len_rest<<'\n';
				}

				scene.update(sub_time_step);
			}

			update_timer-=time_step;
		}
	}
#pragma endregion

	void update(float dt) {
		{//smooth mouse derivatives
			vf2d new_scr_mouse_pos=GetMousePos();

			float dx=new_scr_mouse_pos.x-scr_mouse_pos.x;
			float dy=new_scr_mouse_pos.y-scr_mouse_pos.y;

			float smoothing=.1f;
			smooth_dx+=smoothing*(dx-smooth_dx);
			smooth_dy+=smoothing*(dy-smooth_dy);

			scr_mouse_pos=GetMousePos();
		}
		wld_mouse_pos=tv.ScreenToWorld(scr_mouse_pos);
		
		//only allow input when console closed
		if(!IsConsoleShowing()) handleUserInput();

		//open integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		//update physics when console closed or timing
		if(!IsConsoleShowing()||to_time) handlePhysics(dt);
	}

#pragma region RENDER HELPERS
	void tvDrawThickLine(const vf2d& a, const vf2d& b, float rad, const olc::Pixel& col) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=sub.perp()/len;

		float angle=std::atan2(sub.y, sub.x);
		tv.DrawRotatedDecal(a-rad*tang, prim_rect.Decal(), angle, {0, 0}, {len, 2*rad}, col);
	}

	void tvDrawAABB(const cmn::AABB& aabb, float rad, const olc::Pixel& col) {
		vf2d tr(aabb.max.x, aabb.min.y), bl(aabb.min.x, aabb.max.y);
		tvDrawThickLine(aabb.min, tr, rad, col), tvFillCircle(aabb.min, rad, col);
		tvDrawThickLine(tr, aabb.max, rad, col), tvFillCircle(tr, rad, col);
		tvDrawThickLine(aabb.max, bl, rad, col), tvFillCircle(aabb.max, rad, col);
		tvDrawThickLine(bl, aabb.min, rad, col), tvFillCircle(bl, rad, col);
	}

	void tvFillCircle(const vf2d& pos, float rad, const olc::Pixel& col) {
		vf2d offset(rad, rad);
		tv.DrawDecal(pos-offset, prim_circ.Decal(), {2*rad/prim_circ.Sprite()->width, 2*rad/prim_circ.Sprite()->height}, col);
	}
	
	void tvDrawString(const vf2d& pos, const std::string& str, const olc::Pixel& col, float scale=1) {
		vf2d offset(4*str.length(), 4);
		tv.DrawStringDecal(pos-scale*offset, str, col, {scale, scale});
	}

	void renderGrid(float spacing, const olc::Pixel& col) {
		//screen bounds in world space, snap to nearest	
		vf2d tl=tv.GetWorldTL(), br=tv.GetWorldBR();
		int i_s=std::floor(tl.x/spacing), j_s=std::floor(tl.y/spacing);
		int i_e=std::ceil(br.x/spacing), j_e=std::ceil(br.y/spacing);

		//vert
		for(int i=i_s; i<=i_e; i++) {
			float x=spacing*i;
			vf2d top{x, tl.y}, btm{x, br.y};
			if(i%10==0) tvDrawThickLine(top, btm, .1f, col);
			else tv.DrawLineDecal(top, btm, col);
		}

		//horiz
		for(int j=j_s; j<=j_e; j++) {
			float y=spacing*j;
			vf2d lft{tl.x, y}, rgt{br.x, y};
			if(j%10==0) tvDrawThickLine(lft, rgt, .1f, col);
			else tv.DrawLineDecal(lft, rgt, col);
		}
	}

	//color based on if overlapping
	void renderShapeBounds(const Shape& shp) {
		cmn::AABB box=shp.getAABB();

		bool overlaps=false;
		for(const auto& s:scene.shapes) {
			if(s==&shp) continue;

			if(box.overlaps(s->getAABB())) {
				overlaps=true;
				break;
			}
		}

		olc::Pixel col=overlaps?olc::RED:olc::GREEN;
		tvDrawAABB(box, .075f, col);
	}

	void renderShapeAnchors(const Shape& shp) {
		//rotation matrix
		vf2d cosin=cmn::polar<vf2d>(1, shp.anchor_rot);
		auto rotate=[cosin] (const vf2d& p) {
			return vf2d(
				p.x*cosin.x-p.y*cosin.y,
				p.x*cosin.y+p.y*cosin.x
			);
		};

		//outlines
		for(int i=0; i<shp.getNum(); i++) {
			vf2d a=shp.anchor_pos+rotate(shp.anchors[i]);
			vf2d b=shp.anchor_pos+rotate(shp.anchors[(i+1)%shp.getNum()]);
			tvDrawThickLine(a, b, .05f, olc::GREY);
			tvFillCircle(a, .05f, olc::GREY);
		}
	}

	void renderShapeFill(const Shape& shp) {
		//initialize indexes
		std::unordered_map<PointMass*, int> idx;
		for(int i=0; i<shp.getNum(); i++) {
			idx[&shp.points[i]]=i;
		}
		std::list<int> indexes;
		for(const auto& c:shp.shell) {
			indexes.push_back(idx[c.a]);
		}

		//ear clipping triangulation
		for(auto curr=indexes.begin(); curr!=indexes.end();) {
			//get previous and next points
			auto prev=std::prev(curr==indexes.begin()?indexes.end():curr);
			auto next=std::next(curr); if(next==indexes.end()) next=indexes.begin();
			const auto& pt_p=shp.points[*prev].pos;
			const auto& pt_c=shp.points[*curr].pos;
			const auto& pt_n=shp.points[*next].pos;

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
				const auto& pt_o=shp.points[*other].pos;
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
				tv.DrawWarpedDecal(prim_rect.Decal(), {pt_p, pt_c, pt_n, pt_n}, shp.col);

				//remove this index and start over
				indexes.erase(curr);
				curr=indexes.begin();
			} else curr++;
		}
	}

	//show mass as string at centroid
	void renderShapeMass(const Shape& shp) {
		float mass=0;
		vf2d ctr=shp.getCOM();
		float r_sum=0;
		for(int i=0; i<shp.getNum(); i++) {
			r_sum+=(shp.points[i].pos-ctr).mag();
			mass+=shp.points[i].mass;
		}
		float rad=r_sum/shp.getNum(), scale=rad/8;
		std::string mass_str=std::to_string(int(mass));
		tvDrawString(ctr, mass_str, olc::DARK_GREY, scale/mass_str.length());
	}

	//show vertex as index string
	void renderShapeVertexes(const Shape& shp) {
		for(int i=0; i<shp.getNum(); i++) {
			vf2d pos=tv.WorldToScreen(shp.points[i].pos);
			auto str=std::to_string(i);
			vf2d size(2+8*str.size(), 10);
			FillRectDecal(pos-size/2, size, olc::WHITE);
			DrawStringDecal(pos-size/2+1, str, olc::BLACK);
		}
	}

	void renderCut() {
		//show cut path
		for(int i=1; i<cut.size(); i++) {
			tvDrawThickLine(cut[i-1], cut[i], .05f, olc::RED);
		}

		//show scissors
		if(cut.size()) {
			float angle=cmn::Pi+std::atan2(smooth_dy, smooth_dx);
			float diff=cut_anim?.017f:.352f;
			vf2d sz(cut_tex.Sprite()->width, cut_tex.Sprite()->height);
			vf2d ctr(.5047f, .1917f);
			float scl=40;
			DrawRotatedDecal(scr_mouse_pos, cut_tex.Decal(), angle-diff, ctr*sz, {scl/sz.x, -scl/sz.x}, olc::BLACK);
			DrawRotatedDecal(scr_mouse_pos, cut_tex.Decal(), angle+diff, ctr*sz, {scl/sz.x, scl/sz.x}, olc::BLACK);
		}
	}
#pragma endregion

	void render() {
		Clear(olc::Pixel(255, 254, 235));
		SetDecalMode(show_wireframes?olc::DecalMode::WIREFRAME:olc::DecalMode::NORMAL);

		if(show_grid) renderGrid(1, olc::Pixel(140, 222,255));

		//show phys bounds
		tvDrawAABB(scene.phys_bounds, .1f, olc::Pixel(255, 33, 122));

		//draw shapes
		for(const auto& shp:scene.shapes) {
			if(show_bounds) renderShapeBounds(*shp);

			if(shp->anchored) renderShapeAnchors(*shp);

			renderShapeFill(*shp);

			//constraint outlines
			for(const auto& c:shp->shell) {
				tvDrawThickLine(c.a->pos, c.b->pos, .1f, olc::BLACK);
				tvFillCircle(c.a->pos, .1f, olc::BLACK);
			}

			if(show_springs) {
				for(const auto& s:shp->springs) {
					tv.DrawLineDecal(s.a->pos, s.b->pos, olc::GREEN);
				}
			}

			if(show_mass) renderShapeMass(*shp);

			if(show_vertexes) renderShapeVertexes(*shp);
		}

		//show mouse springs
		for(const auto& m:mouse_springs) {
			tv.DrawLineDecal(m.a->pos, m.b->pos, olc::MAGENTA);
		}

		//show add rect
		if(rect_start) {
			cmn::AABB box;
			box.fitToEnclose(*rect_start);
			box.fitToEnclose(wld_mouse_pos);
			tvDrawAABB(box, .05f, olc::DARK_GREY);
		}

		renderCut();
	}

	bool OnUserUpdate(float dt) override {
		cmn::Stopwatch update_watch;
		update_watch.start();
		update(dt);
		update_watch.stop();

		cmn::Stopwatch render_watch;
		render_watch.start();
		render();
		render_watch.stop();
		
		if(to_time) {
			auto update_dur=update_watch.getMicros();
			auto render_dur=render_watch.getMicros();
			std::cout<<"  update: "<<update_dur<<"us ("<<(update_dur/1000.f)<<"ms)\n";
			std::cout<<"  render: "<<render_dur<<"us ("<<(render_dur/1000.f)<<"ms)\n";

			to_time=false;
		}

		return true;
	}

public:
	JellyCarGame() {
		sAppName="Jelly Car Clone";
	}
};


int main() {
	JellyCarGame jcg;
	bool vsync=true;
	if(jcg.Construct(640, 480, 1, 1, false, vsync)) jcg.Start();

	return 0;
}