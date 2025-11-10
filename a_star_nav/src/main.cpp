#define OLC_PGE_APPLICATION
#include "common/3d/engine_3d.h"
using cmn::vf3d;
using cmn::Mat4;

#include "common/aabb.h"
#include "common/utils.h"
namespace cmn {
	using AABB=AABB_generic<olc::vf2d>;
}

#include "mesh.h"

#include "poisson_disc.h"
#include "triangulate.h"

#include <unordered_map>

#include "graph.h"

struct AStarNavUI : cmn::Engine3D {
	AStarNavUI() {
		sAppName="A* Navigation";
	}

	//camera positioning
	float cam_yaw=cmn::Pi/2;
	float cam_pitch=0;
	vf3d light_pos;

	//ui stuff
	vf3d mouse_dir;
	bool realize_graph=true;

	//scene stuff
	Mesh terrain;
	std::list<Mesh> obstacles;

	Graph graph;

	//route markers
	Node* from_node=nullptr, * to_node=nullptr;
	std::vector<Node*> route;

	//route fanciness
	std::vector<olc::Sprite*> texture_atlas;
	float anim_timer=0;

	bool help_menu=false;

#pragma region SETUP HELPERS
	bool setupTerrain() {
		//load terrain mesh	
		if(!Mesh::loadFromOBJ(terrain, "assets/models/terrain.txt")) {
			std::cout<<"  unable to load terrain\n";

			return false;
		}

		//color terrain based on slope
		olc::Sprite* gradient_spr=new olc::Sprite("assets/img/grass_falloff.png");
		const vf3d up(0, 1, 0);
		for(auto& t:terrain.tris) {
			//get steepness
			float s=up.cross(t.getNorm()).mag();
			t.col=gradient_spr->Sample(s, 0);
		}
		delete gradient_spr;

		return true;
	}

	//house placement
	bool setupObstacles() {
		//load home mesh
		Mesh house_model;
		if(!Mesh::loadFromOBJ(house_model, "assets/models/house.txt")) {
			std::cout<<"  unable to load house\n";

			return false;
		}
		house_model.scale={1.5f, 1.5f, 1.5f};

		//transform and add homes
		struct House { vf3d p; float r; olc::Pixel col; };
		const std::vector<House> homes{
			{{3.54f, 1.37f, 18.2f}, .8f*cmn::Pi, olc::WHITE},
			{{5.64f, 2.86f, 51.79f}, 1.4f*cmn::Pi, olc::BLUE},
			{{51.31f, 1.67f, 41.95f}, .2f*cmn::Pi, olc::WHITE},
			{{62.04f, 1.67f, -5.12f}, .6f*cmn::Pi, olc::RED},
			{{12.09f, 2.16f, -38.03f}, 1.1f*cmn::Pi, olc::WHITE},
			{{-38.35f, 2.63f, -33.38f}, .3f*cmn::Pi, olc::GREEN},
			{{-52.99f, 1.43f, 16.84f}, 1.8f*cmn::Pi, olc::WHITE}
		};

		for(const auto& h:homes) {
			house_model.pos=h.p;
			house_model.rot.y=h.r;
			house_model.updateTransforms();
			house_model.updateTriangles(h.col);
			obstacles.push_back(house_model);
		}

		return true;
	}

	void setupGraph() {
		//randomly sample points on xz plane
		cmn::AABB3 bounds=terrain.getAABB();
		auto xz_pts=poissonDiscSample({{bounds.min.x, bounds.min.z}, {bounds.max.x, bounds.max.z}}, 2);

		//project pts onto terrain
		std::unordered_map<olc::vf2d*, Node*> xz2way;
		for(auto& p:xz_pts) {
			vf3d orig(p.x, bounds.min.y-.1f, p.y);
			vf3d dir(0, 1, 0);
			float dist=terrain.intersectRay(orig, dir);
			graph.nodes.push_back(new Node(orig+(.2f+dist)*dir));
			xz2way[&p]=graph.nodes.back();
		}

		//triangulate and add links
		auto tris=delaunay::triangulate(xz_pts);
		auto edges=delaunay::extractEdges(tris);
		for(const auto& e:edges) {
			auto a=xz2way[&xz_pts[e.p[0]]];
			auto b=xz2way[&xz_pts[e.p[1]]];
			graph.addLink(a, b);
			graph.addLink(b, a);
		}

		//remove any nodes in way of obstacle
		for(auto it=graph.nodes.begin(); it!=graph.nodes.end();) {
			auto& n=*it;
			//check if inside any meshes
			bool blocked=false;
			for(const auto& o:obstacles) {
				if(o.contains(n->pos)) {
					blocked=true;
					break;
				}
			}
			if(blocked) {
				//remove corresponding links
				for(auto& o:graph.nodes) {
					auto oit=std::find(o->links.begin(), o->links.end(), n);
					if(oit!=o->links.end()) o->links.erase(oit);
				}
				//deallocate and remove this node
				delete n;
				it=graph.nodes.erase(it);
			} else it++;
		}
	}
#pragma endregion

