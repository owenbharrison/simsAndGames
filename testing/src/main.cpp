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
std::vector<vf2d> poissonDiscSample(const cmn::AABB& box, float rad, int samples=30) {
	std::vector<vf2d> pts;

	//determine spacing
	float cell_size=rad/std::sqrtf(2);
	int w=1+(box.max.x-box.min.x)/cell_size;
	int h=1+(box.max.y-box.min.y)/cell_size;
	int* grid=new int[w*h];
	memset(grid, -1, sizeof(int)*w*h);

	//add center as start point
	std::list<vf2d> spawn_pts;
	spawn_pts.push_back(box.getCenter());

	//while spawn points viable,
	while(spawn_pts.size()) {
		//choose random spawn pt
		auto it=spawn_pts.begin();
		std::advance(it, rand()%spawn_pts.size());
		const auto& ctr=*it;

		//try find a close point near it to add
		bool accepted=false;
		for(int k=0; k<samples; k++) {
			float angle=cmn::random(2*cmn::Pi);
			float dist=cmn::random(rad, 2*rad);
			vf2d cand=ctr+cmn::polar(dist, angle);

			//cand must be in region
			if(!box.contains(cand)) continue;

			//check 5x5 kernel around cand
			bool valid=true;
			int ci=cand.x/cell_size;
			int cj=cand.y/cell_size;
			int si=std::max(0, ci-2);
			int sj=std::max(0, cj-2);
			int ei=std::min(w-1, ci+2);
			int ej=std::min(h-1, cj+2);
			for(int i=si; i<=ei; i++) {
				for(int j=sj; j<=ej; j++) {
					const int& idx=grid[i+w*j];
					if(idx!=-1&&(cand-pts[idx]).mag2()<rad*rad) {
						valid=false;
						break;
					}
				}
				if(!valid) break;
			}

			if(valid) {
				//add point to list and make it spawnable
				int i=cand.x/cell_size;
				int j=cand.y/cell_size;
				grid[i+w*j]=pts.size();
				pts.push_back(cand);
				spawn_pts.push_back(cand);
				accepted=true;
				break;
			}
		}
		//otherwise remove from spawn list
		if(!accepted) spawn_pts.erase(it);
	}

	delete[] grid;

	return pts;
}

struct Triangle {
	int e[3]{0, 0, 0};

	Triangle() {}

	Triangle(int a, int b, int c) {
		if(a>b) std::swap(a, b);
		if(b>c) std::swap(b, c);
		if(a>b) std::swap(a, b);
		e[0]=a, e[1]=b, e[2]=c;
	}

	bool contains(int a) const {
		for(int i=0; i<3; i++) {
			if(e[i]==a) return true;
		}
		return false;
	}

	bool equals(const Triangle& t) const {
		for(int i=0; i<3; i++) {
			if(t.e[i]!=e[i]) return false;
		}
		return true;
	}

	void getEdge(int i, int& e0, int& e1) const {
		e0=e[i], e1=e[(i+1)%3];
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

//https://en.wikipedia.org/wiki/Bowyer-Watson_algorithm
std::list<Triangle> delaunayTriangulation(const std::vector<vf2d>& pts_ref) {
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
			bool cc=getCircumcircle(pts[t.e[0]], pts[t.e[1]], pts[t.e[2]], pos, rad_sq);
			if(cc&&(pos-pt).mag2()<rad_sq) bad_tris.push_back(t);
		}
		
		//find the boundary of the polygonal hole
		std::list<vi2d> polygon;
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
					if(a>b) std::swap(a, b);
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
			tris.emplace_back(e.x, e.y, p);
		}
	}

	//done inserting points, now clean up
	for(auto it=tris.begin(); it!=tris.end();) {
		//if tri has super tri vert, remove
		bool contains=false;
		for(int i=0; i<3; i++) {
			for(int j=0; j<3; j++) {
				if(it->e[i]==st_a+j) {
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

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="poisson disc sampling testing";
	}

	std::vector<vf2d> pts;

	std::list<Triangle> tris;

	void reset() {
		//poisson disc sampling
		cmn::Stopwatch pts_watch;
		pts_watch.start();

		cmn::AABB box{{0, 0}, vf2d(ScreenWidth(), ScreenHeight())};
		pts=poissonDiscSample(box, 8, 10);

		pts_watch.stop();
		auto pts_dur=pts_watch.getMicros();
		std::cout<<"pts took: "<<pts_dur<<"us ("<<(pts_dur/1000.f)<<"ms)\n";

		//delaunay triangulation
		cmn::Stopwatch tri_watch;
		tri_watch.start();

		tris=delaunayTriangulation(pts);

		tri_watch.stop();
		auto tri_dur=tri_watch.getMicros();
		std::cout<<"tri took: "<<tri_dur<<"us ("<<(tri_dur/1000.f)<<"ms)\n";
	}

	bool OnUserCreate() override {
		reset();

		return true;
	}

	bool OnUserUpdate(float dt) override {
		if(GetKey(olc::Key::ENTER).bPressed) reset();
		
		Clear(olc::BLACK);

		for(const auto& t:tris) {
			//draw each edge
			DrawLine(pts[t.e[0]], pts[t.e[1]]);
			DrawLine(pts[t.e[1]], pts[t.e[2]]);
			DrawLine(pts[t.e[2]], pts[t.e[0]]);
		}

		for(const auto& p:pts) {
			Draw(p, olc::RED);
		}

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(200, 200, 2, 2, false, true)) e.Start();

	return 0;
}