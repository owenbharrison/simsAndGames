#define SOKOL_IMPL
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/include/sokol_app.h"
#include "sokol/include/sokol_gfx.h"
#include "sokol/include/sokol_glue.h"

#include "sokol/sokol_engine.h"

#include "sokol/render_utils.h"

//for time
#include <ctime>

#include "sdf_shape.h"

#include "cmn/utils.h"

void insideGradient(float t, float* r, float* g, float* b) {
	static const float cols[][3]{
		{0, 1, 0},//green
		{1, 1, 0},//yellow
		{1, 0, 0}//red
	};
	return cmn::colorGradient(
		cols, sizeof(cols)/sizeof(cols[0]),
		t, r, g, b
	);
}

void outsideGradient(float t, float* r, float* g, float* b) {
	static const float cols[][3]{
		{0, 1, 1},//cyan
		{0, 0, 1},//blue
		{.5f, 0, 1}//purple
	};
	return cmn::colorGradient(
		cols, sizeof(cols)/sizeof(cols[0]),
		t, r, g, b
	);
}

float invLerp(float v, float a, float b) {
	return (v-a)/(b-a);
}

cmn::vf2d mix(const cmn::vf2d& a, const cmn::vf2d& b, float t) {
	return a+t*(b-a);
}

using cmn::vf2d;

struct MarchingSquares : public cmn::SokolEngine {
	const float min_cell_sz=3, max_cell_sz=25;
	float cell_sz=5;
	int width=0, height=0;
	int ix(int i, int j) { return i+width*j; }

	float* val_grid=nullptr;
	vf2d* pos_grid=nullptr;
	float* col_grid=nullptr;

	vf2d mouse_pos;

	std::list<SDFShape*> shapes;
	vf2d* held_pt=nullptr;
	const float handle_rad=6;

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
		const vf2d res(sapp_widthf(), sapp_heightf());

		auto rect=new SDFRectangle{};
		rect->r=1;
		rect->g=0;
		rect->b=0;
		rect->p0=vf2d(.1f, .1f)*res;
		rect->p1=vf2d(.7f, .9f)*res;

		shapes.push_back(rect);
		auto circ=new SDFCircle{};
		rect->r=0;
		rect->g=0;
		rect->b=1;
		circ->ctr=vf2d(.7f, .5f)*res;
		circ->edge=vf2d(.9f, .5f)*res;
		shapes.push_back(circ);
	}

	void setupSGL() {
		sgl_desc_t sgl_desc{};
		sgl_desc.max_commands=1920/2*1080/2;
		sgl_desc.max_vertices=4*sgl_desc.max_commands;
		sgl_setup(sgl_desc);
	}
#pragma endregion

	bool user_create() override {
		app_title="Marching Squares";
		
		std::srand(std::time(0));

		setupShapes();

		setupSGL();

		return true;
	}

