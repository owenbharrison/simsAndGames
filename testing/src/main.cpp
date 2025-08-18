#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "common/utils.h"
#include "common/aabb.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

vf2d rotateVec(const vf2d& v, float f) {
	float c=std::cos(f), s=std::sin(f);
	return {c*v.x-s*v.y, s*v.x+c*v.y};
}

struct Shape {
	std::vector<vf2d> pts;
	vf2d pos;
	float rot=0;

	Shape() {}

	Shape(const vf2d& p, float r, int n) {
		pos=p;
		for(int i=0; i<n; i++) {
			float angle=2*cmn::Pi*i/n;
			pts.push_back({r*std::cos(angle), r*std::sin(angle)});
		}
	}

	vf2d localToWorld(const vf2d& v) const {
		return pos+rotateVec(v, rot);
	}

	vf2d worldToLocal(const vf2d& v) const {
		return rotateVec(v-pos, -rot);
	}

	bool contains(vf2d pt) const {
		pt=worldToLocal(pt);
		vf2d dir=cmn::polar<vf2d>(1, cmn::random(2*cmn::Pi));

		int num=0;
		for(int i=0; i<pts.size(); i++) {
			vf2d a=pts[i];
			vf2d b=pts[(1+i)%pts.size()];
			vf2d tu=cmn::lineLineIntersection(pt, pt+dir, a, b);
			if(tu.x>0&&tu.y>0&&tu.y<1) num++;
		}

		return num%2;
	}

	std::vector<vf2d> getAxes() const {
		std::vector<vf2d> axes;
		for(int i=0; i<pts.size(); i++) {
			vf2d a=pts[i];
			vf2d b=pts[(1+i)%pts.size()];
			vf2d tang=(b-a).norm();
			vf2d norm(tang.y, -tang.x);
			axes.push_back(rotateVec(norm, rot));
		}
		return axes;
	}

