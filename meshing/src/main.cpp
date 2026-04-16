#define OLC_PGE_APPLICATION
#include "olc/include/olcPixelGameEngine.h"

#include "cmn/math/v2d.h"

#include "poisson.h"

#include "delaunay.h"

using cmn::vf2d;

void stressGradient(float t, float& r, float& g, float& b) {
	static const float cols[5][3]{
		{0, 0, 1},//blue
		{0, 1, 1},//cyan
		{0, 1, 0},//green
		{1, 1, 0},//yellow
		{1, 0, 0}//red
	};
	static const int num=sizeof(cols)/sizeof(cols[0]);

	//clamp percent
	if(t<0) t=0;
	if(t>=1) t=.999f;

	//floor index, fract t
	float x=t*(num-1);
	int i=x;
	t=x-i;

	//lerp cols
	const auto& c1=cols[i];
	const auto& c2=cols[i+1];
	r=c1[0]+t*(c2[0]-c1[0]);
	g=c1[1]+t*(c2[1]-c1[1]);
	b=c1[2]+t*(c2[2]-c1[2]);
}

float snapTo(float a, float b) {
	return b*std::round(a/b);
}

struct MeshingUI : olc::PixelGameEngine {
	MeshingUI() {
		sAppName="Meshing Demo";
	}

	vf2d mouse_pos;

	const float rad=5;
	std::vector<vf2d> shell, verts;
	std::vector<Edge> constraints;
	std::vector<Tri> tris;

	bool show_quality=true;
	bool show_constraints=true;
	bool show_grid=true;
	const float grid_sz=15;

	bool OnUserCreate() override {
		return true;
	}

	void handleAddAction() {
		const auto action=GetMouse(olc::Mouse::LEFT);
		if(action.bPressed) {
			shell.clear(), verts.clear();
			constraints.clear();
			tris.clear();
		}

		if(action.bHeld) shell.push_back(mouse_pos);

		if(action.bReleased) {
			//ensure verts are spaced apart
			auto isUnique=[&] (const vf2d& p) {
				for(const auto& v:verts) {
					if((v-p).mag_sq()<rad*rad) {
						return false;
					}
				}
				return true;
			};

			//make shell
			for(int i=0; i<shell.size(); i++) {
				int j=(i+1)%shell.size();
				const auto& a=shell[i], ab=shell[j]-a;

				//ensure verts placed along jumps
				const int num=100;
				for(int k=0; k<num; k++) {
					float t=k/(num-1.f);
					vf2d p=a+t*ab;
					if(isUnique(p)) verts.push_back(p);
				}
			}
			shell.clear();

			//wind shell
			for(int i=0; i<verts.size(); i++) {
				int j=(i+1)%verts.size();
				constraints.push_back({i, j});
			}

			//ray-polygon test
			auto insideShell=[&] (const vf2d& p) {
				//random direction
				float angle=cmn::randFloat(2*cmn::Pi);
				vf2d pr=p+cmn::polar<vf2d>(1, angle);

				int num=0;
				for(const auto& c:constraints) {
					const auto& a=verts[c[0]], & b=verts[c[1]];
					//treat as ray
					vf2d tu=cmn::lineLineIntersection(p, pr, a, b);
					if(tu.x>0&&tu.y>0&&tu.y<1) num++;
				}

				//odd? inside!
				return num%2;
			};

			//add verts inside shell
			cmn::vf2d min(1e30f, 1e30f);
			cmn::vf2d max(-1e30f, -1e30f);
			for(const auto& p:verts) {
				if(p.x<min.x) min.x=p.x;
				if(p.y<min.y) min.y=p.y;
				if(p.x>max.x) max.x=p.x;
				if(p.y>max.y) max.y=p.y;
			}
			auto poisson=poissonDiscSample(min, max, rad);
			for(const auto& p:poisson) {
				if(insideShell(p)&&isUnique(p)) verts.push_back(p);
			}

			//triangulate w.r.t. shell
			auto cdt=constrainedDelaunayTriangulate(verts, constraints);

			//remove tris outside shell
			for(const auto& t:cdt) {
				vf2d c=(verts[t[0]]+verts[t[1]]+verts[t[2]])/3;
				if(insideShell(c)) tris.push_back(t);
			}
		}
	}