#pragma region UPDATE HELPERS
	void handleAddAction() {
		//add rectangle
		if(GetKey(SAPP_KEYCODE_R).pressed) {
			auto rect=new SDFRectangle{};
			rect->r=cmn::randFloat();
			rect->g=cmn::randFloat();
			rect->b=cmn::randFloat();
			vf2d half_size(
				cmn::randFloat(20, 50),
				cmn::randFloat(20, 50)
			);
			rect->p0=mouse_pos-half_size;
			rect->p1=mouse_pos+half_size;
			shapes.push_back(rect);
		}

		//add circle
		if(GetKey(SAPP_KEYCODE_C).pressed) {
			auto circ=new SDFCircle{};
			circ->r=cmn::randFloat();
			circ->g=cmn::randFloat();
			circ->b=cmn::randFloat();
			circ->ctr=mouse_pos;
			float rad=cmn::randFloat(20, 50);
			circ->edge=mouse_pos+vf2d(rad, 0);
			shapes.push_back(circ);
		}

		//add triangle
		if(GetKey(SAPP_KEYCODE_T).pressed) {
			auto tri=new SDFTriangle{};
			tri->r=cmn::randFloat();
			tri->g=cmn::randFloat();
			tri->b=cmn::randFloat();
			float size=50;
			tri->p0=mouse_pos+cmn::polar<vf2d>(size, cmn::randFloat(2*cmn::Pi));
			tri->p1=mouse_pos+cmn::polar<vf2d>(size, cmn::randFloat(2*cmn::Pi));
			tri->p2=mouse_pos+cmn::polar<vf2d>(size, cmn::randFloat(2*cmn::Pi));
			shapes.push_back(tri);
		}
	}

	//choose handle under mouse
	void handleGrabAction() {
		const auto grab_action=GetMouse(SAPP_MOUSEBUTTON_LEFT);
		if(grab_action.pressed) {
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
		if(grab_action.held) {
			if(held_pt) *held_pt=mouse_pos;
		}
		if(grab_action.released) {
			held_pt=nullptr;
		}
	}

	void handleRemoveAction() {
		if(!GetKey(SAPP_KEYCODE_X).pressed) return;

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

	void handleUserInput(float dt) {
		//graphics toggles
		if(GetKey(SAPP_KEYCODE_SPACE).pressed) combine_union^=true;
		if(GetKey(SAPP_KEYCODE_V).pressed) render_values^=true;
		if(GetKey(SAPP_KEYCODE_S).pressed) render_shapes^=true;

		//change resolution?
		if(GetKey(SAPP_KEYCODE_UP).held) cell_sz*=1-dt;
		if(GetKey(SAPP_KEYCODE_DOWN).held) cell_sz*=1+dt;
		cell_sz=std::clamp(cell_sz, min_cell_sz, max_cell_sz);

		handleAddAction();

		handleGrabAction();

		handleRemoveAction();
	}

	void updateSizing() {
		int w=1+sapp_widthf()/cell_sz;
		int h=1+sapp_heightf()/cell_sz;
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
				if(t>0) outsideGradient(t, &r, &g, &b);
				else insideGradient(-t, &r, &g, &b);
			}
		}
	}
#pragma endregion

	bool user_update(float dt) override {
		//get mouse
		mouse_pos.x=GetMouseX();
		mouse_pos.y=GetMouseY();

		handleUserInput(dt);

		updateSizing();

		updateGrids();

		return true;
	}

#pragma region RENDER HELPERS
	//show values as filled rect background
	void renderValues() {
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				int k=ix(i, j);
				float x=cell_sz*i;
				float y=cell_sz*j;
				cmn::fill_rect(
					x, y, cell_sz, cell_sz,
					col_grid[3*k], col_grid[1+3*k], col_grid[2+3*k]
				);
			}
		}
	}

	void renderShapes() {
		for(const auto& s:shapes) {
			s->render();
		}
	}

	void renderMarchedSquares(float surf) {
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
					cmn::draw_thick_line(
						p[0].x, p[0].y, p[1].x, p[1].y,
						2,
						1, 1, 1
					);
				}
			}
		}
	}

	void renderShapeHandles() {
		for(const auto& shp:shapes) {
			auto handles=shp->getHandles();
			for(const auto& h:handles) {
				cmn::fill_circle(
					h->x, h->y, handle_rad,
					shp->r, shp->g, shp->b
				);
			}
		}
	}
#pragma endregion

	bool user_render() override {
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value={0, 0, 0, 1};
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sgl_defaults();

		sgl_matrix_mode_projection();
		sgl_ortho(0, sapp_widthf(), sapp_heightf(), 0, -1, 1);

		if(render_values) renderValues();

		//these look better behind
		if(render_shapes) renderShapes();

		renderMarchedSquares(0);

		//these look better in front
		if(render_shapes) renderShapeHandles();

		sgl_draw();

		sg_end_pass();

		sg_commit();

		return true;
	}
};