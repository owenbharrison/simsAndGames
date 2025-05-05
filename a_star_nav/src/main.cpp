/*TODO:
impl ctrl z, y
impl path optimization
impl time-saving routing
impl references(photos)
impl only show whats on screen?
*/

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_TRANSFORMEDVIEW
#include "olcPGEX_TransformedView.h"

#include <list>
#include <fstream>
#include <sstream>

#include "common/utils.h"

#include "graph.h"

#include "common/aabb.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

struct AStarUI : olc::PixelGameEngine {
	AStarUI() {
		sAppName="A* Navigation";
	}

	vf2d scr_mouse_pos, wld_mouse_pos;

	Graph map;

	Node* connect_start=nullptr;
	Node* held_node=nullptr;

	bool selecting=false;
	vf2d selection_start;
	std::list<Node*> selection;

	std::list<Node*> copied;

	Node* route_from=nullptr, * route_to=nullptr;
	std::list<Node*> route;

	float animation=0;

	olc::TransformedView tv;

	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;
	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;

	olc::Sprite* arrow_sprite=nullptr;
	olc::Decal* arrow_decal=nullptr;

	bool show_grid=true;
	const float grid_spacing=20.f;

	bool show_background=false;
	olc::Sprite* background_sprite=nullptr;
	olc::Decal* background_decal=nullptr;

	bool show_ids=false;

	bool OnUserCreate() override {
		tv.Initialise(GetScreenSize());
		tv.SetScaleExtents({.1f, .1f}, {100, 100});
		tv.EnableScaleClamp(true);

		//make some "primitives" to draw with
		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);
		{
			int sz=1024;
			prim_circ_spr=new olc::Sprite(sz, sz);
			SetDrawTarget(prim_circ_spr);
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2);
			SetDrawTarget(nullptr);
			prim_circ_dec=new olc::Decal(prim_circ_spr);
		}

		arrow_sprite=new olc::Sprite("assets/arrow.png");
		arrow_decal=new olc::Decal(arrow_sprite);

		ConsoleCaptureStdOut(true);

		return true;
	}

	bool OnUserDestroy() override {
		removeMarkers();

		delete prim_rect_spr;
		delete prim_rect_dec;
		delete prim_circ_spr;
		delete prim_circ_dec;

		delete arrow_sprite;
		delete arrow_decal;

		delete background_sprite;
		delete background_decal;

		return true;
	}

#pragma region GRAPH HELPERS
	void removeMarkers() {
		connect_start=nullptr;
		held_node=nullptr;
		selection.clear();

		copied.clear();

		route_from=nullptr;
		route_to=nullptr;
		route.clear();
	}

	void zoomToFit() {
		if(map.nodes.size()>1) {
			const unsigned int margin=50;
			cmn::AABB bounds;
			for(const auto& n:map.nodes) {
				bounds.fitToEnclose(n->pos);
			}
			//how many can fit?
			vf2d bounds_size=bounds.max-bounds.min;
			float num_x=ScreenWidth()/bounds_size.x;
			float num_y=ScreenHeight()/bounds_size.y;
			float scale;
			if(num_x>num_y) {//zoom to height
				scale=(ScreenHeight()-2*margin)/bounds_size.y;
			} else {//zoom to width
				scale=(ScreenWidth()-2*margin)/bounds_size.x;
			}
			tv.SetWorldScale({scale, scale});

			//where is the middle of the screen in the world?
			vf2d mid_scr=GetScreenSize()/2;
			vf2d mid_wld=tv.ScreenToWorld(mid_scr);
			//move by delta
			tv.MoveWorldOffset(bounds.getCenter()-mid_wld);
		}
	}