	void update() {
		mouse_pos=vf2d(GetMouseX(), GetMouseY());

		//snap mouse pos to grid
		if(GetKey(olc::Key::SHIFT).bHeld) {
			mouse_pos.x=snapTo(mouse_pos.x, grid_sz);
			mouse_pos.y=snapTo(mouse_pos.y, grid_sz);
		}

		handleAddAction();

		//graphics toggles
		if(GetKey(olc::Key::Q).bPressed) show_quality^=true;
		if(GetKey(olc::Key::C).bPressed) show_constraints^=true;
		if(GetKey(olc::Key::G).bPressed) show_grid^=true;
	}

	void renderGrid(const olc::Pixel& col) {
		//vertical lines
		int num_x=1+ScreenWidth()/grid_sz;
		for(int i=0; i<=num_x; i++) {
			float x=grid_sz*i;
			olc::vf2d top(x, 0), btm(x, ScreenHeight());
			DrawLineDecal(top, btm, col);
		}

		//horizontal lines
		float num_y=1+ScreenHeight()/grid_sz;
		for(int j=0; j<=num_y; j++) {
			float y=grid_sz*j;
			olc::vf2d lft(0, y), rgt(ScreenWidth(), y);
			DrawLineDecal(lft, rgt, col);
		}
	}

	void renderShell() {
		for(int i=0; i<shell.size(); i++) {
			int j=(i+1)%shell.size();
			const auto& a=shell[i], b=shell[j];
			DrawLineDecal({a.x, a.y}, {b.x, b.y}, olc::RED);
		}
	}

	void renderTris() {
		for(const auto& t:tris) {
			const auto& v1=verts[t[0]];
			const auto& v2=verts[t[1]];
			const auto& v3=verts[t[2]];

			//get color
			float r=1, g=1, b=1;
			if(show_quality) {
				//find aspect ratio
				float la=(v1-v2).mag();
				float lb=(v2-v3).mag();
				float lc=(v3-v1).mag();
				float s=(la+lb+lc)/2;
				float asp=la*lb*lc/8/(s-la)/(s-lb)/(s-lc);

				//color based on how close to 1
				float t=1-std::exp(2.4f*(1-asp));
				stressGradient(t, r, g, b);
			}

			FillTriangleDecal(
				{v1.x, v1.y},
				{v2.x, v2.y},
				{v3.x, v3.y},
				olc::PixelF(r, g, b)
			);
		}

		for(const auto& t:tris) {
			const auto& a=verts[t[0]];
			const auto& b=verts[t[1]];
			const auto& c=verts[t[2]];
			olc::vf2d pa{a.x, a.y};
			olc::vf2d pb{b.x, b.y};
			olc::vf2d pc{c.x, c.y};
			DrawLineDecal(pa, pb, olc::BLACK);
			DrawLineDecal(pb, pc, olc::BLACK);
			DrawLineDecal(pc, pa, olc::BLACK);
		}
	}

	void renderConstraints() {
		for(const auto& c:constraints) {
			const auto& a=verts[c[0]];
			const auto& b=verts[c[1]];
			DrawLineDecal({a.x, a.y}, {b.x, b.y}, olc::WHITE);
		}
	}

	void render() {
		FillRectDecal({0, 0}, GetScreenSize(), olc::Pixel(87, 87, 87));

		if(show_grid) renderGrid(olc::Pixel(156, 156, 156));

		renderShell();

		renderTris();

		if(show_constraints) renderConstraints();
	}

	bool OnUserUpdate(float dt) override {
		update();

		render();

		return true;
	}
};

int main() {
	MeshingUI mui;
	bool fullscreen=false;
	bool vsync=true;
	bool cohesion=false;//?
	bool realwindow=true;//resize
	if(mui.Construct(640, 480, 1, 1, fullscreen, vsync, cohesion, realwindow)) mui.Start();

	return 0;
}