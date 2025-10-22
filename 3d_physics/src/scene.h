#pragma once
#ifndef SCENE_CLASS_H
#define SCENE_CLASS_H

#include "phys/shape.h"
#include "phys/joint.h"

#include <unordered_map>

class Scene {
	int curr_id=0;
	
	void copyFrom(const Scene&), clear();

public:
	//list so they can be removed w/o realloc?
	std::list<Shape> shapes;
	std::list<Joint> joints;
	cmn::AABB3 bounds{
		{-INFINITY, -INFINITY, -INFINITY},
		{INFINITY, INFINITY, INFINITY}
	};
	vf3d gravity{0, -9.8f, 0};

	Scene() {}

	//1
	Scene(const Scene& s) {
		copyFrom(s);
	}

	//2
	~Scene() {
		clear();
	}

	//3
	Scene& operator=(const Scene& s) {
		if(&s==this) return *this;

		clear();

		copyFrom(s);

		return *this;
	}

	void addShape(Shape& s) {
		s.id=curr_id++;
		shapes.push_back(s);
	}

	//set scene bounds such that they fit all shapes
	void shrinkWrap(float margin) {
		cmn::AABB3 box;
		for(const auto& s:shapes) {
			cmn::AABB3 s_box=s.getAABB();
			box.fitToEnclose(s_box.min);
			box.fitToEnclose(s_box.max);
		}
		bounds.min=box.min-margin;
		bounds.max=box.max+margin;
	}

	//particle-triangle collisions
	//keep a's particles out of b's tris
	void triCollide(Shape& shp_a, Shape& shp_b) {
		//find joint conflicts
		const std::list<int>* skip_ix=nullptr;
		for(const auto& j:joints) {
			if(j.shp_a==&shp_a&&j.shp_b==&shp_b) {
				skip_ix=&j.ix_a;
				break;
			}
			if(j.shp_a==&shp_b&&j.shp_b==&shp_a) {
				skip_ix=&j.ix_b;
				break;
			}
		}

		//for every particle in a
		for(int i=0; i<shp_a.getNum(); i++) {
			//skip jointed particles
			if(skip_ix) {
				bool to_skip=false;
				for(const auto& j:*skip_ix) {
					if(j==i) {
						to_skip=true;
						break;
					}
				}
				if(to_skip) continue;
			}

			//check particle against all tris
			auto& ap=shp_a.particles[i];
			for(const auto& bit:shp_b.tris) {
				//realize triangle
				auto& bp0=shp_b.particles[bit.a];
				auto& bp1=shp_b.particles[bit.b];
				auto& bp2=shp_b.particles[bit.c];

				//find close point on triangle
				cmn::Triangle bt{bp0.pos, bp1.pos, bp2.pos};
				vf3d close_pt=bt.getClosePt(ap.pos);

				//is it too close to surface?
				float mag2=(ap.pos-close_pt).mag2();
				if(mag2<Particle::rad*Particle::rad) {
					//where should it be?
					vf3d norm=bt.getNorm();
					vf3d new_pt=close_pt+Particle::rad*norm;
					vf3d delta=new_pt-ap.pos;

					//find contributions
					float m1a=1/ap.mass;
					float m1b0=1/bp0.mass;
					float m1b1=1/bp1.mass;
					float m1b2=1/bp2.mass;
					float m1t=m1a+m1b0+m1b1+m1b2;

					//push apart
					if(!ap.locked) ap.pos+=m1a/m1t*delta;
					//barycentric weights next?
					if(!bp0.locked) bp0.pos-=m1b0/m1t*delta;
					if(!bp1.locked) bp1.pos-=m1b1/m1t*delta;
					if(!bp2.locked) bp2.pos-=m1b2/m1t*delta;
					break;
				}
			}
		}
	}

