/*todo:
saving/loading
	.obj-esque parsing
physics
	collisions
		SAT
	sounds?
graphics
	add transformed view
	add particles
	add textures
	add more debug views
*/

#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_TRANSFORMEDVIEW
#include "common/olcPGEX_TransformedView.h"

#include "pixel_set.h"

//debugging
std::ostream& operator<<(std::ostream& o, const PixelSet& p) {
	for(int j=0; j<p.getH(); j++) {
		for(int i=0; i<p.getW(); i++) {
			//dont print empty
			const auto& c=p(i, j);
			if(c!=PixelSet::Empty) o<<int(c);
			else o<<' ';

			o<<' ';
		}
		o<<'\n';
	}
	return o;
}

struct PixelGame : olc::PixelGameEngine {
	PixelGame() {
		sAppName="Testing";
	}

	vf2d scr_mouse_pos;
	vf2d wld_mouse_pos;
	vf2d prev_wld_mouse_pos;
	vf2d gravity;

	std::list<PixelSet*> pixelsets;

	bool show_bounds=false;
	bool show_wireframes=false;
	bool show_grids=false;
	bool show_mass=false;
	bool show_outlines=true;
	bool update_phys=true;

	bool to_time=false;

	vf2d drag_start;
	PixelSet* drag_set=nullptr;

	vf2d spring_offset;
	PixelSet* spring_set=nullptr;

	std::vector<vf2d> addition;
	float addition_timer=0;

	olc::Sprite* rect_sprite=nullptr;
	olc::Decal* rect_decal=nullptr;
	olc::Sprite* circle_sprite=nullptr;
	olc::Decal* circle_decal=nullptr;

	olc::TransformedView tv;

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

		gravity={0, 0};

#pragma region CONSTRUCTION
		{//make floor
			PixelSet* thing=new PixelSet(12, 3);
			for(int i=0; i<thing->getW()*thing->getH(); i++) {
				thing->grid[i]=true;
			}
			thing->updateTypes();
			thing->updateMeshes();
			thing->updateOutlines();

			thing->scale=20;
			thing->updateMass();
			thing->updateInertia();

			pixelsets.push_back(thing);
		}

		{//make ball
			const int sz=16;
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

			thing->scale=8;
			thing->updateMass();
			thing->updateInertia();

			pixelsets.push_back(thing);
		}

		{//make ngon
			float rad=random(120, 240);
			std::vector<vf2d> pts;
			for(int i=0; i<3; i++) {
				float angle=2*Pi*i/3;
				pts.emplace_back(polar(rad, angle));
			}

			PixelSet* thing=new PixelSet(PixelSet::fromPolygon(pts, 5));
			thing->updateTypes();
			thing->updateMeshes();
			thing->updateOutlines();

			thing->updateMass();
			thing->updateInertia();

			pixelsets.push_back(thing);
		}
#pragma endregion

		placeAllRandomly();

		ConsoleCaptureStdOut(true);

		//make some primitives to draw with
		rect_sprite=new olc::Sprite(1, 1);
		rect_sprite->SetPixel(0, 0, olc::WHITE);
		rect_decal=new olc::Decal(rect_sprite);

		int circle_width=1024;
		const unsigned int rad_sq=circle_width*circle_width/4;
		circle_sprite=new olc::Sprite(circle_width, circle_width);
		for(int x=0; x<circle_width; x++) {
			int dx=x-circle_width/2;
			for(int y=0; y<circle_width; y++) {
				int dy=y-circle_width/2;
				circle_sprite->SetPixel(x, y, dx*dx+dy*dy<rad_sq?olc::WHITE:olc::BLANK);
			}
		}
		circle_decal=new olc::Decal(circle_sprite);

		tv.Initialise(GetScreenSize());
		tv.SetScaleExtents({.5f, .5f}, {100, 100});
		tv.EnableScaleClamp(true);

		return true;
	}

	void reset() {
		spring_set=nullptr;
		drag_set=nullptr;

		for(const auto& p:pixelsets) delete p;
		pixelsets.clear();
	}

	bool OnUserDestroy() override {
		reset();

		delete rect_decal;
		delete rect_sprite;

		delete circle_decal;
		delete circle_sprite;

		return true;
	}

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") ConsoleClear();

		else if(cmd=="reset") {
			std::cout<<"removed "<<pixelsets.size()<<" pixelsets\n";
			reset();
		}

		else if(cmd=="count") {
			int num=pixelsets.size();
			std::cout<<"there ";
			std::cout<<(num==1?"is ":"are ");
			std::cout<<num;
			std::cout<<" pixelset";
			if(num!=1) std::cout<<'s';
			std::cout<<'\n';
		}

		else if(cmd=="usage") {
			if(pixelsets.empty()) std::cout<<"n/a\n";
			else {
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
			}
		}

		else if(cmd=="time") to_time=true;

		else std::cout<<"unknown command.\n";

		return true;
	}