	void fill(olc::PixelGameEngine* pge, olc::Pixel col) const {
		//initialize indexes
		std::list<int> indexes;
		for(int i=0; i<pts.size(); i++) indexes.emplace_back(i);

		//ear clipping triangulation
		for(auto curr=indexes.begin(); curr!=indexes.end();) {
			//get previous and next points
			auto prev=std::prev(curr==indexes.begin()?indexes.end():curr);
			auto next=std::next(curr); if(next==indexes.end()) next=indexes.begin();
			const auto& pt_p=pts[*prev], & pt_c=pts[*curr], & pt_n=pts[*next];

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
				const auto& pt_o=pts[*other];
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
				pge->FillTriangle(
					localToWorld(pt_p),
					localToWorld(pt_c),
					localToWorld(pt_n),
					col
				);

				//remove this index and start over
				indexes.erase(curr);
				curr=indexes.begin();
			} else curr++;
		}
	}

	void drawOutline(olc::PixelGameEngine* pge, olc::Pixel col) const {
		for(int i=0; i<pts.size(); i++) {
			vf2d a=localToWorld(pts[i]);
			vf2d b=localToWorld(pts[(1+i)%pts.size()]);
			pge->DrawLine(a, b, col);
		}
	}

	void drawNorms(olc::PixelGameEngine* pge, float r, olc::Pixel col) const {
		for(int i=0; i<pts.size(); i++) {
			vf2d a=pts[i];
			vf2d b=pts[(1+i)%pts.size()];
			vf2d m=(a+b)/2;
			vf2d tang=(b-a).norm();
			vf2d norm=rotateVec({tang.y, -tang.x}, rot);
			pge->DrawLine(m, m+r*norm, col);
		}
	}

	void drawVerts(olc::PixelGameEngine* pge, olc::Pixel col) const {
		for(int i=0; i<pts.size(); i++) {
			vf2d p=localToWorld(pts[i]);
			auto str=std::to_string(i);
			pge->DrawString(vf2d(p.x-4*str.length(), p.y-4), str, col);
		}
	}

	static void satCollide(Shape& a, Shape& b) {
		std::vector<vf2d> axes;
		{
			auto axes_a=a.getAxes();
			axes.insert(axes.end(), axes_a.begin(), axes_a.end());
			auto axes_b=b.getAxes();
			axes.insert(axes.end(), axes_b.begin(), axes_b.end());
		}

		//for each axis
		vf2d norm;
		float sep=-1;
		for(const auto& n:axes) {
			//project a's pts
			float min_a=0, max_a=0;
			for(int i=0; i<a.pts.size(); i++) {
				float v=n.dot(a.localToWorld(a.pts[i]));
				if(i==0||v<min_a) min_a=v;
				if(i==0||v>max_a) max_a=v;
			}
			//project b's pts
			float min_b=0, max_b=0;
			for(int i=0; i<b.pts.size(); i++) {
				float v=n.dot(b.localToWorld(b.pts[i]));
				if(i==0||v<min_b) min_b=v;
				if(i==0||v>max_b) max_b=v;
			}

			//are ranges overlapping?
			float dab=max_a-min_b;
			if(dab<0) return;
			float dba=max_b-min_a;
			if(dba<0) return;

			float d=std::min(dab, dba);

			//get min overlap
			if(sep<0||d<sep) sep=d, norm=n;
		}

		//make sure norm pushes apart
		if(norm.dot(a.pos-b.pos)<0) norm*=-1;
		vf2d diff=.5f*sep*norm;
		a.pos+=diff;
		b.pos-=diff;
	}
};

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Separating Axis Theorem";
	}

	Shape s1, s2;

	bool show_norms=false;
	bool show_verts=false;

	vf2d mouse_pos, prev_mouse_pos;

	bool OnUserCreate() override {
		const vf2d resolution(ScreenWidth(), ScreenHeight());
		s1=Shape(vf2d(.33f, .67f)*resolution, 30, 5);
		s2=Shape(vf2d(.67f, .33f)*resolution, 40, 3);

		return true;
	}

	bool OnUserUpdate(float dt) override {
		prev_mouse_pos=mouse_pos;
		mouse_pos=GetMousePos();
		{
			bool mouse_down=GetMouse(olc::Mouse::LEFT).bHeld;
			vf2d mouse_delta=mouse_pos-prev_mouse_pos;
			int mouse_amt=GetMouseWheel();
			if(mouse_amt!=0) mouse_amt=25*mouse_amt/std::abs(mouse_amt);
			if(s1.contains(mouse_pos)) {
				if(mouse_down) s1.pos+=mouse_delta;
				s1.rot+=mouse_amt*dt;
			} else if(s2.contains(mouse_pos)) {
				if(mouse_down) s2.pos+=mouse_delta;
				s2.rot+=mouse_amt*dt;
			}
		}

		if(GetKey(olc::Key::N).bPressed) show_norms^=true;
		if(GetKey(olc::Key::V).bPressed) show_verts^=true;

		Shape::satCollide(s1, s2);

		Clear(olc::WHITE);

		//show shapes
		s1.fill(this, olc::YELLOW);
		s1.drawOutline(this, olc::BLACK);
		s2.fill(this, olc::BLUE);
		s2.drawOutline(this, olc::BLACK);

		if(show_norms) {
			s1.drawNorms(this, 5, olc::BLACK);
			s2.drawNorms(this, 5, olc::BLACK);
		}

		if(show_verts) {
			s1.drawVerts(this, olc::RED);
			s2.drawVerts(this, olc::RED);
		}

		//separating axes
		std::vector<vf2d> axes;
		{
			auto a1=s1.getAxes();
			axes.insert(axes.end(), a1.begin(), a1.end());
			auto a2=s2.getAxes();
			axes.insert(axes.end(), a2.begin(), a2.end());
		}

		//project shapes onto axes
		{
			//aabb to envelop shapes
			cmn::AABB box;
			for(const auto& p:s1.pts) {
				box.fitToEnclose(s1.localToWorld(p));
			}
			for(const auto& p:s2.pts) {
				box.fitToEnclose(s2.localToWorld(p));
			}
			vf2d ctr=box.getCenter();
			float rad=(box.max-box.min).mag()/2;

			//for each axis
			for(const auto& n:axes) {
				//project s1's pts onto it
				float min1=0, max1=0;
				for(int i=0; i<s1.pts.size(); i++) {
					float v=n.dot(s1.localToWorld(s1.pts[i]));
					if(i==0||v<min1) min1=v;
					if(i==0||v>max1) max1=v;
				}
				//project s2's pts onto it
				float min2=0, max2=0;
				for(int i=0; i<s2.pts.size(); i++) {
					float v=n.dot(s2.localToWorld(s2.pts[i]));
					if(i==0||v<min2) min2=v;
					if(i==0||v>max2) max2=v;
				}

				//do the ranges overlap?
				bool overlap=!(min1>max2||min2>max1);
				vf2d tang(n.y, -n.x);
				vf2d front=(ctr.dot(tang)-rad+2)*tang;
				vf2d back=(ctr.dot(tang)-rad-2)*tang;
				olc::Pixel col=overlap?olc::RED:olc::GREEN;
				DrawLine(front+min1*n, front+max1*n, col);
				DrawLine(back+min2*n, back+max2*n, col);
			}
		}

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(300, 300, 2, 2, false, true)) e.Start();

	return 0;
}