	//edge-edge collisions
	//push edges apart if too close
	void edgeCollide(Shape& shp_a, Shape& shp_b) {
		//find joint conflicts
		const std::list<int>* skip_ix_a=nullptr, * skip_ix_b=nullptr;
		for(const auto& j:joints) {
			if(j.shp_a==&shp_a&&j.shp_b==&shp_b) {
				skip_ix_a=&j.ix_a, skip_ix_b=&j.ix_b;
				break;
			}
			if(j.shp_a==&shp_b&&j.shp_b==&shp_a) {
				skip_ix_a=&j.ix_b, skip_ix_b=&j.ix_a;
				break;
			}
		}

		//for every edge of the one shape
		for(const auto& ea:shp_a.edges) {
			//skip jointed edges
			if(skip_ix_a) {
				bool to_skip=false;
				for(const auto& i:*skip_ix_a) {
					if(i==ea.a||i==ea.b) {
						to_skip=true;
						break;
					}
				}
				if(to_skip) break;
			}

			auto& a1=shp_a.particles[ea.a];
			auto& a2=shp_a.particles[ea.b];

			//check every edge of the other shape
			for(const auto& eb:shp_b.edges) {
				//skip jointed edges
				if(skip_ix_b) {
					bool to_skip=false;
					for(const auto& i:*skip_ix_b) {
						if(i==eb.a||i==eb.b) {
							to_skip=true;
							break;
						}
					}
					if(to_skip) break;
				}

				auto& b1=shp_b.particles[eb.a];
				auto& b2=shp_b.particles[eb.b];

				//find close points on both edges
				vf3d u=a2.pos-a1.pos;
				vf3d v=b2.pos-b1.pos;
				vf3d w=a1.pos-b1.pos;
				float a=u.dot(u);
				float b=u.dot(v);
				float c=v.dot(v);
				float d=u.dot(w);
				float e=v.dot(w);
				float D=a*c-b*b;
				float s, t;
				if(std::abs(D)<1e-6f) s=0, t=b>c?d/b:e/c;
				else {
					s=std::max(0.f, std::min(1.f, (b*e-c*d)/D));
					t=std::max(0.f, std::min(1.f, (a*e-b*d)/D));
				};
				vf3d close_a=a1.pos+s*u;
				vf3d close_b=b1.pos+t*v;

				//are they too close to eachother?
				vf3d sub=close_a-close_b;
				float mag_sq=sub.mag2();
				if(mag_sq<Particle::rad*Particle::rad) {
					//find contributions based on inverse mass
					float m1a1=1/a1.mass;
					float m1a2=1/a2.mass;
					float m1b1=1/b1.mass;
					float m1b2=1/b2.mass;
					float m1t=m1a1+m1a2+m1b1+m1b2;

					//calculate resolution
					float mag=std::sqrt(mag_sq);
					vf3d norm=sub/mag;
					float delta=Particle::rad-mag;
					vf3d correct=delta*norm;

					//push apart
					if(!a1.locked) a1.pos+=(1-s)*m1a1/m1t*correct;
					if(!a2.locked) a2.pos+=s*m1a2/m1t*correct;
					if(!b1.locked) b1.pos-=(1-t)*m1b1/m1t*correct;
					if(!b2.locked) b2.pos-=t*m1b2/m1t*correct;
				}
			}
		}
	}

	void handleCollisions() {  
		//find potential matches
		for(auto ait=shapes.begin(); ait!=shapes.end(); ait++) {
			for(auto bit=std::next(ait); bit!=shapes.end(); bit++) {
				//predicated by bounding box
				cmn::AABB3 a_box=ait->getAABB();
				a_box.min-=Particle::rad, a_box.max+=Particle::rad;
				cmn::AABB3 b_box=bit->getAABB();
				b_box.min-=Particle::rad, b_box.max+=Particle::rad;
				if(a_box.overlaps(b_box)) {
					triCollide(*ait, *bit);
					triCollide(*bit, *ait);
					edgeCollide(*ait, *bit);
				}
			}
		}
	}

	void update(float dt) {
		//update joints
		for(auto& j:joints) {
			j.update();
		}

		//integration
		for(auto& s:shapes) {
			for(int i=0; i<s.getNum(); i++) {
				s.particles[i].accelerate(gravity);
			}
			s.update(dt);
			s.keepIn(bounds);
		}
	}