#pragma endregion

	bool update(float dt) {
		//USER INPUT
		scr_mouse_pos=GetMousePos();
		wld_mouse_pos=tv.ScreenToWorld(scr_mouse_pos);
		if(GetKey(olc::Key::SHIFT).bHeld) {
			//snap to nearest half-block
			const float spacing=grid_spacing/2;
			wld_mouse_pos.x=spacing*std::roundf(wld_mouse_pos.x/spacing);
			wld_mouse_pos.y=spacing*std::roundf(wld_mouse_pos.y/spacing);
		}

		if(!IsConsoleShowing()) {
			//selection logic
			if(GetMouse(olc::Mouse::RIGHT).bPressed) {
				for(const auto& s:selection) map.seperate(s);
				selection.clear();

				selecting=true;
				selection_start=wld_mouse_pos;
			}
			if(GetMouse(olc::Mouse::RIGHT).bReleased) {
				cmn::AABB box;
				box.fitToEnclose(selection_start);
				box.fitToEnclose(wld_mouse_pos);
				for(const auto& n:map.nodes) {
					if(box.contains(n->pos)) {
						selection.emplace_back(n);
					}
				}
				selecting=false;
			}

			//drag nodes
			if(GetMouse(olc::Mouse::LEFT).bPressed) {
				held_node=nullptr;
				if(selection.size()) selection_start=wld_mouse_pos;
				else held_node=map.getNode(wld_mouse_pos);
			}
			if(GetMouse(olc::Mouse::LEFT).bHeld) {
				if(selection.size()) {
					vf2d delta=wld_mouse_pos-selection_start;
					for(const auto& s:selection) s->pos+=delta;
					selection_start=wld_mouse_pos;
					route.clear();
				} else if(held_node) {
					held_node->pos=wld_mouse_pos;
					route.clear();
				}
			}
			if(GetMouse(olc::Mouse::LEFT).bReleased) {
				map.seperate(held_node);
				held_node=nullptr;
			}

			//handle panning
			if(GetMouse(olc::Mouse::MIDDLE).bPressed) tv.StartPan(scr_mouse_pos);
			if(GetMouse(olc::Mouse::MIDDLE).bHeld) tv.UpdatePan(scr_mouse_pos);
			if(GetMouse(olc::Mouse::MIDDLE).bReleased) tv.EndPan(scr_mouse_pos);

			//handle zoom
			if(GetMouseWheel()>0) tv.ZoomAtScreenPos(1.07f, GetMousePos());
			if(GetMouseWheel()<0) tv.ZoomAtScreenPos(1/1.07f, GetMousePos());

			if(GetKey(olc::Key::CTRL).bHeld) {
				//select all
				if(GetKey(olc::Key::A).bPressed) selection=map.nodes;
				//copy
				if(GetKey(olc::Key::C).bPressed) {
					copied.clear();
					for(const auto& s:selection) copied.emplace_back(s);
				}
				//paste
				if(GetKey(olc::Key::V).bPressed) {
					//clear selection, reset ids
					selection.clear();
					for(const auto& n:map.nodes) n->id=0;

					//copy all nodes into map
					int id=1;
					for(const auto& c:copied) {
						c->id=id++;
						map.nodes.emplace_back(new Node(*c));
						//update selection to pasted
						selection.emplace_back(map.nodes.back());
					}

					//copy links only if in copied
					for(const auto& c_n:copied) {
						Node* n=map.getNodeById(-c_n->id);
						for(const auto& c_l:c_n->links) {
							bool found=false;
							for(const auto& o:copied) {
								if(o==c_l) {
									found=true;
									break;
								}
							}
							if(found) map.addLink(n, map.getNodeById(-c_l->id));
						}
					}
				}
			} else {
				//visual toggles
				if(GetKey(olc::Key::G).bPressed) show_grid^=true;
				if(GetKey(olc::Key::B).bPressed) {
					show_background^=true;
					//load on first press
					if(show_background&&!background_sprite) {
						background_sprite=new olc::Sprite("assets/boulder.png");
						background_decal=new olc::Decal(background_sprite);
					}
				}
				if(GetKey(olc::Key::I).bPressed) show_ids^=true;

				//zoom to fit?
				if(GetKey(olc::Key::HOME).bPressed) zoomToFit();

				//add nodes
				if(GetKey(olc::Key::A).bPressed) {
					map.addNode(wld_mouse_pos);
				}

				//start new connection
				if(GetKey(olc::Key::C).bPressed) {
					connect_start=map.getNode(wld_mouse_pos);
				}

				//end new connection(add)
				if(GetKey(olc::Key::C).bReleased) {
					Node* connect_end=map.getNode(wld_mouse_pos);
					if(map.addLink(connect_start, connect_end)) route.clear();
					connect_start=nullptr;
				}

				//route selection
				if(GetKey(olc::Key::F).bPressed) {
					Node* from=map.getNode(wld_mouse_pos);
					if(from&&from!=route_to) {
						route_from=from;
						route.clear();
					}
				}
				if(GetKey(olc::Key::T).bPressed) {
					Node* to=map.getNode(wld_mouse_pos);
					if(to&&to!=route_from) {
						route_to=to;
						route.clear();
					}
				}
				if(GetKey(olc::TAB).bPressed) {
					std::swap(route_from, route_to);
					route.clear();
				}
				if(GetKey(olc::Key::ENTER).bPressed) {
					route=map.route(route_from, route_to);
				}

				//deleting
				if(GetKey(olc::Key::X).bHeld) {
					route.clear();
					if(map.removeNodesAt(wld_mouse_pos)) removeMarkers();
					if(map.removeLinksAt(wld_mouse_pos)) removeMarkers();

					//delete all in selection
					for(const auto& s:selection) {
						for(auto nit=map.nodes.begin(); nit!=map.nodes.end();) {
							if(*nit==s) {
								//remove corresponding links
								for(const auto& o:map.nodes) {
									for(auto lit=o->links.begin(); lit!=o->links.end();) {
										if(*lit==s) lit=o->links.erase(lit);
										else lit++;
									}
								}
								nit=map.nodes.erase(nit);
							} else nit++;
						}
						delete s;
					}
					selection.clear();
				}
			}
		}

		//this is awesome
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		animation+=.3f*dt;
		return true;
	}

	bool OnConsoleCommand(const std::string& line) override {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="import") {
			std::string filename;
			line_str>>filename;

			if(map.importAs(filename)) {
				std::cout<<"imported "<<map.getInfo()<<" from "<<filename<<'\n';
				removeMarkers();
				zoomToFit();
			} else std::cout<<"invalid filename\n";
		}

		else if(cmd=="export") {
			std::string filename;
			line_str>>filename;

			if(map.exportAs(filename)) {
				std::cout<<"exported "<<map.getInfo()<<" to "<<filename<<'\n';
				removeMarkers();
			} else std::cout<<"invalid filename\n";
		}

		else if(cmd=="reset") {
			std::cout<<"reset "<<map.getInfo()<<'\n';
			removeMarkers();
			map.clear();
		}

		else if(cmd=="clear") ConsoleClear();

		return true;
	}