#pragma region HELPERS
	void DrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float rad, olc::Pixel col=olc::WHITE) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=(sub/len).perp();

		float angle=atan2f(sub.y, sub.x);
		DrawRotatedDecal(a-rad*tang, rect_decal, angle, {0, 0}, {len, 2*rad}, col);
	}

	void tvDrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float rad, olc::Pixel col=olc::WHITE) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=(sub/len).perp();

		float angle=atan2f(sub.y, sub.x);
		tv.DrawRotatedDecal(a-rad*tang, rect_decal, angle, {0, 0}, {len, 2*rad}, col);
	}

	void tvFillCircleDecal(const olc::vf2d& pos, float rad, olc::Pixel col=olc::WHITE) {
		olc::vf2d offset(rad, rad);
		olc::vf2d scale{2*rad/circle_sprite->width, 2*rad/circle_sprite->width};
		tv.DrawDecal(pos-offset, circle_decal, scale, col);
	}
#pragma endregion

	bool OnUserUpdate(float dt) override {
#pragma region UPDATE
		auto physics_start=std::chrono::steady_clock::now();

		prev_wld_mouse_pos=wld_mouse_pos;
		scr_mouse_pos=GetMousePos();
		wld_mouse_pos=tv.ScreenToWorld(scr_mouse_pos);

#pragma region USER INPUT
		//only allow input when console NOT open
		if(!IsConsoleShowing()) {
			//zooming
			if(GetMouseWheel()>0) tv.ZoomAtScreenPos(1.07f, scr_mouse_pos);
			if(GetMouseWheel()<0) tv.ZoomAtScreenPos(1/1.07f, scr_mouse_pos);

			//panning
			if(GetMouse(olc::Mouse::MIDDLE).bPressed) tv.StartPan(scr_mouse_pos);
			if(GetMouse(olc::Mouse::MIDDLE).bHeld) tv.UpdatePan(scr_mouse_pos);
			if(GetMouse(olc::Mouse::MIDDLE).bReleased) tv.EndPan(scr_mouse_pos);
			
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
				if(addition.size()>=3) {
					float resolution=random(2, 15);
					PixelSet* thing=new PixelSet(PixelSet::fromPolygon(addition, resolution));

					pixelsets.emplace_back(thing);
				}
				addition.clear();
			}

			//slice objects
			const auto s_key=GetKey(olc::Key::S);
			if(s_key.bHeld) {
				drag_set=nullptr;
				spring_set=nullptr;

				AABB seg_box;
				seg_box.fitToEnclose(prev_wld_mouse_pos);
				seg_box.fitToEnclose(wld_mouse_pos);

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
				drag_set=nullptr;
				spring_set=nullptr;

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
		}

		//open and close the integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		//close application
		if(GetKey(olc::Key::END).bPressed) return false;
#pragma endregion

#pragma region PHYSICS
		if(update_phys) {
			//dynamics
			for(const auto& p:pixelsets) {
				p->applyForce(p->total_mass*gravity, p->localToWorld(p->center_of_mass));
				p->update(dt);
			}

			//clear colliding displays
			for(const auto& p:pixelsets) {
				memset(p->colliding, false, sizeof(bool)*p->getW()*p->getH());
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

		if(to_time) {
			auto end=std::chrono::steady_clock::now();
			auto dur=std::chrono::duration_cast<std::chrono::microseconds>(end-physics_start);
			auto us=dur.count();
			auto ms=us/1000.f;
			std::cout<<"update: "<<us<<"us ("<<ms<<"ms)\n";
		}
#pragma endregion

#pragma region RENDER
		auto render_start=std::chrono::steady_clock::now();

		//draw grey background
		Clear(olc::Pixel(0, 100, 255));
		SetDecalMode(show_wireframes?olc::DecalMode::WIREFRAME:olc::DecalMode::NORMAL);

		if(true) {
			const auto grid_spacing=10;

			//screen bounds in world space, snap to nearest
			vf2d tl=tv.GetWorldTL(), br=tv.GetWorldBR();
			int i_s=std::floor(tl.x/grid_spacing), j_s=std::floor(tl.y/grid_spacing);
			int i_e=std::ceil(br.x/grid_spacing), j_e=std::ceil(br.y/grid_spacing);

			//vert
			for(int i=i_s; i<=i_e; i++) {
				float x=grid_spacing*i;
				vf2d top{x, tl.y}, btm{x, br.y};
				if(i%10==0) tvDrawThickLine(top, btm, 1, olc::GREY);
				else tv.DrawLineDecal(top, btm, olc::GREY);
			}

			//horiz
			for(int j=j_s; j<=j_e; j++) {
				float y=grid_spacing*j;
				vf2d lft{tl.x, y}, rgt{br.x, y};
				if(j%10==0) tvDrawThickLine(lft, rgt, 1, olc::GREY);
				else tv.DrawLineDecal(lft, rgt, olc::GREY);
			}
		}

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
			//draw rectangle bounding box
			if(show_bounds) {
				//color if it overlaps
				AABB box=p->getAABB();
				olc::Pixel col=olc::GREEN;
				for(const auto& o:pixelsets) {
					//dont check self
					if(o==p) continue;

					if(box.overlaps(o->getAABB())) {
						col=olc::RED;
						break;
					}
				}

				vf2d v1(box.max.x, box.min.y);
				vf2d v2(box.min.x, box.max.y);
				tvDrawThickLine(box.min, v1, 1, col);
				tvDrawThickLine(v1, box.max, 1, col);
				tvDrawThickLine(box.max, v2, 1, col);
				tvDrawThickLine(v2, box.min, 1, col);
			}

			//show local grid
			if(show_grids) {
				//draw "vertical" ticks
				for(int i=0; i<=p->getW(); i++) {
					tv.DrawLineDecal(p->localToWorld(vf2d(i, 0)), p->localToWorld(vf2d(i, p->getH())), olc::DARK_GREY);
				}

				//draw "horizontal" ticks
				for(int j=0; j<=p->getH(); j++) {
					tv.DrawLineDecal(p->localToWorld(vf2d(0, j)), p->localToWorld(vf2d(p->getW(), j)), olc::DARK_GREY);
				}

				//draw axes
				tvDrawThickLine(p->localToWorld(vf2d(-1, 0)), p->localToWorld(vf2d(p->getW()+1, 0)), 1, olc::BLACK);
				tvDrawThickLine(p->localToWorld(vf2d(0, -1)), p->localToWorld(vf2d(0, p->getH()+1)), 1, olc::BLACK);
			}

			//render meshes
			for(const auto& m:p->meshes) {
				vf2d pos=p->localToWorld(vf2d(m.i, m.j));
				vf2d size(p->scale*m.w, p->scale*m.h);
				tv.DrawRotatedDecal(pos, rect_decal, p->rot, {0, 0}, size, m.col);
			}

			//render outlines
			if(show_outlines) {
				const float amt=.1f;
				for(const auto& o:p->outlines) {
					vf2d st(o.i, o.j), en=st;
					if(o.vert) st.y-=amt, en.y+=amt+o.sz;
					else st.x-=amt, en.x+=amt+o.sz;
					tvDrawThickLine(p->localToWorld(st), p->localToWorld(en), amt*p->scale, o.col);
				}
			}

#pragma region EXPERIMENTAL
			//render colliding pixels
			for(int i=0; i<p->getW(); i++) {
				for(int j=0; j<p->getH(); j++) {
					if(!p->colliding[p->ix(i, j)]) continue;

					vf2d pos=p->localToWorld(vf2d(i, j));
					vf2d size(p->scale, p->scale);
					tv.DrawRotatedDecal(pos, rect_decal, p->rot, {0, 0}, size, olc::RED);
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
						tv.DrawRotatedDecal(pos, rect_decal, p->rot, {0, 0}, size, olc::GREEN);
					}
				}
			}

			//show center of mass
			if(show_mass) {
				vf2d pos=p->localToWorld(p->center_of_mass);
				tvFillCircleDecal(pos, .5f*p->scale, olc::MAGENTA);

				//show mass string
				auto mass_str=std::to_string(int(p->total_mass));
				tv.DrawStringDecal(pos, mass_str, olc::BLACK);

				//show inertia string
				auto inertia_str=std::to_string(int(p->moment_of_inertia));
				tv.DrawStringDecal(pos+vf2d(0, 10), inertia_str, olc::BLACK);
			}
#pragma endregion
		}

		//show spring set
		if(spring_set) {
			vf2d spr_pos=spring_set->localToWorld(spring_offset);
			tvDrawThickLine(wld_mouse_pos, spr_pos, 1, olc::YELLOW);
			tvFillCircleDecal(spr_pos, .5f*spring_set->scale, olc::YELLOW);
		}

		if(!update_phys) {
			vf2d tl(0, 0), br=GetScreenSize();
			vf2d tr(br.x, 0), bl(0, br.y);
			DrawThickLine(tl, tr, 3, olc::RED);
			DrawThickLine(tr, br, 3, olc::RED);
			DrawThickLine(br, bl, 3, olc::RED);
			DrawThickLine(bl, tl, 3, olc::RED);
		}

		if(to_time) {
			auto end=std::chrono::steady_clock::now();
			auto dur=std::chrono::duration_cast<std::chrono::microseconds>(end-render_start);
			auto us=dur.count();
			auto ms=us/1000.f;
			std::cout<<"render: "<<us<<"us ("<<ms<<"ms)\n";

			to_time=false;
		}
#pragma endregion

		return true;
	}
};

int main() {
	PixelGame t;
	if(t.Construct(800, 640, 1, 1, false, true)) t.Start();

	return 0;
}