	void saveToFZX(const std::string& filename) {
		if(filename.empty()) throw std::runtime_error("no filename");

		std::ofstream file(filename);
		if(file.fail()) throw std::runtime_error("invalid filename");

		//shape -> index
		std::unordered_map<const Shape*, int> ids;

		//print each shape
		int id=0;
		for(const auto& shp:shapes) {
			//print header
			file<<"shp "<<
				shp.getNum()<<' '<<
				shp.constraints.size()<<' '<<
				shp.springs.size()<<' '<<
				shp.tris.size()<<' '<<
				shp.edges.size()<<'\n';

			//print particles
			for(int i=0; i<shp.getNum(); i++) {
				const auto& p=shp.particles[i];
				file<<"  p "<<
					p.pos.x<<' '<<p.pos.y<<' '<<p.pos.z<<' '<<
					p.mass<<' '<<p.locked<<'\n';
			}

			//print constraints
			for(const auto& c:shp.constraints) {
				int a=c.a-shp.particles;
				int b=c.b-shp.particles;
				file<<"  c "<<a<<' '<<b<<' '<<c.rest_len<<'\n';
			}

			//print springs
			for(const auto& s:shp.springs) {
				int a=s.a-shp.particles;
				int b=s.b-shp.particles;
				file<<"  s "<<a<<' '<<b<<' '<<s.rest_len<<' '<<s.stiffness<<' '<<s.damping<<'\n';
			}

			//print triangles
			for(const auto& t:shp.tris) {
				file<<"  t "<<t.a<<' '<<t.b<<' '<<t.c<<'\n';
			}

			//print edges
			for(const auto& e:shp.edges) {
				file<<"  e "<<e.a<<' '<<e.b<<'\n';
			}

			//print fill
			file<<"  f "<<int(shp.fill.r)<<' '<<int(shp.fill.g)<<' '<<int(shp.fill.b)<<'\n';

			//add to ids
			ids[&shp]=id++;
		}

		//print each joint
		for(const auto& j:joints) {
			file<<"jnt "<<ids[j.shp_a]<<' '<<ids[j.shp_b]<<'\n';
			file<<"  a";
			for(const auto& ia:j.ix_a) file<<' '<<ia;
			file<<'\n';
			file<<"  b";
			for(const auto& ib:j.ix_b) file<<' '<<ib;
			file<<'\n';
		}

		//print bounds
		file<<"bnd\n"<<
			"  a "<<bounds.min.x<<' '<<bounds.min.y<<' '<<bounds.min.z<<'\n'<<
			"  b "<<bounds.max.x<<' '<<bounds.max.y<<' '<<bounds.max.z<<'\n';

		//print gravity
		file<<"grv "<<gravity.x<<' '<<gravity.y<<' '<<gravity.z<<'\n';

		file.close();
	}