#pragma region RENDER HELPERS
	void tvDrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float rad, olc::Pixel col) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=sub.perp()/len;

		float angle=std::atan2f(sub.y, sub.x);
		tv.DrawRotatedDecal(a-rad*tang, prim_rect_dec, angle, {0, 0}, {len, 2*rad}, col);
	}

	void tvFillCircle(const olc::vf2d& pos, float rad, olc::Pixel col) {
		olc::vf2d offset(rad, rad);
		olc::vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		tv.DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	void tvDrawArrow(const vf2d pos, float angle, float rad, olc::Pixel col) {
		static const olc::vi2d arrow_size{
			arrow_decal->sprite->width,
			arrow_decal->sprite->height
		};
		float scl=2*rad/arrow_size.x;
		tv.DrawRotatedDecal(pos, arrow_decal, angle, arrow_size/2, {scl, scl}, col);
	}
#pragma endregion

	bool render() {
		Clear(olc::Pixel(0, 100, 255));
		if(show_background) tv.DrawDecal({0, 0}, background_decal);

		if(show_grid) {
			//screen bounds in world space, snap to nearest
			vf2d tl=tv.GetWorldTL(), br=tv.GetWorldBR();
			int i_s=std::floor(tl.x/grid_spacing), j_s=std::floor(tl.y/grid_spacing);
			int i_e=std::ceil(br.x/grid_spacing), j_e=std::ceil(br.y/grid_spacing);

			//vert
			for(int i=i_s; i<=i_e; i++) {
				float x=grid_spacing*i;
				vf2d top{x, tl.y}, btm{x, br.y};
				if(i%5==0) tvDrawThickLine(top, btm, 1, olc::GREY);
				else tv.DrawLineDecal(top, btm, olc::GREY);
			}

			//horiz
			for(int j=j_s; j<=j_e; j++) {
				float y=grid_spacing*j;
				vf2d lft{tl.x, y}, rgt{br.x, y};
				if(j%5==0) tvDrawThickLine(lft, rgt, 1, olc::GREY);
				else tv.DrawLineDecal(lft, rgt, olc::GREY);
			}
		}

		//show selection box
		if(selecting) {
			cmn::AABB box;
			box.fitToEnclose(selection_start);
			box.fitToEnclose(wld_mouse_pos);

			vf2d size=box.max-box.min;
			tv.FillRectDecal(box.min, size, olc::Pixel(0, 255, 255, 127));
			tv.DrawRectDecal(box.min, size, olc::BLACK);
		}

		//show route markers
		if(route_from) {
			tvFillCircle(route_from->pos, 4*route_from->rad, olc::Pixel(255, 0, 255, 127));
		}
		if(route_to) {
			tvFillCircle(route_to->pos, 4*route_to->rad, olc::Pixel(0, 255, 255, 127));
		}

		//links as lines
		for(const auto& n:map.nodes) {
			for(const auto& l:n->links) {
				float rad=.5f*(n->rad+l->rad);
				tvDrawThickLine(n->pos, l->pos, rad+1, olc::BLACK);
			}
		}
		for(const auto& n:map.nodes) {
			for(const auto& l:n->links) {
				float rad=.5f*(n->rad+l->rad);
				tvDrawThickLine(n->pos, l->pos, rad, olc::WHITE);
			}
		}

		//with direction arrows
		for(const auto& n:map.nodes) {
			for(const auto& l:n->links) {
				float rad=.4f*(n->rad+l->rad);
				vf2d sub=l->pos-n->pos;
				float angle=atan2f(sub.y, sub.x);
				tvDrawArrow(n->pos+.5f*sub, angle, rad, olc::DARK_GREY);
			}
		}

		//draw new connect as line
		if(connect_start) {
			tvDrawThickLine(connect_start->pos, wld_mouse_pos, 1, olc::CYAN);
		}

		//draw route
		if(route.size()) {
			float anim=std::fmod(animation, 1);
			Node* prev=nullptr;
			for(const auto& curr:route) {
				if(prev) {
					//route links
					float t_rad=prev->rad+curr->rad;
					tvDrawThickLine(prev->pos, curr->pos, .5f*t_rad, olc::GREEN);

					//with direction arrows
					vf2d sub=curr->pos-prev->pos;
					float angle=atan2f(sub.y, sub.x);
					tvDrawArrow(prev->pos+anim*sub, angle, .4f*t_rad, olc::BLUE);
					tvDrawArrow(prev->pos+std::fmod(.5f+anim, 1)*sub, angle, .4f*t_rad, olc::BLUE);
				}
				prev=curr;
			}
		}

		//draw nodes as circles
		for(const auto& n:map.nodes) {
			//can this node be visited and left?
			bool out=n->links.size(), in=false;
			for(const auto& o:map.nodes) {
				if(o==n) continue;

				for(const auto& l:o->links) {
					if(l==n) {
						in=true;
						break;
					}
				}
				if(in) break;
			}
			bool valid=in&&out;
			tvFillCircle(n->pos, n->rad+1, olc::BLACK);
			tvFillCircle(n->pos, n->rad, valid?olc::WHITE:olc::RED);
		}

		//show selection
		for(const auto& s:selection) {
			tvFillCircle(s->pos, s->rad, olc::DARK_BLUE);
		}

		//show ids
		if(show_ids) {
			for(const auto& n:map.nodes) {
				auto id_str=std::to_string(n->id);
				float scale=n->rad/8;
				vf2d offset(4*id_str.length(), 4);
				tv.DrawStringDecal(n->pos-scale*offset, id_str, olc::DARK_GREY, {scale, scale});
			}
		}

		//draw route nodes
		for(const auto& n:route) {
			tvFillCircle(n->pos, n->rad+1, olc::GREEN);
		}

		//show route marker text
		if(route_from) {
			tv.DrawStringDecal(route_from->pos+vf2d(-16, 8), "From", olc::BLACK);
		}
		if(route_to) {
			tv.DrawStringDecal(route_to->pos+vf2d(-8, 8), "To", olc::BLACK);
		}

		return true;
	}

	bool OnUserUpdate(float dt) override {
		update(dt);

		render();

		return true;
	}
};

int main() {
	AStarUI asui;
	bool vsync=true;
	if(asui.Construct(800, 600, 1, 1, false, vsync)) asui.Start();

	return 0;
}