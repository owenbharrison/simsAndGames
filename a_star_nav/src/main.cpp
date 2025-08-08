#include "common/3d/engine_3d.h"
using cmn::vf3d;
using cmn::Mat4;

constexpr float Pi=3.1415927f;

#include "mesh.h"

#include "common/aabb.h"
#include "common/utils.h"
namespace cmn {
	using AABB=AABB_generic<olc::vf2d>;

	olc::vf2d polar(float rad, float angle) {
		return polar_generic<olc::vf2d>(rad, angle);
	}
}

#include "poisson_disc.h"
#include "triangulate.h"

#include <unordered_map>

#include "graph.h"

#include "anim/animated_mesh.h"

float fract(float x) {
	return x-std::floor(x);
}

struct Example : cmn::Engine3D {
	Example() {
		sAppName="A* Navigation";
	}

	//camera positioning
	float cam_yaw=Pi/2;
	float cam_pitch=0;
	vf3d light_pos;

	//ui stuff
	vf3d mouse_dir;
	bool show_graph=true;

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

	bool walking=false;
	const float time_per_seg=1.3f;
	float seg_timer=0;
	int seg_ix=0;

	//super fancy stuff!
	AnimatedMesh knight;

	bool user_create() override {
		cam_pos={0, 7, 0};
		light_pos={0, 45, 0};

		//load error prone assets
		try {
			//color terrain based on slope
			terrain=Mesh::loadFromOBJ("assets/models/terrain.txt");
			olc::Sprite* gradient_spr=new olc::Sprite("assets/img/grass_falloff.png");
			const vf3d up(0, 1, 0);
			for(auto& t:terrain.tris) {
				//get steepness
				float s=up.cross(t.getNorm()).mag();
				t.col=gradient_spr->Sample(s, 0);
			}
			delete gradient_spr;

			//house placement
			Mesh house_model=Mesh::loadFromOBJ("assets/models/house.txt");
			house_model.scale={1.5f, 1.5f, 1.5f};
			struct House { vf3d p; float r; olc::Pixel col; };
			std::vector<House> homes{
				{{3.54f, 1.37f, 18.2f}, .8f*Pi, olc::WHITE},
				{{5.64f, 2.86f, 51.79f}, 1.4f*Pi, olc::BLUE},
				{{51.31f, 1.67f, 41.95f}, .2f*Pi, olc::WHITE},
				{{62.04f, 1.67f, -5.12f}, .6f*Pi, olc::RED},
				{{12.09f, 2.16f, -38.03f}, 1.1f*Pi, olc::WHITE},
				{{-38.35f, 2.63f, -33.38f}, .3f*Pi, olc::GREEN},
				{{-52.99f, 1.43f, 16.84f}, 1.8f*Pi, olc::WHITE}
			};
			for(const auto& h:homes) {
				house_model.translation=h.p;
				house_model.rotation.y=h.r;
				house_model.updateTransforms();
				house_model.updateTriangles(h.col);
				obstacles.push_back(house_model);
			}

			//load animated knight
			knight=AnimatedMesh::load("assets/models/knight", 1, 20);
		} catch(const std::exception& e) {
			std::cout<<"  "<<e.what()<<'\n';
			return false;
		}

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

		//load route marker textures
		texture_atlas.push_back(new olc::Sprite("assets/img/start.png"));
		texture_atlas.push_back(new olc::Sprite("assets/img/end.png"));

		return true;
	}

	bool user_destroy() override {
		for(auto& t:texture_atlas) delete t;

		return true;
	}

	bool user_update(float dt) override {
		//look up, down
		if(GetKey(olc::Key::UP).bHeld) cam_pitch+=dt;
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
		if(GetKey(olc::Key::DOWN).bHeld) cam_pitch-=dt;
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;

		//look left, right
		if(GetKey(olc::Key::LEFT).bHeld) cam_yaw-=dt;
		if(GetKey(olc::Key::RIGHT).bHeld) cam_yaw+=dt;

		//polar to cartesian
		cam_dir=vf3d(
			std::cos(cam_yaw)*std::cos(cam_pitch),
			std::sin(cam_pitch),
			std::sin(cam_yaw)*std::cos(cam_pitch)
		);

		//speed modifier
		float speed=dt;
		if(GetKey(olc::Key::CTRL).bHeld) speed*=3;

		//move up, down
		if(GetKey(olc::Key::SPACE).bHeld) cam_pos.y+=4.f*speed;
		if(GetKey(olc::Key::SHIFT).bHeld) cam_pos.y-=4.f*speed;

		//move forward, backward
		vf3d fb_dir(std::cos(cam_yaw), 0, std::sin(cam_yaw));
		if(GetKey(olc::Key::W).bHeld) cam_pos+=5.f*speed*fb_dir;
		if(GetKey(olc::Key::S).bHeld) cam_pos-=3.f*speed*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(GetKey(olc::Key::A).bHeld) cam_pos+=4.f*speed*lr_dir;
		if(GetKey(olc::Key::D).bHeld) cam_pos-=4.f*speed*lr_dir;

		//set light pos
		if(GetKey(olc::Key::L).bHeld) light_pos=cam_pos;

		//graphics toggles
		if(GetKey(olc::Key::G).bPressed) show_graph^=true;

		//update mouse ray(matrix could be singular)
		try {
			//unprojection matrix
			cmn::Mat4 inv_vp=cmn::Mat4::inverse(mat_view*mat_proj);

			//get ray thru screen mouse pos
			float ndc_x=1-2.f*GetMouseX()/ScreenWidth();
			float ndc_y=1-2.f*GetMouseY()/ScreenHeight();
			vf3d clip(ndc_x, ndc_y, 1);
			vf3d world=clip*inv_vp;
			world/=world.w;
			mouse_dir=(world-cam_pos).norm();
		} catch(const std::exception& e) {}

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
				walking=false;

				if(GetKey(olc::Key::CTRL).bHeld) route=graph.route(from_node, to_node);
			}

