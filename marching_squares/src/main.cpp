#define OLC_PGE_APPLICATION
#include "olc/include/olcPixelGameEngine.h"

//for time
#include <ctime>

#include "cmn/utils.h"

#include "sdf_shape.h"

//c friendly!
void colorGradient(
	const float cols[][3], int num,
	float t, float& r, float& g, float& b
) {
	//clamp percent
	if(t<0) t=0;
	if(t>.999f) t=.999f;

	//floor index, fract t
	float x=t*(num-1);
	int i=x;
	t=x-i;

	//lerp cols
	const auto& c0=cols[i];
	const auto& c1=cols[1+i];
	r=c0[0]+t*(c1[0]-c0[0]);
	g=c0[1]+t*(c1[1]-c0[1]);
	b=c0[2]+t*(c1[2]-c0[2]);
}

void insideGradient(float t, float& r, float& g, float& b) {
	static const float cols[][3]{
		{0, 1, 0},//green
		{1, 1, 0},//yellow
		{1, 0, 0}//red
	};
	return colorGradient(
		cols, sizeof(cols)/sizeof(cols[0]),
		t, r, g, b
	);
}

void outsideGradient(float t, float& r, float& g, float& b) {
	static const float cols[][3]{
		{0, 1, 1},//cyan
		{0, 0, 1},//blue
		{.5f, 0, 1}//purple
	};
	return colorGradient(
		cols, sizeof(cols)/sizeof(cols[0]),
		t, r, g, b
	);
}

float invLerp(float v, float a, float b) {
	return (v-a)/(b-a);
}

olc::vf2d mix(const olc::vf2d& a, const olc::vf2d& b, float t) {
	return a+t*(b-a);
}

using olc::vf2d;

struct MarchingSquares : public olc::PixelGameEngine {
	const float min_cell_sz=3, max_cell_sz=25;
	float cell_sz=5;
	int width=0, height=0;
	int ix(int i, int j) { return i+width*j; }

	float* val_grid=nullptr;
	vf2d* pos_grid=nullptr;
	float* col_grid=nullptr;

	vf2d mouse_pos;

	std::list<SDFShape*> shapes;
	olc::vf2d* held_pt=nullptr;
	const float handle_rad=6;

	olc::Renderable prim_rect, prim_circ;

	bool combine_union=true;
	bool render_values=true;
	bool render_shapes=true;

	//max 2 edges per state
	//-1 is stop flag
	const int edge_table[16][4]{
		{-1, -1, -1, -1}, {0, 3, -1, -1},
		{0, 1, -1, -1}, {1, 3, -1, -1},
		{2, 3, -1, -1}, {0, 2, -1, -1},
		{0, 3, 1, 2}, {1, 2, -1, -1},
		{1, 2, -1, -1}, {0, 1, 2, 3},
		{0, 2, -1, -1}, {2, 3, -1, -1},
		{1, 3, -1, -1}, {0, 1, -1, -1},
		{0, 3, -1, -1}, {-1, -1, -1, -1}
	};
#pragma region SETUP HELPERS
	void setupShapes() {
		const olc::vf2d res=GetScreenSize();

		auto rect=new SDFRectangle{};
		rect->col={255, 0, 0, 255};
		rect->p0=vf2d(.1f, .1f)*res;
		rect->p1=vf2d(.7f, .9f)*res;
			
		shapes.push_back(rect);
		auto circ=new SDFCircle{};
		circ->col={0, 0, 255, 255};
		circ->ctr=vf2d(.7f, .5f)*res;
		circ->edge=vf2d(.9f, .5f)*res;
		shapes.push_back(circ);
	}

	//simple textures to help draw with
	void setupPrimitives() {
		prim_rect.Create(1, 1);
		prim_rect.Sprite()->SetPixel(0, 0, olc::WHITE);
		prim_rect.Decal()->Update();
		
		const int sz=1024;
		prim_circ.Create(sz, sz);
		SetDrawTarget(prim_circ.Sprite());
		Clear(olc::BLANK);
		FillCircle(sz/2, sz/2, sz/2);
		SetDrawTarget(nullptr);
		prim_circ.Decal()->Update();
	}
#pragma endregion

	bool OnUserCreate() override {
		std::srand(std::time(0));

		setupShapes();

		setupPrimitives();

		return true;
	}

#pragma region UPDATE HELPERS
	void updateSizing() {
		int w=1+ScreenWidth()/cell_sz;
		int h=1+ScreenHeight()/cell_sz;
		if(w==width&&h==height) return;

		delete[] val_grid;
		delete[] pos_grid;
		delete[] col_grid;

		width=w, height=h;
		val_grid=new float[width*height];
		pos_grid=new vf2d[width*height];
		col_grid=new float[3*width*height];
	}