	bool user_create() override {
		cam_pos={0, 7, 0};
		light_pos={0, 45, 0};

		if(!setupTerrain()) return false;

		if(!setupObstacles()) return false;

		setupGraph();

		//load route marker textures
		texture_atlas.push_back(new olc::Sprite("assets/img/start.png"));
		texture_atlas.push_back(new olc::Sprite("assets/img/end.png"));

		return true;
	}

	bool user_destroy() override {
		for(auto& t:texture_atlas) delete t;

		return true;
	}

#pragma region UPDATE HELPERS
	void handleCameraLooking(float dt) {
		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>cmn::Pi/2) cam_pitch=cmn::Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-cmn::Pi/2) cam_pitch=.001f-cmn::Pi/2;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw-=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw+=dt;
	}

	void handleCameraMovement(float dt) {
		//move up, down
		if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*dt;
		if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::cos(cam_yaw), 0, std::sin(cam_yaw));
		if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*dt*fb_dir;
		if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*dt*lr_dir;
		if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*dt*lr_dir;
	}

	void handleMouseRay() {
		//unprojection matrix
		cmn::Mat4 inv_vp=cmn::Mat4::inverse(mat_view*mat_proj);

		//get ray thru screen mouse pos
		float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
		float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
		vf3d clip(ndc_x, ndc_y, 1);
		vf3d world=clip*inv_vp;
		world/=world.w;

		mouse_dir=(world-cam_pos).norm();
	}
#pragma endregion

	bool user_update(float dt) override {
		handleCameraLooking(dt);

		//polar to cartesian
		cam_dir=vf3d(
			std::cos(cam_yaw)*std::cos(cam_pitch),
			std::sin(cam_pitch),
			std::sin(cam_yaw)*std::cos(cam_pitch)
		);

		handleCameraMovement(dt);

		handleMouseRay();

		//find node "under" mouse
		float record=-1;
		Node* close_node=nullptr;
		for(auto& n:graph.nodes) {
			vf3d pa=n->pos-cam_pos;
			//infront of camera.
			if(pa.dot(mouse_dir)>0) {
				//sort by perp dist from ray
				float dist=pa.cross(mouse_dir).mag();
				if(record<0||dist<record) {
					record=dist;
					close_node=n;
				}
			}
		}

		if(close_node) {
			//set from waypoint
			if(GetKey(olc::Key::F).bHeld) {
				from_node=close_node;

				route.clear();
			}

			//set to waypoint
			if(GetKey(olc::Key::T).bHeld) {
				to_node=close_node;

				route.clear();
			}
		}

		//swap waypoints
		if(GetKey(olc::Key::TAB).bPressed) {
			std::swap(to_node, from_node);

			route.clear();
		}

		//remove waypoints
		if(GetKey(olc::Key::ESCAPE).bPressed) {
			from_node=nullptr, to_node=nullptr;

			route.clear();
		}

		//route
		if(GetKey(olc::Key::R).bPressed) {
			route=graph.route(from_node, to_node);
		}

		//set light pos
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//graphics toggles
		if(GetKey(olc::Key::G).bPressed) realize_graph^=true;
		if(GetKey(olc::Key::H).bPressed) help_menu^=true;

		anim_timer+=dt;

		return true;
	}

