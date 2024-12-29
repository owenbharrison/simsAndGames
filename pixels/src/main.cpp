/*todo:
verlet integration?
forces
	mass
	center of mass
torques
	moment of inertia
	rot_acc
slicing
	impl bresenhams
saving/loading
	.obj-esque parsing?
*/

#include "common/myPixelGameEngine.h"
using olc::vf2d;
using olc::vi2d;

#include "aabb.h"
#include "pixel_set.h"

std::ostream& operator<<(std::ostream& o, const PixelSet& ps) {
	for(int j=0; j<ps.getH(); j++) {
		for(int i=0; i<ps.getW(); i++) {
			//dont print empty
			const auto& p=ps(i, j);
			if(p!=PixelSet::Empty) o<<int(p);
			else o<<' ';

			o<<' ';
		}
		o<<'\n';
	}
	return o;
}

//clever default param placement:
//random()=0-1
//random(a)=0-a
//random(a, b)=a-b
float random(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=rand()/rand_max;
	return a+t*(b-a);
}

constexpr float Pi=3.1415927f;

//thanks wikipedia + pattern recognition
//NOTE: this returns t and u, NOT the point.
vf2d lineLineIntersection(
	const vf2d& a, const vf2d& b,
	const vf2d& c, const vf2d& d) {
	vf2d ab=a-b, ac=a-c, cd=c-d;
	return vf2d(
		ac.cross(cd),
		ac.cross(ab)
	)/ab.cross(cd);
}

//could make this faster with scanline rasterization...
PixelSet pixelsetFromOutline(const std::vector<vf2d>& outline) {
	//get bounds of outline
	AABB box;
	for(const auto& o:outline) box.fitToEnclose(o);

	//determine spacing
	vf2d size=box.max-box.min;
	const float scale=5;
	int w=1+size.x/scale, h=1+size.y/scale;

	//for every point
	PixelSet p(w, h);
	p.pos=box.min;
	p.scale=scale;
	for(int i=0; i<w; i++) {
		for(int j=0; j<h; j++) {
			//is it inside the outline
			vf2d pos=p.localToWorld(vf2d(i, j));

			//deterministic, but less artifacting
			float angle=random(2*Pi);
			vf2d dir(std::cosf(angle), std::sinf(angle));

			//polygon raycast algorithm
			int num=0;
			for(int k=0; k<outline.size(); k++) {
				const auto& a=outline[k];
				const auto& b=outline[(k+1)%outline.size()];
				vf2d tu=lineLineIntersection(a, b, pos, pos+dir);
				if(tu.x>0&&tu.x<1&&tu.y>0) num++;
			}

			//odd? inside!
			p(i, j)=num%2;
		}
	}

	//should this be encapsulated into a function?
	p.compress();
	p.updateTypes();
	p.updateMeshes();
	p.updateMass();

	return p;
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

	vf2d drag_start;
	PixelSet* drag_set=nullptr;

	std::vector<vf2d> addition;
	float addition_timer=0;

	//given screen bounds, place ALL pixelsets randomly
	//such that their bounds lie inside the screen
	void placeRandom() {
		for(const auto& p:pixelsets) {
			p->rot=random(2*Pi);

			//aabb needs cossin
			p->updateRot();
			AABB box=p->getAABB();

			//easier to just offset based on where it already is
			p->pos.x+=random(-box.min.x, ScreenWidth()-box.max.x);
			p->pos.y+=random(-box.min.y, ScreenHeight()-box.max.y);
		}
	}

	bool on_init() override {
		srand(time(0));

		gravity={0, 0};

#pragma region CONSTRUCTION
		{//make floor
			PixelSet* thing=new PixelSet(12, 3);
			for(int i=0; i<thing->getW()*thing->getH(); i++) {
				thing->grid[i]=PixelSet::Normal;
			}
			thing->updateTypes();
			thing->updateMeshes();
			thing->updateMass();

			thing->scale=20;

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
			thing->compress();
			thing->updateTypes();
			thing->updateMeshes();
			thing->updateMass();

			thing->scale=8;

			pixelsets.push_back(thing);
		}

		{//make triangle
			const int w=56, h=44;

			//rasterize it??
			olc::Sprite tri(w, h);
			SetDrawTarget(&tri);
			Clear(olc::BLACK);
			FillTriangle(w/2, 0, w-1, h-1, 0, h-1, olc::WHITE);
			SetDrawTarget(nullptr);

			//copy over data
			PixelSet* thing=new PixelSet(w, h);
			for(int i=0; i<w; i++) {
				for(int j=0; j<h; j++) {
					(*thing)(i, j)=tri.GetPixel(i, j).r>0;
				}
			}
			thing->compress();
			thing->updateTypes();
			thing->updateMeshes();
			thing->updateMass();

			thing->scale=5;

			pixelsets.push_back(thing);
		}
#pragma endregion

		placeRandom();

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
		prev_mouse_pos=mouse_pos;
		mouse_pos=GetMousePos();

#pragma region USER INPUT
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
				if(drag_set) drag_set->pos+=mouse_pos-drag_start;
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
					PixelSet* thing=new PixelSet(pixelsetFromOutline(addition));

					pixelsets.emplace_back(thing);

					std::cout<<"added "<<thing->getW()<<'x'<<thing->getH()<<" pixelset\n";
				}
				addition.clear();
			}

			//slice objects
			//THIS DOESNT ALWAYS WORK
			//implement bresenhams
			//clip segment against local space
			if(GetKey(olc::Key::S).bHeld) {
				AABB seg_box;
				seg_box.fitToEnclose(prev_mouse_pos);
				seg_box.fitToEnclose(mouse_pos);

				//check every pixelset
				for(const auto& p:pixelsets) {

					//is mouse in bounds?
					if(!seg_box.overlaps(p->getAABB())) continue;

					//convert segment to pixelset space
					vf2d s0=p->worldToLocal(prev_mouse_pos);
					vf2d s1=p->worldToLocal(mouse_pos);

					//check all blocks against it
					bool edited=false;
					for(int i=0; i<p->getW(); i++) {
						for(int j=0; j<p->getH(); j++) {
							//skip air
							if((*p)(i, j)==PixelSet::Empty) continue;

							//find corners of box
							vf2d v0(i, j), v3(1+i, 1+j);
							vf2d v1(1+i, j), v2(i, 1+j);

							//check each edge
							bool hit=false;
							for(int p=0; p<4; p++) {
								vf2d a, b;
								switch(p) {
									case 0: a=v0, b=v1; break;
									case 1: a=v1, b=v2; break;
									case 2: a=v2, b=v3; break;
									case 3: a=v3, b=v0; break;
								}
								vf2d tu=lineLineIntersection(a, b, s0, s1);
								if(tu.x>0&&tu.x<1&&tu.y>0&&tu.y<1) {
									hit=true;
									break;
								}
							}
							if(hit) {
								(*p)(i, j)=PixelSet::Empty;
								edited=true;
							}
						}
					}
					if(edited) {
						p->compress();
						p->updateTypes();
						p->updateMeshes();
						p->updateMass();
						std::cout<<"sliced pixelset\n";
					}
				}
			}

			//remove objects
			if(GetKey(olc::Key::DEL).bPressed) {
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
								std::cout<<"removed pixelset\n";

								continue;
							}
						}
					}

					it++;
				}
			}

			//randomize rotations
			if(GetKey(olc::Key::R).bPressed) placeRandom();

			//visual toggles with xor
			if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
			if(GetKey(olc::Key::W).bPressed) show_wireframes^=true;
			if(GetKey(olc::Key::G).bPressed) show_grids^=true;
		}

		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);