	//update values based on signed distance field
	void updateGrids() {
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				int k=ix(i, j);

				//center of cell
				auto p=cell_sz*vf2d(.5f+i, .5f+j);
				pos_grid[k]=p;

				bool first=true;
				auto& record=val_grid[k];
				for(const auto& s:shapes) {
					float sd=s->signedDist(p);

					//min for union, max for intersection
					bool more=sd>record;
					if(first||(combine_union^more)) {
						record=sd;
						first=false;
					}
				}
				if(first) record=0;
			}
		}

		//find absolute value range
		float max=1e-6f;
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				float v=std::abs(val_grid[ix(i, j)]);
				if(v>max) max=v;
			}
		}

		//color ramp
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				int k=ix(i, j);

				//sign predicates in/out
				float t=val_grid[k]/max;
				float& r=col_grid[3*k];
				float& g=col_grid[1+3*k];
				float& b=col_grid[2+3*k];
				if(t>0) outsideGradient(t, r, g, b);
				else insideGradient(-t, r, g, b);
			}
		}
	}

	void handleAddAction() {
		//add rectangle
		if(GetKey(olc::Key::R).bPressed) {
			auto rect=new SDFRectangle{};
			rect->col=olc::PixelF(
				cmn::randFloat(),
				cmn::randFloat(),
				cmn::randFloat()
			);
			vf2d half_size(
				cmn::randFloat(20, 50),
				cmn::randFloat(20, 50)
			);
			rect->p0=mouse_pos-half_size;
			rect->p1=mouse_pos+half_size;
			shapes.push_back(rect);
		}

		//add circle
		if(GetKey(olc::Key::C).bPressed) {
			auto circ=new SDFCircle{};
			circ->col=olc::PixelF(
				cmn::randFloat(),
				cmn::randFloat(),
				cmn::randFloat()
			);
			circ->ctr=mouse_pos;
			float rad=cmn::randFloat(20, 50);
			circ->edge=mouse_pos+vf2d(rad, 0);
			shapes.push_back(circ);
		}

		//add triangle
		if(GetKey(olc::Key::T).bPressed) {
			auto tri=new SDFTriangle{};
			tri->col=olc::PixelF(
				cmn::randFloat(),
				cmn::randFloat(),
				cmn::randFloat()
			);
			float size=50;
			tri->p0=mouse_pos+polar(size, cmn::randFloat(2*cmn::Pi));
			tri->p1=mouse_pos+polar(size, cmn::randFloat(2*cmn::Pi));
			tri->p2=mouse_pos+polar(size, cmn::randFloat(2*cmn::Pi));
			shapes.push_back(tri);
		}
	}

	//choose handle under mouse
	void handleGrabAction() {
		const auto grab_action=GetMouse(olc::Mouse::LEFT);
		if(grab_action.bPressed) {
			held_pt=nullptr;

			//for each shape
			for(auto& s:shapes) {
				//choose handle under mouse
				auto handles=s->getHandles();
				for(auto& h:handles) {
					if((*h-mouse_pos).mag()<handle_rad) {
						held_pt=h;
						break;
					}
				}
				if(held_pt) break;
			}
		}
		if(grab_action.bHeld) {
			if(held_pt) *held_pt=mouse_pos;
		}
		if(grab_action.bReleased) {
			held_pt=nullptr;
		}
	}

	void handleRemoveAction() {
		if(!GetKey(olc::Key::X).bPressed) return;

		//remove held marker
		held_pt=nullptr;

		//for each shape
		for(auto it=shapes.begin(); it!=shapes.end();) {
			auto& s=*it;

			auto handles=s->getHandles();
			
			//if any handle under mouse
			bool hover=false;
			for(auto& h:handles) {
				if((*h-mouse_pos).mag()<handle_rad) {
					hover=true;
					break;
				}
			}

			//remove it
			if(hover) {
				delete s;
				it=shapes.erase(it);
			} else it++;
		}
	}
