/*todo:
saving/loading
	.obj-esque parsing
physics
	collisions
		SAT
	sounds?
graphics
	add outlines
	add transformed view
	add particles
	add textures
	add more debug views
*/

#include "common/myPixelGameEngine.h"
using olc::vf2d;
using olc::vi2d;

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

struct PixelGame : MyPixelGameEngine {
	PixelGame() {
		sAppName="Testing";
	}

	vf2d mouse_pos;
	vf2d prev_mouse_pos;
	vf2d gravity;

	std::list<PixelSet*> pixelsets;

	bool show_bounds=false;
	bool show_wireframes=false;
	bool show_grids=false;
	bool show_mass=false;
	bool show_outlines=true;

	bool to_time=false;

	vf2d drag_start;
	PixelSet* drag_set=nullptr;

	std::vector<vf2d> addition;
	float addition_timer=0;

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

	bool on_init() override {
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

		return true;
	}

	void reset() {
		for(const auto& p:pixelsets) delete p;
		pixelsets.clear();
	}

	bool on_exit() override {
		reset();

		return true;
	}

	bool on_update(float dt) override {
		auto timer_start=std::chrono::steady_clock::now();
		
		prev_mouse_pos=mouse_pos;
		mouse_pos=GetMousePos();

#pragma region USER INPUT
		//only allow input when console NOT open
		if(!IsConsoleShowing()) {
			//mouse grabbing
			const auto left_mouse=GetMouse(olc::Mouse::LEFT);
			if(left_mouse.bPressed) {
				drag_start=mouse_pos;
				//which dragset is the mouse on?
				drag_set=nullptr;
				for(const auto& p:pixelsets) {
					//is mouse in bounds?
					if(!p->getAABB().contains(mouse_pos)) continue;

					//are local coords valid?
					vf2d ij=p->worldToLocal(mouse_pos);
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
					drag_set->pos+=mouse_pos-drag_start;
					//reset vel?
					drag_set->old_pos=drag_set->pos;
				}
				drag_start=mouse_pos;
			}
			if(left_mouse.bReleased) {
				//let go of that sucker
				drag_set=nullptr;
			}

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
						if((a-mouse_pos).mag()<1) {
							exist=true;
							break;
						}
					}
					//add point to addition
					if(!exist) addition.emplace_back(mouse_pos);
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
				AABB seg_box;
				seg_box.fitToEnclose(prev_mouse_pos);
				seg_box.fitToEnclose(mouse_pos);

				//check every pixelset
				for(auto it=pixelsets.begin(); it!=pixelsets.end();) {
					const auto& p=*it;
					if(p->slice(prev_mouse_pos, mouse_pos)) {
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
				for(auto it=pixelsets.begin(); it!=pixelsets.end();) {

					//is mouse in bounds?
					const auto& p=*it;
					if(p->getAABB().contains(mouse_pos)) {

						//are local coords valid?
						vf2d ij=p->worldToLocal(mouse_pos);
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
		}

		//open and close the integrated console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);
#pragma endregion

#pragma region PHYSICS
		//dynamics
		for(const auto& p:pixelsets) {
			p->applyForce(p->total_mass*gravity, p->localToWorld(p->center_of_mass));
			p->update(dt);
		}

		//clear collide display
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
#pragma endregion

		if(to_time) {
			auto timer_end=std::chrono::steady_clock::now();
			auto dur=std::chrono::duration_cast<std::chrono::microseconds>(timer_end-timer_start);
			std::cout<<"update: "<<dur.count()<<"us\n";
		}

		return true;
	}

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") {
			ConsoleClear();
		}

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

		else if(cmd=="time") to_time=true;

		else {
			std::cout<<"unknown command.\n";
			return false;
		}

		return true;
	}

	bool on_render() override {
		auto timer_start=std::chrono::steady_clock::now();

		//draw grey background
		Clear(olc::WHITE);
		SetDecalMode(show_wireframes?olc::DecalMode::WIREFRAME:olc::DecalMode::NORMAL);

		//show addition
		if(addition.size()) {
			vf2d prev;
			for(int i=0; i<addition.size(); i++) {
				const auto& a=addition[i];
				const auto& b=addition[(i+1)%addition.size()];
				DrawThickLine(a, b, 1, olc::BLUE);
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
				DrawThickLine(box.min, v1, 1, col);
				DrawThickLine(v1, box.max, 1, col);
				DrawThickLine(box.max, v2, 1, col);
				DrawThickLine(v2, box.min, 1, col);
			}

			//show local grid
			if(show_grids) {
				//draw "vertical" ticks
				for(int i=0; i<=p->getW(); i++) {
					DrawLineDecal(p->localToWorld(vf2d(i, 0)), p->localToWorld(vf2d(i, p->getH())), olc::DARK_GREY);
				}

				//draw "horizontal" ticks
				for(int j=0; j<=p->getH(); j++) {
					DrawLineDecal(p->localToWorld(vf2d(0, j)), p->localToWorld(vf2d(p->getW(), j)), olc::DARK_GREY);
				}

				//draw axes
				DrawThickLine(p->localToWorld(vf2d(-1, 0)), p->localToWorld(vf2d(p->getW()+1, 0)), 1, olc::BLACK);
				DrawThickLine(p->localToWorld(vf2d(0, -1)), p->localToWorld(vf2d(0, p->getH()+1)), 1, olc::BLACK);
			}

			//render meshes
			for(const auto& m:p->meshes) {
				vf2d pos=p->localToWorld(vf2d(m.i, m.j));
				vf2d size(p->scale*m.w, p->scale*m.h);
				DrawRotatedDecal(pos, getRectDecal(), p->rot, {0, 0}, size, m.col);
			}

			//render outlines
			if(show_outlines) for(const auto& o:p->outlines) {
				vf2d st(o.i, o.j), en=st;
				if(o.vert) en.y+=o.sz;
				else en.x+=o.sz;
				DrawThickLine(p->localToWorld(st), p->localToWorld(en), 1, o.col);
			}

#pragma region EXPERIMENTAL
			//render colliding pixels
			for(int i=0; i<p->getW(); i++) {
				for(int j=0; j<p->getH(); j++) {
					if(!p->colliding[p->ix(i, j)]) continue;

					vf2d pos=p->localToWorld(vf2d(i, j));
					vf2d size(p->scale, p->scale);
					DrawRotatedDecal(pos, getRectDecal(), p->rot, {0, 0}, size, olc::RED);
				}
			}

			//show hover block
			//is mouse in bounds?
			if(p->getAABB().contains(mouse_pos)) {
				//are local coords valid?
				vf2d ij=p->worldToLocal(mouse_pos);
				int i=std::floor(ij.x), j=std::floor(ij.y);
				if(p->inRangeX(i)&&p->inRangeY(j)) {
					//is the block solid?
					if((*p)(i, j)!=PixelSet::Empty) {
						vf2d pos=p->localToWorld(vf2d(i, j));
						vf2d size(p->scale, p->scale);
						DrawRotatedDecal(pos, getRectDecal(), p->rot, {0, 0}, size, olc::GREEN);
					}
				}
			}

			//show center of mass
			if(show_mass) {
				vf2d pos=p->localToWorld(p->center_of_mass);
				FillCircleDecal(pos, p->scale, olc::MAGENTA);

				//show mass string
				auto mass_str=std::to_string(int(p->total_mass));
				DrawStringDecal(pos, mass_str, olc::BLACK);

				//show inertia string
				auto inertia_str=std::to_string(int(p->moment_of_inertia));
				DrawStringDecal(pos+vf2d(0, 10), inertia_str, olc::BLACK);
			}
#pragma endregion
		}

		if(to_time) {
			auto timer_end=std::chrono::steady_clock::now();
			auto dur=std::chrono::duration_cast<std::chrono::microseconds>(timer_end-timer_start);
			std::cout<<"render: "<<dur.count()<<"us\n";

			to_time=false;
		}

		return true;
	}
};

int main() {
	PixelGame t;
	if(t.Construct(800, 640, 1, 1, false, false)) t.Start();

	return 0;
}