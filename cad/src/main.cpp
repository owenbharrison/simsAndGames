//add delete option
#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "point_constraint.h"

#include "geom_constraint.h"

//thanks wikipedia + pattern recognition
vf2d lineLineIntersection(
	const vf2d& a, const vf2d& b,
	const vf2d& c, const vf2d& d) {
	//get segments
	vf2d ab=a-b, ac=a-c, cd=c-d;
	
	//parallel
	float denom=ab.cross(cd);
	if(std::abs(denom)<Epsilon) return {-1, -1};

	//find interpolators
	return vf2d(
		ac.cross(cd),
		ac.cross(ab)
	)/denom;
}

float clamp(float x, float a, float b) {
	if(x<a) return a;
	if(x>b) return b;
	return x;
}

#include "aabb.h"

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Example";
	}

	//user input
	vf2d mouse_pos;

	//points to work with
	const float rad=2;
	const float select_rad=2*rad;
	vf2d* drag_pt=nullptr;
	std::list<vf2d*> points;
	std::list<vf2d*> ref_pts;
	void selectPoint(vf2d* pt) {
		bool unique=true;
		for(auto it=ref_pts.begin(); it!=ref_pts.end(); it++) {
			if(*it==pt) {
				//unselect
				ref_pts.erase(it);

				unique=false;
				break;
			}
		}
		//add if new
		if(unique) ref_pts.emplace_back(pt);
	}
	void removePoint(vf2d* pt) {
		auto ait=std::find(points.begin(), points.end(), pt);
		//does it exist?
		if(ait!=points.end()) {
			//find corresponding point constraints
			for(auto bit=point_constraints.begin(); bit!=point_constraints.end(); ) {
				const auto& pc=*bit;
				//is it connected?
				if(pc->a==pt||pc->b==pt) {
					//find corresponding geom constraints
					for(auto cit=geom_constraints.begin(); cit!=geom_constraints.end(); ) {
						const auto& gc=*cit;
						//is it connected?
						if(gc->a==pc||gc->b==pc) {
							//delete geom constraint
							delete gc;
							cit=geom_constraints.erase(cit);
						} else cit++;
					}
					//delete point constraint
					delete pc;
					bit=point_constraints.erase(bit);
				} else bit++;
			}
			//delete point
			delete pt;
			points.erase(ait);
		}
	}

	//point selection, point constraints
	std::list<PointConstraint*> point_constraints;
	std::list<LineConstraint*> ref_lines;
	bool unique(PointConstraint* cst) const {
		for(const auto& c:point_constraints) {
			if(*c==*cst) return false;
		}
		return true;
	}
	void selectLine(LineConstraint* line) {
		bool unique=true;
		for(auto it=ref_lines.begin(); it!=ref_lines.end(); it++) {
			if(*it==line) {
				//unselect
				ref_lines.erase(it);

				unique=false;
				break;
			}
		}
		//add if new
		if(unique) ref_lines.emplace_back(line);
	}
	void removePointConstraint(PointConstraint* pc) {
		auto ait=std::find(point_constraints.begin(), point_constraints.end(), pc);
		//does it exist?
		if(ait!=point_constraints.end()) {
			//find corresponding geom constraints
			for(auto bit=geom_constraints.begin(); bit!=geom_constraints.end(); ) {
				const auto& gc=*bit;
				//is it connected?
				if(gc->a==pc||gc->b==pc) {
					//delete geom constraint	
					delete gc;
					bit=geom_constraints.erase(bit);
				} else bit++;
			}
			//delete point constraint
			delete pc;
			point_constraints.erase(ait);
		}
	}

	//snipping feature
	//only thru line constraints?
	std::list<vf2d> snip;
	float snip_timer=0;
	
	//line selection, line constraints
	std::list<GeometryConstraint*> geom_constraints;
	bool unique(GeometryConstraint* cst) const {
		for(const auto& c:geom_constraints) {
			if(*c==*cst) return false;
		}
		return true;
	}
	void removeGeomConstraint(GeometryConstraint* gc) {
		auto ait=std::find(geom_constraints.begin(), geom_constraints.end(), gc);
		//does it exist?
		if(ait!=geom_constraints.end()) {
			//delete geom constraint
			delete gc;
			geom_constraints.erase(ait);
		}
	}

	vf2d* select_box=nullptr;

	bool OnUserCreate() override {

		return true;
	}

