#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;
using olc::vi2d;

#include "common/utils.h"
#include "common/aabb.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;

	vf2d polar(float rad, float angle) {
		return polar_generic<vf2d>(rad, angle);
	}

	vf2d lineLineIntersection(
		const vf2d& a, const vf2d& b,
		const vf2d& c, const vf2d& d) {
		return lineLineIntersection_generic(a, b, c, d);
	}
}

#include "common/stopwatch.h"

//https://www.youtube.com/watch?v=7WcmyxyFO7o
std::list<vf2d> poissonDiscSampling(const cmn::AABB& box, float rad) {
	//determine spacing
	float cell_size=rad/std::sqrtf(2);
	int w=1+(box.max.x-box.min.x)/cell_size;
	int h=1+(box.max.y-box.min.y)/cell_size;
	vf2d** grid=new vf2d*[w*h];
	for(int i=0; i<w*h; i++) grid[i]=nullptr;

	//where can i spawn from?
	std::list<vf2d> spawn_pts{box.getCenter()};

	//as long as there are spawnable pts,
	std::list<vf2d> pts;
	while(spawn_pts.size()) {
		//choose random spawn pt
		auto it=spawn_pts.begin();
		std::advance(it, rand()%spawn_pts.size());
		const auto& spawn=*it;

		//try n times to add pt
		int k=0;
		const int samples=20;
		for(; k<samples; k++) {
			float angle=cmn::random(2*cmn::Pi);
			float dist=cmn::random(rad, 2*rad);
			vf2d cand=spawn+cmn::polar(dist, angle);
			if(!box.contains(cand)) continue;

			//check 3x3 region around candidate
			bool valid=true;
			int ci=(cand.x-box.min.x)/cell_size;
			int cj=(cand.y-box.min.y)/cell_size;
			int si=std::max(0, ci-2);
			int sj=std::max(0, cj-2);
			int ei=std::min(w-1, ci+2);
			int ej=std::min(h-1, cj+2);
			for(int i=si; i<=ei; i++) {
				for(int j=sj; j<=ej; j++) {
					//if there is a point, and its too close,
					const auto& idx=grid[i+w*j];
					if(idx&&(*idx-cand).mag2()<rad*rad) {
						valid=false;
						break;
					}
				}
				if(!valid) break;
			}

			//if no points too close, add the sucker
			if(valid) {
				if(ci<0||cj<0||ci>=w||cj>=h) continue;
				pts.push_back(cand);
				grid[ci+w*cj]=&pts.back();
				spawn_pts.push_back(cand);
				break;
			}
		}

		//not spawnable enough, remove
		if(k==samples) spawn_pts.erase(it);
	}

	delete[] grid;

	return pts;
}

#include <set>

//i dont want to pollute the global with triangle.
namespace delaunay {
	struct Triangle {
		int p[3]{0, 0, 0};

		Triangle() {}

		Triangle(int a, int b, int c) {
			if(a>b) std::swap(a, b);
			if(b>c) std::swap(b, c);
			if(a>b) std::swap(a, b);
			p[0]=a, p[1]=b, p[2]=c;
		}

		bool contains(int a) const {
			for(int i=0; i<3; i++) {
				if(p[i]==a) return true;
			}
			return false;
		}

		bool equals(const Triangle& t) const {
			for(int i=0; i<3; i++) {
				if(t.p[i]!=p[i]) return false;
			}
			return true;
		}

		void getEdge(int e, int& p0, int& p1) const {
			p0=p[e], p1=p[(e+1)%3];
		}

		static bool edgeShared(
			const Triangle& t0, const int& e0,
			const Triangle& t1, const int& e1
		) {
			int v00=0, v01=0, v10=0, v11=0;
			t0.getEdge(e0, v00, v01);
			t1.getEdge(e1, v10, v11);
			if(v00==v10&&v01==v11) return true;
			if(v00==v11&&v01==v10) return true;
			return false;
		}
	};

	struct Edge {
		int p[2]{0, 0};

		Edge() {}

		Edge(int a, int b) {
			if(a>b) std::swap(a, b);
			p[0]=a, p[1]=b;
		}

		bool operator<(const Edge& e) const {
			if(p[0]==e.p[0]) return p[1]<e.p[1];
			return p[0]<e.p[0];
		}
	};