#pragma endregion

	void update(float dt) {
		//get mouse
		mouse_pos.x=GetMouseX();
		mouse_pos.y=GetMouseY();
		
		//graphics toggles
		if(GetKey(olc::Key::SPACE).bPressed) combine_union^=true;
		if(GetKey(olc::Key::V).bPressed) render_values^=true;
		if(GetKey(olc::Key::S).bPressed) render_shapes^=true;

		//change resolution?
		if(GetKey(olc::Key::UP).bHeld) cell_sz*=1+dt;
		if(GetKey(olc::Key::DOWN).bHeld) cell_sz*=1-dt;
		cell_sz=std::clamp(cell_sz, min_cell_sz, max_cell_sz);
		
		updateSizing();

		handleAddAction();

		handleGrabAction();

		handleRemoveAction();

		updateGrids();
	}
#pragma region RENDER HELPERS
	void DrawThickLineDecal(const vf2d& a, const vf2d& b, float t, olc::Pixel col) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d norm=sub/len;
		vf2d tang(-norm.y, norm.x);

		float angle=std::atan2(sub.y, sub.x);
		DrawRotatedDecal(a-t/2*tang, prim_rect.Decal(), angle, {0, 0}, {len, t}, col);
	}

	void FillCircleDecal(const vf2d& pos, float rad, olc::Pixel col) {
		vf2d offset(rad, rad);
		vf2d scale{2*rad/prim_circ.Sprite()->width, 2*rad/prim_circ.Sprite()->width};
		DrawDecal(pos-offset, prim_circ.Decal(), scale, col);
	}
	
	//show values as filled rect background
	void renderValues() {
		const auto size=cell_sz*vf2d(1, 1);
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				int k=ix(i, j);
				float x=cell_sz*i;
				float y=cell_sz*j;
				auto col=olc::PixelF(
					col_grid[3*k],
					col_grid[1+3*k],
					col_grid[2+3*k]
				);
				FillRectDecal({x, y}, size, col);
			}
		}
	}

	void renderShapes() {
		for(const auto& s:shapes) {
			s->render(this);
		}
	}

	void renderMarchedSquares(float surf, const olc::Pixel& col) {
		for(int i=0; i<width-1; i++) {
			for(int j=0; j<height-1; j++) {
				const auto& v0=val_grid[ix(i, j)];
				const auto& v1=val_grid[ix(i+1, j)];
				const auto& v2=val_grid[ix(i, j+1)];
				const auto& v3=val_grid[ix(i+1, j+1)];
				const auto& p0=pos_grid[ix(i, j)];
				const auto& p1=pos_grid[ix(i+1, j)];
				const auto& p2=pos_grid[ix(i, j+1)];
				const auto& p3=pos_grid[ix(i+1, j+1)];
				//threshold values against surface
				bool b0=v0>surf, b1=v1>surf, b2=v2>surf, b3=v3>surf;
				//bitbang state
				int state=b3<<3|b2<<2|b1<<1|b0<<0;
				const auto& edges=edge_table[state];
				for(int k=0; k<4; k+=2) {
					if(edges[k]==-1) break;

					vf2d p[2];
					for(int l=0; l<2; l++) {
						switch(edges[k+l]) {
							case 0: p[l]=mix(p0, p1, invLerp(surf, v0, v1)); break;
							case 1: p[l]=mix(p1, p3, invLerp(surf, v1, v3)); break;
							case 2: p[l]=mix(p3, p2, invLerp(surf, v3, v2)); break;
							case 3: p[l]=mix(p2, p0, invLerp(surf, v2, v0)); break;
						}
					}
					DrawThickLineDecal(p[0], p[1], 2, col);
				}
			}
		}
	}

	void renderShapeHandles() {
		for(const auto& s:shapes) {
			auto handles=s->getHandles();
			for(const auto& h:handles) {
				FillCircleDecal(*h, handle_rad, s->col);
			}
		}
	}
#pragma endregion

	void render() {
		FillRectDecal({0, 0}, GetScreenSize(), olc::BLACK);

		if(render_values) renderValues();

		//these look better behind
		if(render_shapes) renderShapes();

		renderMarchedSquares(0, olc::WHITE);

		//these look better in front
		if(render_shapes) renderShapeHandles();
	}

	bool OnUserUpdate(float dt) override {
		update(dt);
		
		render();

		return true;
	}

	MarchingSquares() {
		sAppName="Marching Squares";
	}
};

int main() {
	MarchingSquares tui;
	bool fullscreen=false;
	bool vsync=true;
	bool cohesion=false;
	bool realwindow=true;
	if(tui.Construct(
		720, 480, 1, 1,
		fullscreen,
		vsync,
		cohesion,
		realwindow)) tui.Start();

	return 0;
}