#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "common/utils.h"
#include "common/aabb.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

std::vector<vf2d> clipPolyByBisector(const std::vector<vf2d>& poly, const vf2d& a, const vf2d& b) {
	std::vector<vf2d> out;
	vf2d sub=b-a, mid=(a+b)/2;
	for(int i=0; i<poly.size(); i++) {
		const auto& prev=poly[(poly.size()+i-1)%poly.size()];
		const auto& curr=poly[i];
		bool prev_in=sub.dot(prev-mid)<0;
		bool curr_in=sub.dot(curr-mid)<0;
		if(curr_in!=prev_in) {
			vf2d norm(-sub.y, sub.x);
			vf2d tu=cmn::lineLineIntersection(mid, mid+norm, prev, curr);
			out.push_back(prev+tu.y*(curr-prev));
		}
		if(curr_in) out.push_back(curr);
	}
	return out;
}

#include <list>

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Voronoi Sharding";
	}

	const float pt_rad=3;
	vf2d* grab_pt=nullptr;
	std::list<vf2d> pts;

	float total_dt=0;

	bool OnUserCreate() override {
		std::srand(std::time(0));

		generatePoints();

		return true;
	}

	void generatePoints() {
		pts.clear();
		grab_pt=nullptr;

		for(int i=0; i<5; i++) {
			float x=cmn::random(ScreenWidth());
			float y=cmn::random(ScreenHeight());
			pts.push_back({x, y});
		}
	}

	bool OnUserUpdate(float dt) override {
		const vf2d mouse_pos=GetMousePos();

		if(GetKey(olc::Key::N).bPressed) generatePoints();

		const auto select_action=GetMouse(olc::Mouse::LEFT);
		if(select_action.bPressed) {
			grab_pt=nullptr;
			for(auto& p:pts) {
				if((mouse_pos-p).mag2()<pt_rad*pt_rad) {
					grab_pt=&p;
				}
			}
		}
		if(select_action.bHeld) {
			if(grab_pt) *grab_pt=mouse_pos;
		}
		if(select_action.bReleased) {
			grab_pt=nullptr;
		}

		total_dt+=dt;

		//calculate polygons
		std::vector<std::vector<vf2d>> polys;
		for(auto ait=pts.begin(); ait!=pts.end(); ait++) {
			//starting polygon
			std::vector<vf2d> poly{
				{0, 0},
				vf2d(ScreenWidth(), 0),
				vf2d(ScreenWidth(), ScreenHeight()),
				vf2d(0, ScreenHeight())
			};
			//iteratively clip by all bisectors
			for(auto bit=pts.begin(); bit!=pts.end(); bit++) {
				if(bit==ait) continue;

				poly=clipPolyByBisector(poly, *ait, *bit);
			}

			polys.push_back(poly);
		}

		//black background
		Clear(olc::BLACK);

		//fill polygons
		for(int j=0; j<polys.size(); j++) {
			const auto& poly=polys[j];

			//color based on time and cell index
			std::srand(100*int(total_dt)+3*j);
			olc::Pixel col(std::rand()%256, std::rand()%256, std::rand()%256);

			//ear clipping triangulation
			{
				//initialize indexes
				std::list<int> indexes;
				for(int i=0; i<poly.size(); i++) indexes.emplace_back(i);

				//ear clipping triangulation
				for(auto curr=indexes.begin(); curr!=indexes.end();) {
					//get previous and next points
					auto prev=std::prev(curr==indexes.begin()?indexes.end():curr);
					auto next=std::next(curr); if(next==indexes.end()) next=indexes.begin();
					const auto& pt_p=poly[*prev], & pt_c=poly[*curr], & pt_n=poly[*next];

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
						const auto& pt_o=poly[*other];
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
						FillTriangle(pt_p, pt_c, pt_n, col);

						//remove this index and start over
						indexes.erase(curr);
						curr=indexes.begin();
					} else curr++;
				}
			}
		}

		//outline polygons
		for(const auto& p:polys) {
			for(int i=0; i<p.size(); i++) {
				const auto& a=p[i], & b=p[(1+i)%p.size()];
				DrawLine(a, b, olc::WHITE);
			}
		}

		//show cell centers
		for(const auto& p:pts) {
			FillCircle(p, pt_rad, olc::BLACK);
		}

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(400, 300, 2, 2, false, true)) e.Start();

	return 0;
}