#pragma endregion

#pragma region PHYSICS
		//dynamics
		for(const auto& p:pixelsets) {
			p->applyForce(gravity);
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

		return true;
	}

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") {
			ConsoleClear();
		}

		if(cmd=="reset") {
			std::cout<<"removed "<<pixelsets.size()<<" pixelsets\n";
			reset();
		}

		if(cmd=="count") {
			int num=pixelsets.size();
			std::cout<<"there ";
			std::cout<<(num==1?"is ":"are ");
			std::cout<<num;
			std::cout<<" pixelset";
			if(num!=1) std::cout<<'s';
			std::cout<<'\n';
		}

		return true;
	}

	bool on_render() override {
		//draw grey background
		Clear(olc::GREY);
		SetDecalMode(show_wireframes?olc::DecalMode::WIREFRAME:olc::DecalMode::NORMAL);

		//show addition
		if(addition.size()) {
			vf2d prev;
			for(int i=0; i<addition.size(); i++) {
				const auto& a=addition[i];
				const auto& b=addition[(i+1)%addition.size()];
				DrawThickLine(a, b, 1, olc::CYAN);
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
			{
				vf2d moment_sum;
				float mass_sum=0;
				for(int i=0; i<p->getW(); i++) {
					for(int j=0; j<p->getH(); j++) {
						if((*p)(i, j)==PixelSet::Empty) continue;

						float mass=1;
						mass_sum+=mass;

						moment_sum+=mass*vf2d(.5f+i, .5f+j);
					}
				}
				vf2d ctr=moment_sum/mass_sum;
				vf2d pos=p->localToWorld(ctr);
				FillCircleDecal(pos, 5, olc::MAGENTA);
			}
#pragma endregion
		}

		return true;
	}
};

int main() {
	PixelGame t;
	if(t.Construct(800, 640, 1, 1, false, true)) t.Start();

	return 0;
}