			//set to waypoint
			if(GetKey(olc::Key::T).bHeld) {
				to_node=close_node;

				route.clear();
				walking=false;

				if(GetKey(olc::Key::CTRL).bHeld) route=graph.route(from_node, to_node);
			}
		}

		//swap waypoints
		if(GetKey(olc::Key::TAB).bPressed) {
			std::swap(to_node, from_node);

			route.clear();
			walking=false;
		}

		//remove waypoints
		if(GetKey(olc::Key::ESCAPE).bPressed) {
			from_node=nullptr, to_node=nullptr;

			route.clear();
			walking=false;
		}

		//route
		if(GetKey(olc::Key::ENTER).bPressed) {
			route=graph.route(from_node, to_node);

			walking=false;
		}

		anim_timer+=dt;

		//walk
		if(GetKey(olc::Key::R).bPressed) {
			walking=false;

			//only if walkable
			if(route.size()>=2) {
				walking=true;

				seg_ix=0;
				seg_timer=0;
			}
		}

		//walk update
		if(walking) {
			seg_timer+=dt;

			if(seg_timer>time_per_seg) {
				seg_timer=0;

				seg_ix++;
				if(seg_ix==route.size()-1) walking=false;
			}
		}

		return true;
	}

	void makeQuad(vf3d p, float w, float h, float ax, float ay, cmn::Triangle& a, cmn::Triangle& b) {
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

	bool user_geometry() override {
		//add main light
		lights.push_back({light_pos, olc::WHITE});
		
		//add terrain mesh
		tris_to_project.insert(tris_to_project.end(), terrain.tris.begin(), terrain.tris.end());

		//add obstacles
		for(const auto& o:obstacles) {
			tris_to_project.insert(tris_to_project.end(), o.tris.begin(), o.tris.end());
		}

		if(show_graph) {
			//add links as black lines
			for(const auto& n:graph.nodes) {
				for(const auto& o:n->links) {
					cmn::Line l1{n->pos, o->pos}; l1.col=olc::BLACK;
					lines_to_project.push_back(l1);
				}
			}
		}

		//add nodes as billboards
		for(const auto& n:graph.nodes) {
			//default size, anchor, and id values
			float sz=.4f, ay=.5f;
			int id=-1;

			//change if route marker.
			if(n==from_node) sz*=3, ay=1, id=0;
			else if(n==to_node) sz*=3, ay=1, id=1;
			else if(!show_graph) continue;

			//tessellate
			cmn::Triangle t1, t2;
			makeQuad(n->pos, sz, sz, .5f, ay, t1, t2);

			//add
			t1.id=id, t2.id=id;
			tris_to_project.push_back(t1);
			tris_to_project.push_back(t2);
		}

		//place yellow quads along route
		{
			bool is_first=true;
			vf3d prev;
			//silly little walk animation
			float anim=fract(anim_timer);
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
						makeQuad(pt, .3f, .3f, .5f, .5f, t1, t2);
						t1.col=olc::YELLOW, t2.col=olc::YELLOW;
						tris_to_project.push_back(t1);
						tris_to_project.push_back(t2);
					}
				}
				prev=curr;
			}
		}

		//add walk point
		if(walking) {
			//interpolate between route pts
			vf3d a=route[seg_ix]->pos, b=route[1+seg_ix]->pos;
			vf3d ba=b-a;
			float t=seg_timer/time_per_seg;
			vf3d pt=a+t*ba;

			//place knight at pt
			knight.translation=pt;
			//rotate based on segment xz plane projection
			knight.rotation.y=std::atan2(ba.z, ba.x)-Pi/2;
			knight.updateTransforms();
			knight.applyAnimation(t);
			//add animated geometry
			tris_to_project.insert(tris_to_project.end(), knight.tris.begin(), knight.tris.end());
		}

		return true;
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

		return true;
	}
};

int main() {
	Example e;
	if(e.Construct(640, 320, 1, 1, false, true)) e.Start();

	return 0;
}