	static Scene loadFromFZX(const std::string& filename) {
		std::ifstream file(filename);
		if(file.fail()) throw std::runtime_error("invalid filename");

		Scene scene;
		int id=0;

		//index -> shape
		std::vector<Shape*> shape_ptrs;

		//read file line by line
		for(std::string line; std::getline(file, line);) {
			//parse type
			std::stringstream line_str(line);
			std::string type; line_str>>type;

			if(type=="shp") {
				//parse shape header
				int num_p, num_c, num_s, num_t, num_e;
				line_str>>num_p>>num_c>>num_s>>num_t>>num_e;

				//check counts
				if(num_p<=0) throw std::runtime_error("invalid particle count");
				if(num_c<0) throw std::runtime_error("invalid constraint count");
				if(num_s<0) throw std::runtime_error("invalid spring count");
				if(num_t<0) throw std::runtime_error("invalid triangle count");
				if(num_e<0) throw std::runtime_error("invalid edge count");

				//allocate
				Shape shp(num_p);

				//parse particles
				for(int i=0; i<num_p; i++) {
					std::getline(file, line);
					line_str.str(line), line_str.clear();

					//check
					char elem; line_str>>elem;
					if(elem!='p') throw std::runtime_error("expected particle");

					//add
					Particle p;
					line_str>>p.pos.x>>p.pos.y>>p.pos.z>>p.mass>>p.locked;
					p.old_pos=p.pos;
					shp.particles[i]=p;
				}

				//parse constraints
				for(int i=0; i<num_c; i++) {
					std::getline(file, line);
					line_str.str(line), line_str.clear();

					//check
					char elem; line_str>>elem;
					if(elem!='c') throw std::runtime_error("expected constraint");

					//check
					int a=0, b=0;
					line_str>>a;
					if(a<0||a>=num_p) throw std::runtime_error("invalid particle index");
					line_str>>b;
					if(b<0||b>=num_p) throw std::runtime_error("invalid particle index");

					//add
					Constraint c;
					c.a=shp.particles+a;
					c.b=shp.particles+b;
					line_str>>c.rest_len;
					shp.constraints.push_back(c);
				}

				//parse springs
				for(int i=0; i<num_s; i++) {
					std::getline(file, line);
					line_str.str(line), line_str.clear();

					//check
					char elem; line_str>>elem;
					if(elem!='s') throw std::runtime_error("expected spring");

					//check
					int a=0, b=0;
					line_str>>a;
					if(a<0||a>=num_p) throw std::runtime_error("invalid particle index");
					line_str>>b;
					if(b<0||b>=num_p) throw std::runtime_error("invalid particle index");

					//add
					Spring s;
					s.a=shp.particles+a;
					s.b=shp.particles+b;
					line_str>>s.rest_len>>s.stiffness>>s.damping;
					shp.springs.push_back(s);
				}

				//parse triangles
				for(int i=0; i<num_t; i++) {
					std::getline(file, line);
					line_str.str(line), line_str.clear();

					//check
					char elem; line_str>>elem;
					if(elem!='t') throw std::runtime_error("expected triangle");

					//check
					IndexTriangle it;
					line_str>>it.a;
					if(it.a<0||it.a>=num_p) throw std::runtime_error("invalid particle index");
					line_str>>it.b;
					if(it.b<0||it.b>=num_p) throw std::runtime_error("invalid particle index");
					line_str>>it.c;
					if(it.c<0||it.c>=num_p) throw std::runtime_error("invalid particle index");

					//add
					shp.tris.push_back(it);
				}

				//parse edges
				for(int i=0; i<num_e; i++) {
					std::getline(file, line);
					line_str.str(line), line_str.clear();

					//check
					char elem; line_str>>elem;
					if(elem!='e') throw std::runtime_error("expected edge");

					//check
					int a=0, b=0;
					line_str>>a;
					if(a<0||a>=num_p) throw std::runtime_error("invalid particle index");
					line_str>>b;
					if(b<0||b>=num_p) throw std::runtime_error("invalid particle index");

					//add
					shp.edges.emplace_back(a, b);
				}

				//parse fill
				{
					std::getline(file, line);
					line_str.str(line), line_str.clear();

					//check
					char elem; line_str>>elem;
					if(elem!='f') throw std::runtime_error("expected fill");

					//add
					int r=0, g=0, b=0;
					line_str>>r>>g>>b;
					shp.fill.r=r;
					shp.fill.g=g;
					shp.fill.b=b;
				}

				//initialize id
				shp.id=id++;

				//add
				scene.shapes.push_back(shp);
				shape_ptrs.push_back(&scene.shapes.back());
			}

			//parse joint
			else if(type=="jnt") {
				//check
				int a=0, b=0;
				line_str>>a>>b;
				if(a<0||b<0) throw std::runtime_error("invalid shape index");

				Joint j;
				j.shp_a=shape_ptrs[a];
				j.shp_b=shape_ptrs[b];

				std::getline(file, line);
				line_str.str(line), line_str.clear();

				//check
				char elem; line_str>>elem;
				if(elem!='a') throw std::runtime_error("expected an a");

				for(int ia; line_str>>ia;) j.ix_a.push_back(ia);

				std::getline(file, line);
				line_str.str(line), line_str.clear();

				//check
				line_str>>elem;
				if(elem!='b') throw std::runtime_error("expected a b");

				for(int ib; line_str>>ib;) j.ix_b.push_back(ib);

				//add
				scene.joints.push_back(j);
			}

			//parse bounds
			else if(type=="bnd") {
				std::getline(file, line);
				line_str.str(line), line_str.clear();

				//check
				char elem; line_str>>elem;
				if(elem!='a') throw std::runtime_error("expected an a");

				line_str>>scene.bounds.min.x>>scene.bounds.min.y>>scene.bounds.min.z;

				std::getline(file, line);
				line_str.str(line), line_str.clear();

				//check
				line_str>>elem;
				if(elem!='b') throw std::runtime_error("expected a b");

				line_str>>scene.bounds.max.x>>scene.bounds.max.y>>scene.bounds.max.z;
			}

			//parse gravity
			else if(type=="grv") {
				line_str>>scene.gravity.x>>scene.gravity.y>>scene.gravity.z;
			}
		}

		return scene;
	}