	//https://en.wikipedia.org/wiki/Bowyer-Watson_algorithm
	std::list<Triangle> triangulate(const std::vector<vf2d>& pts_ref) {
		std::list<Triangle> tris;
		std::vector<vf2d> pts=pts_ref;
		int st_a=pts.size();
		{//make super triangle
			cmn::AABB box;
			for(const auto& p:pts_ref) {
				box.fitToEnclose(p);
			}
			box.min-=1, box.max+=1;
			float x=box.min.x, y=box.min.y;
			float w=box.max.x-x, h=box.max.y-y;
			pts.emplace_back(x-h, y);
			pts.emplace_back(x+w/2, y+h+w/2);
			pts.emplace_back(x+w+h, y);
			tris.emplace_back(st_a, st_a+1, st_a+2);
		}

		//perpendicular bisector
		auto getCircumcircle=[] (const vf2d& a, const vf2d& b, const vf2d& c, vf2d& p, float& r_sq) {
			float D=2*(a.x*(b.y-c.y)+b.x*(c.y-a.y)+c.x*(a.y-b.y));
			if(std::fabsf(D)<1e-6f) return false;

			float a_sq=a.x*a.x+a.y*a.y;
			float b_sq=b.x*b.x+b.y*b.y;
			float c_sq=c.x*c.x+c.y*c.y;

			p.x=(a_sq*(b.y-c.y)+b_sq*(c.y-a.y)+c_sq*(a.y-b.y))/D;
			p.y=(a_sq*(c.x-b.x)+b_sq*(a.x-c.x)+c_sq*(b.x-a.x))/D;
			r_sq=(p-a).mag2();

			return true;
		};

		//add all the points one at a time to the triangulation
		for(int p=0; p<st_a; p++) {
			const auto& pt=pts[p];

			//first find all the triangles that are no longer valid due to the insertion
			std::list<Triangle> bad_tris;
			for(const auto& t:tris) {
				//dont check tri pts
				if(t.contains(p)) continue;

				vf2d pos;
				float rad_sq;
				bool cc=getCircumcircle(pts[t.p[0]], pts[t.p[1]], pts[t.p[2]], pos, rad_sq);
				if(cc&&(pos-pt).mag2()<rad_sq) bad_tris.push_back(t);
			}

			//find the boundary of the polygonal hole
			std::list<Edge> polygon;
			for(const auto& bt:bad_tris) {
				//for each edge in tri
				for(int i=0; i<3; i++) {
					//if edge NOT shared by any other tri in bad_tris
					bool shared=false;
					for(const auto& o_bt:bad_tris) {
						if(&o_bt==&bt) continue;
						for(int j=0; j<3; j++) {
							if(Triangle::edgeShared(bt, i, o_bt, j)) {
								shared=true;
								break;
							}
						}
						if(shared) break;
					}
					//add edge to polygon
					if(!shared) {
						int a, b;
						bt.getEdge(i, a, b);
						polygon.emplace_back(a, b);
					}
				}

				//remove them from the data structure
				for(auto it=tris.begin(); it!=tris.end();) {
					if(it->equals(bt)) {
						it=tris.erase(it);
					} else it++;
				}
			}

			//re-triangulate the polygonal hole
			for(const auto& e:polygon) {
				//form a triangle from edge to point
				tris.emplace_back(e.p[0], e.p[1], p);
			}
		}

		//done inserting points, now clean up
		for(auto it=tris.begin(); it!=tris.end();) {
			//if tri has super tri vert, remove
			bool contains=false;
			for(int i=0; i<3; i++) {
				for(int j=0; j<3; j++) {
					if(it->p[i]==st_a+j) {
						contains=true;
						break;
					}
				}
				if(contains) break;
			}
			if(contains) {
				it=tris.erase(it);
			} else it++;
		}

		return tris;
	}

