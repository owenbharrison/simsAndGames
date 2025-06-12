#pragma once
#ifndef SCENE_CLASS_H
#define SCENE_CLASS_H

#include "phys/shape.h"
#include "phys/joint.h"

#include <unordered_map>

class Scene {
	void copyFrom(const Scene&), clear();

public:
	//list so they can be removed w/o realloc?
	std::list<Shape> shapes;
	std::list<Joint> joints;
	cmn::AABB3 bounds{vf3d(0, 0, 0), vf3d(1, 1, 1)};

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

	bool saveToFZX(const std::string& filename) {
		std::ofstream file(filename);
		if(file.fail()) return false;

		//shape -> index
		std::unordered_map<const Shape*, int> ids;

		//print each shape
		int id=0;
		for(const auto& s:shapes) {
			//print header
			file<<"shp "<<
				s.getNum()<<' '<<
				s.constraints.size()<<' '<<
				s.tris.size()<<' '<<
				s.edges.size()<<'\n';

			//print particles
			for(int i=0; i<s.getNum(); i++) {
				const auto& p=s.particles[i];
				file<<"  p "<<
					p.pos.x<<' '<<p.pos.y<<' '<<p.pos.z<<' '<<
					p.mass<<' '<<p.locked<<'\n';
			}

			//print constraints
			for(const auto& c:s.constraints) {
				int a=c.a-s.particles;
				int b=c.b-s.particles;
				file<<"  c "<<a<<' '<<b<<' '<<c.rest_len<<'\n';
			}

			//print triangles
			for(const auto& t:s.tris) {
				file<<"  t "<<t.a<<' '<<t.b<<' '<<t.c<<'\n';
			}

			//print edges
			for(const auto& e:s.edges) {
				file<<"  e "<<e.a<<' '<<e.b<<'\n';
			}

			//print fill
			file<<"  f "<<int(s.fill.r)<<' '<<int(s.fill.g)<<' '<<int(s.fill.b)<<'\n';

			//add to ids
			ids[&s]=id++;
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

		file.close();

		return true;
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
				int num_p, num_c, num_t, num_e;
				line_str>>num_p>>num_c>>num_t>>num_e;

				//check counts
				if(num_p<=0) throw std::runtime_error("invalid particle count");
				if(num_c<0) throw std::runtime_error("invalid constraint count");
				if(num_t<0) throw std::runtime_error("invalid triangle count");
				if(num_e<0) throw std::runtime_error("invalid edge count");

				//allocate
				Shape s(num_p);

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
					s.particles[i]=p;
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
					c.a=s.particles+a;
					c.b=s.particles+b;
					line_str>>c.rest_len;
					s.constraints.push_back(c);
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
					s.tris.push_back(it);
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
					s.edges.emplace_back(a, b);
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
					s.fill.r=r;
					s.fill.g=g;
					s.fill.b=b;
				}

				//initialize id
				s.id=id++;
				
				//add
				scene.shapes.push_back(s);
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

			shape.fill.r=rand()%255;
			shape.fill.g=rand()%255;
			shape.fill.b=rand()%255;
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
}

void Scene::clear() {
	shapes.clear();
	joints.clear();
}
#endif