#pragma region GEOMETRY HELPERS
	void realizeQuad(vf3d p, float w, float h, float ax, float ay, cmn::Triangle& a, cmn::Triangle& b) {
		//billboarded to point at camera
		vf3d norm=(p-cam_pos).norm();
		vf3d up(0, 1, 0);
		vf3d rgt=norm.cross(up).norm();
		up=rgt.cross(norm);

		//vertex positioning
		vf3d tl=p-w*ax*rgt+h*ay*up;
		vf3d tr=p+w*(1-ax)*rgt+h*ay*up;
		vf3d bl=p-w*ax*rgt-h*(1-ay)*up;
		vf3d br=p+w*(1-ax)*rgt-h*(1-ay)*up;

		//texture coords
		cmn::v2d tl_t{0, 0};
		cmn::v2d tr_t{1, 0};
		cmn::v2d bl_t{0, 1};
		cmn::v2d br_t{1, 1};

		//tessellation
		a={tl, br, tr, tl_t, br_t, tr_t};
		b={tl, bl, br, tl_t, bl_t, br_t};
	}

	void realizeLinks(const olc::Pixel& col) {
		//add links as lines
		for(const auto& n:graph.nodes) {
			for(const auto& o:n->links) {
				cmn::Line l1{n->pos, o->pos}; l1.col=col;
				lines_to_project.push_back(l1);
			}
		}
	}

	//add nodes as billboards
	void realizeNodes(float size, const olc::Pixel& col) {
		for(const auto& n:graph.nodes) {
			//default size, anchor, and id values
			float ay=.5f;
			int id=-1;

			//change if route marker.
			float sz=size;
			if(n==from_node) sz*=3, ay=1, id=0;
			else if(n==to_node) sz*=3, ay=1, id=1;
			else if(!realize_graph) continue;

			//tessellate
			cmn::Triangle t1, t2;
			realizeQuad(n->pos, sz, sz, .5f, ay, t1, t2);

			//add
			t1.id=id, t2.id=id;
			t1.col=col, t2.col=col;
			tris_to_project.push_back(t1);
			tris_to_project.push_back(t2);
		}
	}

	//place billboards along route
	void realizeRoute(float sz, const olc::Pixel& col) {
		bool is_first=true;
		vf3d prev;
		//silly little walk animation
		//  take fractional part of anim_timer
		float anim=anim_timer-int(anim_timer);
		for(const auto& r:route) {
			vf3d curr=r->pos;
			if(is_first) is_first=false;
			else {
				const int num=2;
				vf3d ba=curr-prev;
				for(int i=0; i<num; i++) {
					//interpolate between route pts
					float t=(anim+i)/num;
					vf3d pt=prev+t*ba;

					cmn::Triangle t1, t2;
					realizeQuad(pt, sz, sz, .5f, .5f, t1, t2);
					t1.col=col, t2.col=col;
					tris_to_project.push_back(t1);
					tris_to_project.push_back(t2);
				}
			}
			prev=curr;
		}
	}
#pragma endregion

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});

		//add terrain mesh
		tris_to_project.insert(tris_to_project.end(), terrain.tris.begin(), terrain.tris.end());

		//add obstacles
		for(const auto& o:obstacles) {
			tris_to_project.insert(tris_to_project.end(), o.tris.begin(), o.tris.end());
		}

		if(realize_graph) realizeLinks(olc::BLACK);

		realizeNodes(.5f, olc::WHITE);

		realizeRoute(.3f, olc::YELLOW);

		return true;
	}

	void renderHelpHints() {
		int cx=ScreenWidth()/2;
		if(help_menu) {
			DrawString(8, 8, "Movement Controls");
			DrawString(8, 16, "WASD, Space, & Shift to move");
			DrawString(8, 24, "ARROWS to look around");

			DrawString(ScreenWidth()-8*17, 8, "General Controls");
			DrawString(ScreenWidth()-8*15, 16, "F for set from", from_node?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*13, 24, "T for set to", to_node?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*13, 32, "Tab for swap");
			DrawString(ScreenWidth()-8*12, 40, "R for route", route.size()?olc::WHITE:olc::RED);
			DrawString(ScreenWidth()-8*14, 48, "ESC for clear");
			DrawString(ScreenWidth()-8*17, 56, "G for graph view", realize_graph?olc::WHITE:olc::RED);

			DrawString(cx-4*18, ScreenHeight()-8, "[Press H to close]");
		} else {
			DrawString(cx-4*18, ScreenHeight()-8, "[Press H for help]");
		}
	}

	bool user_render() override {
		Clear(olc::Pixel(60, 60, 60));

		//render 3d stuff
		resetBuffers();

		for(const auto& t:tris_to_draw) {
			if(t.id==-1) {
				FillDepthTriangle(
					t.p[0].x, t.p[0].y, t.t[0].w,
					t.p[1].x, t.p[1].y, t.t[1].w,
					t.p[2].x, t.p[2].y, t.t[2].w,
					t.col, t.id
				);
			} else {
				SetPixelMode(olc::Pixel::Mode::ALPHA);
				FillTexturedDepthTriangle(
					t.p[0].x, t.p[0].y, t.t[0].u, t.t[0].v, t.t[0].w,
					t.p[1].x, t.p[1].y, t.t[1].u, t.t[1].v, t.t[1].w,
					t.p[2].x, t.p[2].y, t.t[2].u, t.t[2].v, t.t[2].w,
					texture_atlas[t.id], olc::WHITE, t.id
				);
				SetPixelMode(olc::Pixel::Mode::NORMAL);
			}
		}

		for(const auto& l:lines_to_draw) {
			DrawDepthLine(
				l.p[0].x, l.p[0].y, l.t[0].w,
				l.p[1].x, l.p[1].y, l.t[1].w,
				l.col, l.id
			);
		}

		renderHelpHints();

		return true;
	}
};

int main() {
	AStarNavUI asnui;
	if(asnui.Construct(640, 320, 1, 1, false, true)) asnui.Start();

	return 0;
}