	//for connecting things up.
	template<typename TriContainer>
	std::set<Edge> extractEdges(const TriContainer& tris) {
		std::set<Edge> edges;
		for(const auto& t:tris) {
			for(int i=0; i<3; i++) {
				int a, b;
				t.getEdge(i, a, b);
				edges.emplace(a, b);
			}
		}
		return edges;
	}
}

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="meshing testing";
	}

	float rad=10;

	std::vector<vf2d> outer;

	int num_pts=0;
	vf2d* points=nullptr;
	std::list<delaunay::Edge> constraints, springs;

	bool OnUserCreate() override {
		return true;
	}

	void DepthLine(
		int x1, int y1,
		int x2, int y2
	) {
		auto draw=[&] (int x, int y, float t) {
			if(x<0||y<0||x>=ScreenWidth()||y>=ScreenHeight()) return;

			Draw(x, y, olc::PixelF(t, t, t));
		};
		int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;
		dx=x2-x1; dy=y2-y1;
		dx1=abs(dx), dy1=abs(dy);
		px=2*dy1-dx1, py=2*dx1-dy1;
		if(dy1<=dx1) {
			bool flip=false;
			if(dx>=0) x=x1, y=y1, xe=x2;
			else x=x2, y=y2, xe=x1, flip=true;
			draw(x, y, flip);
			float t_step=1.f/dx1;
			for(i=0; x<xe; i++) {
				x++;
				if(px<0) px+=2*dy1;
				else {
					if((dx<0&&dy<0)||(dx>0&&dy>0)) y++;
					else y--;
					px+=2*(dy1-dx1);
				}
				draw(x, y, flip?1-t_step*i:t_step*i);
			}
		} else {
			bool flip=false;
			if(dy>=0) x=x1, y=y1, ye=y2;
			else x=x2, y=y2, ye=y1, flip=true;
			draw(x, y, flip);
			float t_step=1.f/dy1;
			for(i=0; y<ye; i++) {
				y++;
				if(py<=0) py+=2*dx1;
				else {
					if((dx<0&&dy<0)||(dx>0&&dy>0)) x++;
					else x--;
					py+=2*(dx1-dy1);
				}
				draw(x, y, flip?1-t_step*i:t_step*i);
			}
		}
	}

	bool OnUserUpdate(float dt) override {
		const vf2d mouse_pos=GetMousePos();

		//every now and then, add addition point
		const auto add_action=GetMouse(olc::Mouse::LEFT);
		if(add_action.bPressed) {
			outer.clear();

			delete[] points;
			points=nullptr;
			constraints.clear();
			springs.clear();
		}
		if(add_action.bHeld) {
			//make sure points are far enough away
			bool unique=true;
			for(const auto& o:outer) {
				if((o-mouse_pos).mag()<rad) {
					unique=false;
					break;
				}
			}
			if(unique) outer.push_back(mouse_pos);
		}
		if(add_action.bReleased) {
			//distribute inside points
			cmn::AABB box;
			for(const auto& s:outer) {
				box.fitToEnclose(s);
			}
			std::list<vf2d> inside=poissonDiscSampling(box, rad);

			//remove outside points
			for(auto it=inside.begin(); it!=inside.end(); ) {
				vf2d dir=cmn::polar(1, cmn::random(2*cmn::Pi));

				int num_ix=0;
				for(int i=0; i<outer.size(); i++) {
					const auto& a=outer[i], & b=outer[(i+1)%outer.size()];
					vf2d tu=cmn::lineLineIntersection(a, b, *it, *it+dir);
					if(tu.x>=0&&tu.x<=1&&tu.y>=0) num_ix++;
				}

				if(num_ix%2) it++;
				else it=inside.erase(it);
			}

			//allocate
			num_pts=outer.size()+inside.size();
			points=new vf2d[num_pts];
			{//copy points over
				int i=0;
				for(const auto& p:outer) {
					points[i++]=p;
				}
				for(const auto& p:inside) {
					points[i++]=p;
				}
			}

			//connect constraints	
			for(int i=0; i<outer.size(); i++) {
				constraints.emplace_back(i, (i+1)%outer.size());
			}

			//ensure winding
			float area=0;
			for(const auto& c:constraints) {
				const auto& a=points[c.p[0]], & b=points[c.p[1]];
				area+=a.x*b.y-a.y*b.x;
			}
			if(area<0) {
				area*=-1;
				//reverse each constraint
				for(auto& c:constraints) {
					std::swap(c.p[0], c.p[1]);
				}
				//reverse constraints
				std::reverse(constraints.begin(), constraints.end());
			}

			//delaunay springs
			std::vector<vf2d> tri_pts(num_pts);
			for(int i=0; i<num_pts; i++) {
				tri_pts[i]=points[i];
			}
			auto tris=delaunay::triangulate(tri_pts);
			auto edges=delaunay::extractEdges(tris);
			for(const auto& e:edges) {
				springs.emplace_back(e.p[0], e.p[1]);
			}

			for(auto it=springs.begin(); it!=springs.end();) {
				//check if already constrained
				bool constrained=false;
				for(const auto& c:constraints) {
					if((c.p[0]==it->p[0]&&c.p[1]==it->p[1])||
						(c.p[0]==it->p[1]&&c.p[1]==it->p[0])) {
						constrained=true;
						break;
					}
				}
				if(constrained) {
					it=springs.erase(it);
					continue;
				}

				//check if spring "leaves" shape
				//naive: is midpoint inside shape?
				{
					vf2d pt=(points[it->p[0]]+points[it->p[1]])/2;
					vf2d dir=cmn::polar(1, cmn::random(2*cmn::Pi));

					int num_ix=0;
					for(int i=0; i<outer.size(); i++) {
						const auto& a=outer[i], & b=outer[(i+1)%outer.size()];
						vf2d tu=cmn::lineLineIntersection(a, b, pt, pt+dir);
						if(tu.x>=0&&tu.x<=1&&tu.y>=0) num_ix++;
					}

					if(num_ix%2==0) {
						it=springs.erase(it);
						continue;
					}
				}

				it++;
			}

			outer.clear();
		}

		Clear(olc::GREY);

		//show outer
		for(const auto& o:outer) {
			FillCircle(o, 2, olc::RED);
		}

		if(points) {
			for(const auto& s:springs) {
				DrawLine(points[s.p[0]], points[s.p[1]], olc::BLACK);
			}

			for(const auto& c:constraints) {
				DrawLine(points[c.p[0]], points[c.p[1]], olc::WHITE);
			}

			for(int i=0; i<num_pts; i++) {
				FillCircle(points[i], 2, olc::BLUE);
			}
		}
		
		DepthLine(ScreenWidth()/2, ScreenHeight()/2, mouse_pos.x, mouse_pos.y);

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(640, 480, 1, 1, false, true)) e.Start();

	return 0;
}