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
	vf2d ab=a-b, ac=a-c, cd=c-d;
	return vf2d(
		ac.cross(cd),
		ac.cross(ab)
	)/ab.cross(cd);
}

float clamp(float x, float a, float b) {
	if(x<a) return a;
	if(x>b) return b;
	return x;
}

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Example";
	}

	//user input
	vf2d mouse_pos;

	//points to work with
	const float rad=2;
	vf2d* drag_pt=nullptr;
	std::list<vf2d*> points;

	//point selection, point constraints
	std::list<vf2d*> ref_pts;
	std::list<PointConstraint*> point_constraints;
	bool unique(PointConstraint* cst) const {
		for(const auto& c:point_constraints) {
			if(*c==*cst) return false;
		}
		return true;
	}

	//snipping feature
	//only thru line constraints?
	std::list<vf2d> snip;
	float snip_timer=0;
	
	//line selection, line constraints
	std::list<LineConstraint*> ref_lines;
	std::list<GeometryConstraint*> geom_constraints;
	bool unique(GeometryConstraint* cst) const {
		for(const auto& c:geom_constraints) {
			if(*c==*cst) return false;
		}
		return true;
	}

	bool OnUserCreate() override {

		return true;
	}

	void solve() {
		//solve all constraints
		for(int i=0; i<200; i++) {
			//dist constraints...
			for(const auto& c:geom_constraints) c->solve();
			for(const auto& c:point_constraints) c->solve();
		}
	}

	bool OnUserUpdate(float dt) override {
		mouse_pos=GetMousePos();

		if(GetKey(olc::Key::A).bPressed) points.emplace_back(new vf2d(mouse_pos));

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

		//set drag pt to point under mouse
		const auto drag_action=GetMouse(olc::Mouse::LEFT);
		if(drag_action.bPressed) {
			drag_pt=nullptr;
			for(const auto& p:points) {
				if((*p-mouse_pos).mag()<rad) {
					drag_pt=p;
					break;
				}
			}
		}
		//set to mouse on drag
		if(drag_action.bHeld) {
			if(drag_pt) {
				*drag_pt=mouse_pos;
				for(const auto& c:point_constraints) {
					if(c->getID()!=LINE_CST) continue;
					if(drag_pt!=c->a&&drag_pt!=c->b) continue;

					//is this how you are supposed to do things?
					dynamic_cast<LineConstraint*>(c)->update();
				}
			}
		}
		//release
		if(drag_action.bReleased) drag_pt=nullptr;

		const auto select_action=GetMouse(olc::Mouse::RIGHT);
		if(select_action.bPressed) {
			bool pt_selected=false;
			for(const auto& p:points) {
				if((*p-mouse_pos).mag()<rad) {
					pt_selected=true;

					bool unique=true;
					for(auto it=ref_pts.begin(); it!=ref_pts.end(); it++) {
						if(*it==p) {
							//unselect
							ref_pts.erase(it);

							unique=false;
							break;
						}
					}
					//add if new
					if(unique) ref_pts.emplace_back(p);
				}
			}
			//ensure no selection overlap	
			if(!pt_selected) for(const auto& c:point_constraints) {
				if(c->getID()!=LINE_CST) continue;

				//find closest point on segment
				vf2d ba=*c->b-*c->a, pa=mouse_pos-*c->a;
				float t=clamp(pa.dot(ba)/ba.dot(ba), 0, 1);
				vf2d close_pt=*c->a+t*ba;

				//if t close .5 -> store t
				//if a pressed, add point + coincedent

				//is it being hovered over?
				if((close_pt-mouse_pos).mag()<rad) {
					bool unique=true;
					for(auto it=ref_lines.begin(); it!=ref_lines.end(); it++) {
						if(*it==c) {
							//unselect
							ref_lines.erase(it);

							unique=false;
							break;
						}
					}
					//add if new
					if(unique) ref_lines.emplace_back(dynamic_cast<LineConstraint*>(c));
				}
			}
		}

		//clear selection
		if(GetKey(olc::Key::ESCAPE).bPressed) {
			ref_pts.clear();
			ref_lines.clear();
		}

		//add point constraints
		if(GetKey(olc::Key::V).bPressed) {
			for(auto ait=ref_pts.begin(); ait!=ref_pts.end(); ait++) {
				for(auto bit=std::next(ait); bit!=ref_pts.end(); bit++) {
					PointConstraint* vert=new VerticalConstraint(*ait, *bit);
					if(unique(vert)) {
						point_constraints.emplace_back(vert);
					} else std::cout<<"constraint already exists.\n";
				}
			}
		}

		if(GetKey(olc::Key::H).bPressed) {
			for(auto ait=ref_pts.begin(); ait!=ref_pts.end(); ait++) {
				for(auto bit=std::next(ait); bit!=ref_pts.end(); bit++) {
					PointConstraint* horiz=new HorizontalConstraint(*ait, *bit);
					if(unique(horiz)) {
						point_constraints.emplace_back(horiz);
					} else std::cout<<"constraint already exists.\n";
				}
			}
		}

		if(GetKey(olc::Key::L).bPressed) {
			size_t num=ref_pts.size();
			if(ref_pts.size()==2) {
				auto a=ref_pts.front(), b=ref_pts.back();
				PointConstraint* line=new LineConstraint(a, b);
				if(unique(line)) {
					point_constraints.emplace_back(line);
					ref_pts.clear();
				} else std::cout<<"constraint already exists.\n";
			}
		}

		//add equal constraints
		if(GetKey(olc::Key::E).bPressed) {
			for(auto ait=ref_lines.begin(); ait!=ref_lines.end(); ait++) {
				for(auto bit=std::next(ait); bit!=ref_lines.end(); bit++) {
					GeometryConstraint* equal=new EqualConstraint(*ait, *bit);
					if(unique(equal)) {
						geom_constraints.emplace_back(equal);
					} else std::cout<<"constraint already exists.\n";
				}
			}
		}

		solve();

		//render
		Clear(olc::GREY);

		//draw all point constraints
		//dotted lines?
		for(const auto& c:point_constraints) {
			olc::Pixel col;
			switch(c->getID()) {
				case 1: col=olc::MAGENTA; break;
				case 2: col=olc::CYAN; break;
				case 3: col=olc::YELLOW; break;
			}
			DrawLine(*c->a, *c->b, col);
		}

		//draw all line constraints
		//for(const auto& c:line_constraints) {
		//}

		//draw selected lines
		for(const auto& c:ref_lines) {
			DrawLine(*c->a, *c->b, olc::BLUE);
		}

		//render snip
		{
			bool is_first=true;
			vf2d first, prev;
			for(const auto& s:snip) {
				if(is_first) is_first=false;
				else {
					DrawLine(prev, s, olc::RED);

					//render snip pts
					for(const auto& c:point_constraints) {
						vf2d tu=lineLineIntersection(prev, s, *c->a, *c->b);
						if(tu.x>=0&&tu.x<=1&&tu.y>=0&&tu.y<=1) {
							vf2d ix=prev+tu.x*(s-prev);
							FillCircle(ix, rad, olc::DARK_GREY);
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
			DrawCircle(*r, rad, olc::BLUE);
		}

		return true;
	}
};

int main() {
	Example demo;
	if(demo.Construct(300, 300, 2, 2, false, true)) demo.Start();

	return 0;
}