#pragma region RENDER HELPERS
	void DrawArc(vf2d ctr, float rad, float start, float size, olc::Pixel col=olc::WHITE) {
		const int num=24;
		float angle=start, step=size/num;
		vf2d prev;
		for(int i=0; i<num; i++) {
			vf2d curr=ctr+rad*vf2d(std::cosf(angle), std::sinf(angle));
			if(i!=0) DrawLine(prev, curr, col);
			prev=curr;
			angle+=step;
		}
	}
#pragma endregion

	float total_dt=0;
	bool OnUserUpdate(float dt) override {
		mouse_pos=GetMousePos();

#pragma region ADDITION
		if(GetKey(olc::Key::A).bPressed) points.emplace_back(new vf2d(mouse_pos));

		//add vertical constraint
		if(GetKey(olc::Key::V).bPressed) {
			for(auto ait=ref_pts.begin(); ait!=ref_pts.end(); ait++) {
				for(auto bit=std::next(ait); bit!=ref_pts.end(); bit++) {
					PointConstraint* vert=new VerticalConstraint(*ait, *bit);
					if(unique(vert)) {
						point_constraints.emplace_back(vert);
					} else delete vert;
				}
			}
		}

		//add horizontal constraint
		if(GetKey(olc::Key::H).bPressed) {
			for(auto ait=ref_pts.begin(); ait!=ref_pts.end(); ait++) {
				for(auto bit=std::next(ait); bit!=ref_pts.end(); bit++) {
					PointConstraint* horiz=new HorizontalConstraint(*ait, *bit);
					if(unique(horiz)) {
						point_constraints.emplace_back(horiz);
					} else delete horiz;
				}
			}
		}

		//add line
		if(GetKey(olc::Key::L).bPressed) {
			//make all possible connections
			for(auto ait=ref_pts.begin(); ait!=ref_pts.end(); ait++) {
				for(auto bit=std::next(ait); bit!=ref_pts.end(); bit++) {
					PointConstraint* line=new LineConstraint(*ait, *bit);
					if(unique(line)) {
						point_constraints.emplace_back(line);
					} else delete line;
				}
			}
		}

		//add equal constraint
		if(GetKey(olc::Key::E).bPressed) {
			for(auto ait=ref_lines.begin(); ait!=ref_lines.end(); ait++) {
				for(auto bit=std::next(ait); bit!=ref_lines.end(); bit++) {
					GeometryConstraint* equal=new EqualConstraint(*ait, *bit);
					if(unique(equal)) {
						geom_constraints.emplace_back(equal);
					} else delete equal;
				}
			}
		}

		//add angle constraint
		if(GetKey(olc::Key::D).bPressed) {
			int num=ref_lines.size();
			if(num==2) {
				GeometryConstraint* angle=new AngleConstraint(ref_lines.front(), ref_lines.back());
				if(unique(angle)) {
					geom_constraints.emplace_back(angle);
				} else delete angle;
			}
		}
#pragma endregion

#pragma region SELECTION
		//set drag pt to point under mouse
		const auto drag_action=GetMouse(olc::Mouse::LEFT);
		if(drag_action.bPressed) {
			drag_pt=nullptr;
			for(const auto& p:points) {
				if((*p-mouse_pos).mag()<select_rad) {
					drag_pt=p;
					break;
				}
			}
		}
		//set to mouse on drag
		if(drag_action.bHeld) {
			if(drag_pt) {
				*drag_pt=mouse_pos;
				//update all lines
				for(const auto& c:point_constraints) {
					if(c->getID()!=LINE_CST) continue;

					if(drag_pt==c->a||drag_pt==c->b) {
						dynamic_cast<LineConstraint*>(c)->update();
					}
				}
				//update all angles
				for(const auto& g:geom_constraints) {
					if(g->getID()!=ANGLE_CST) continue;

					if(g->a->a==drag_pt||g->a->b==drag_pt||
						g->b->a==drag_pt||g->b->b==drag_pt) {
						dynamic_cast<AngleConstraint*>(g)->update();
					}
				}
			}
		}
		//release
		if(drag_action.bReleased) drag_pt=nullptr;

		const auto select_action=GetMouse(olc::Mouse::RIGHT);
		if(select_action.bPressed) {
			bool pt_selected=false;
			for(const auto& p:points) {
				if((*p-mouse_pos).mag()<select_rad) {
					pt_selected=true;

					selectPoint(p);
				}
			}
			//ensure no selection overlap	
			if(!pt_selected) {
				bool line_selected=false;
				for(const auto& c:point_constraints) {
					if(c->getID()!=LINE_CST) continue;

					//find closest point on segment
					vf2d ba=*c->b-*c->a, pa=mouse_pos-*c->a;
					float t=clamp(pa.dot(ba)/ba.dot(ba), 0, 1);
					vf2d close_pt=*c->a+t*ba;

					//if t close .5 -> store t
					//if a pressed, add point + coincedent

					//is it being hovered over?
					if((close_pt-mouse_pos).mag()<select_rad) {
						line_selected=true;

						selectLine(dynamic_cast<LineConstraint*>(c));
					}
				}

				//if nothing selected, start select box
				if(!line_selected) {
					select_box=new vf2d(mouse_pos);
				}
			}
		}
		if(select_action.bReleased) {
			if(select_box) {
				//find selection bounds
				AABB box;
				box.fitToEnclose(*select_box);
				box.fitToEnclose(mouse_pos);
				
				//which points are inside?
				for(const auto& p:points) {
					if(box.contains(*p)) selectPoint(p);
				}
				
				//which lines are inside?
				for(const auto& c:point_constraints) {
					if(c->getID()!=LINE_CST) continue;
					if(box.contains(*c->a)&&box.contains(*c->b)) {
						selectLine(dynamic_cast<LineConstraint*>(c));
					}
				}

				delete select_box;
				select_box=nullptr;
			}
		}

		//clear selection
		if(GetKey(olc::Key::ESCAPE).bPressed) {
			ref_pts.clear();
			ref_lines.clear();

			//exit out early?
			snip.clear();
		}
#pragma endregion

#pragma region REMOVAL
		if(GetKey(olc::Key::DEL).bPressed) {
			for(const auto& l:ref_lines) {
				removePointConstraint(l);
			}
			ref_lines.clear();
			for(const auto& p:ref_pts) {
				removePoint(p);
			}
			ref_pts.clear();
		}

		//snipping deprecated for now
#if 0
		const auto snip_action=GetKey(olc::Key::S);
		if(snip_action.bPressed) {
			snip.clear();
			drag_pt=nullptr;
			//clear selection
			ref_pts.clear();
			ref_lines.clear();
		}
		//every now and then,
		if(snip_timer<0) {
			snip_timer=0;

			//if snip down
			if(snip_action.bHeld) {
				bool unique=true;
				for(const auto& s:snip) {
					if((s-mouse_pos).mag()<rad) {
						unique=false;
						break;
					}
				}
				//add point if far enough away from rest
				if(unique) snip.emplace_back(mouse_pos);
			}
		}
		snip_timer-=dt;
		if(snip_action.bReleased) {
			for(auto cit=point_constraints.begin(); cit!=point_constraints.end();) {
				const auto& c=*cit;

				bool is_first=true, to_snip=false;
				vf2d first, prev;
				for(const auto& s:snip) {
					if(is_first) is_first=false;
					else {
						vf2d tu=lineLineIntersection(prev, s, *c->a, *c->b);
						if(tu.x>=0&&tu.x<=1&&tu.y>=0&&tu.y<=1) {
							to_snip=true;
							break;
						}
					}
					prev=s;
				}
				if(to_snip) {
					//remove all corresponding geometry constraints
					for(auto git=geom_constraints.begin(); git!=geom_constraints.end();) {
						const auto& g=*git;
						if(c==g->a||c==g->b) {
							delete g;
							git=geom_constraints.erase(git);
						} else git++;
					}

					delete c;
					cit=point_constraints.erase(cit);
				} else cit++;
			}

			snip.clear();
		}
#endif
#pragma endregion

#pragma region UPDATE
		//solve all constraints
		for(int i=0; i<200; i++) {
			//dist constraints...
			for(const auto& c:geom_constraints) c->solve();
			for(const auto& c:point_constraints) c->solve();
		}
#pragma endregion

#pragma region RENDER
		const olc::Pixel orange(255, 100, 0);
		Clear(olc::Pixel(200, 200, 200));

		//draw select box
		if(select_box) {
			AABB box;
			box.fitToEnclose(*select_box);
			box.fitToEnclose(mouse_pos);
			vf2d tr{box.max.x, box.min.y}, bl{box.min.x, box.max.y};
			DrawLine(box.min, tr, orange);
			DrawLine(tr, box.max, orange);
			DrawLine(box.max, bl, orange);
			DrawLine(bl, box.min, orange);
		}

		//draw all point constraints
		//change to draggable boxes.
		for(const auto& c:point_constraints) {
			olc::Pixel col;
			switch(c->getID()) {
				case VERT_CST: col=olc::MAGENTA; break;
				case HORIZ_CST: col=olc::CYAN; break;
				case LINE_CST: col=olc::YELLOW; break;
			}
			DrawLine(*c->a, *c->b, col);
		}

		//draw all line constraints
		for(const auto& g:geom_constraints) {
			switch(g->getID()) {
				case ANGLE_CST:
					//get points
					vf2d a=*g->a->a, b=*g->a->b;
					vf2d c=*g->b->a, d=*g->b->b;
					//get segment intersect
					vf2d tu=lineLineIntersection(a, b, c, d);
					vf2d ctr=a+tu.x*(b-a);
					//average dist to center
					float rad=((a-ctr).mag()+(b-ctr).mag()+(c-ctr).mag()+(d-ctr).mag())/4;
					DrawCircle(ctr, rad, olc::BLACK);
					//float start=std::atan2f(a.y-ctr.y, a.x-ctr.x);
					//float size=dynamic_cast<AngleConstraint*>(g)->angle;
					//DrawArc(ctr, rad, start, size, olc::BLACK);
					break;
			}
		}

		//draw selected lines
		for(const auto& c:ref_lines) {
			DrawLine(*c->a, *c->b, orange);
		}

		//render snip
		{
			bool is_first=true;
			vf2d first, prev;
			for(const auto& s:snip) {
				if(is_first) is_first=false;
				else {
					DrawLine(prev, s, orange);

					//render snip pts
					for(const auto& c:point_constraints) {
						vf2d tu=lineLineIntersection(prev, s, *c->a, *c->b);
						if(tu.x>=0&&tu.x<=1&&tu.y>=0&&tu.y<=1) {
							vf2d ix=prev+tu.x*(s-prev);
							FillCircle(ix, rad, olc::Pixel(200, 0, 0));
						}
					}
				}
				prev=s;
			}
		}

		//draw points
		for(const auto& p:points) {
			FillCircle(*p, rad, olc::BLACK);
		}

		//draw selected points
		for(const auto& r:ref_pts) {
			DrawCircle(*r, rad, orange);
		}
#pragma endregion

		return true;
	}
};

int main() {
	Example demo;
	if(demo.Construct(300, 300, 2, 2, false, true)) demo.Start();

	return 0;
}