	static Scene loadFromOBJ(const std::string& filename) {
		std::ifstream file(filename);
		if(file.fail()) throw std::runtime_error("invalid filename");

		//parse obj file line by line
		std::vector<Particle> particles;
		std::vector<IndexTriangle> tris;
		for(std::string line; std::getline(file, line);) {
			std::stringstream line_str(line);
			std::string type; line_str>>type;

			if(type=="v") {
				//add particles
				vf3d v;
				line_str>>v.x>>v.y>>v.z;
				particles.emplace_back(v);
			} else if(type=="f") {
				//parse v/t/n until fail
				std::vector<int> v_ixs;
				int num=0;
				for(std::string vtn; line_str>>vtn; num++) {
					std::stringstream vtn_str(vtn);
					int v_ix;
					if(vtn_str>>v_ix) v_ixs.push_back(v_ix-1);
				}

				//triangulate
				for(int i=2; i<num; i++) {
					tris.push_back({v_ixs[0], v_ixs[i-1], v_ixs[i]});
				}
			}
		}

		//find neighbors(adjacency list)
		std::vector<std::unordered_set<int>> adjacency(particles.size());
		for(const auto& t:tris) {
			adjacency[t.a].insert(t.b), adjacency[t.a].insert(t.c);
			adjacency[t.b].insert(t.c), adjacency[t.b].insert(t.a);
			adjacency[t.c].insert(t.a), adjacency[t.c].insert(t.b);
		}

		//floodfill to find connected particle groups
		std::vector<bool> visited(particles.size(), false);
		std::vector<std::vector<int>> particle_groups;
		for(int i=0; i<particles.size(); i++) {
			if(visited[i]) continue;

			std::vector<int> group;
			std::stack<int> stack;
			stack.push(i);

			while(!stack.empty()) {
				int v=stack.top(); stack.pop();
				if(visited[v]) continue;

				visited[v]=true;
				group.push_back(v);

				for(const auto& neighbor:adjacency[v]) {
					if(!visited[neighbor]) {
						stack.push(neighbor);
					}
				}
			}

			particle_groups.push_back(group);
		}

		//construct shapes
		Scene scene;
		int id=0;
		for(const auto& group:particle_groups) {
			//index mapping
			std::unordered_map<int, int> global_to_local;
			for(int j=0; j<group.size(); j++) {
				global_to_local[group[j]]=j;
			}

			//extract particles
			Shape shape(group.size());
			int i=0;
			for(const auto& idx:group) {
				shape.particles[i++]=particles[idx];
			}

			shape.initConstraints();

			//extract relevant tris and remap indexes
			for(const auto& tri:tris) {
				if(global_to_local.count(tri.a)&&
					global_to_local.count(tri.b)&&
					global_to_local.count(tri.c)) {
					shape.tris.push_back({
						global_to_local[tri.a],
						global_to_local[tri.b],
						global_to_local[tri.c]
						});
				}
			}

			shape.initEdges();

			shape.initMass();

			shape.fill.r=std::rand()%256;
			shape.fill.g=std::rand()%256;
			shape.fill.b=std::rand()%256;
			shape.id=id++;

			scene.shapes.push_back(shape);
		}

		return scene;
	}
};

void Scene::copyFrom(const Scene& scn) {
	//copy over shapes & init pointer lookup
	std::unordered_map<const Shape*, Shape*> scn_to_me;
	for(const auto& s:scn.shapes) {
		shapes.push_back(s);
		scn_to_me[&s]=&shapes.back();
	}

	//copy over joints
	for(const auto& j:scn.joints) {
		joints.push_back({
			scn_to_me[j.shp_a],
			j.ix_a,
			scn_to_me[j.shp_b],
			j.ix_b
			});
	}

	//copy bounds
	bounds=scn.bounds;

	//copy id
	curr_id=scn.curr_id;
}

void Scene::clear() {
	shapes.clear();
	joints.clear();
}
#endif