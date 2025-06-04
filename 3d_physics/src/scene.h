#pragma once
#ifndef SCENE_CLASS_H
#define SCENE_CLASS_H

#include "phys/shape.h"
#include "phys/joint.h"

#include <fstream>
#include <sstream>
#include <exception>

#include <unordered_map>

struct Scene {
	//list so they can be removed w/o realloc?
	std::list<Shape> shapes;
	std::list<Joint> joints;

	bool save(const std::string& filename) {
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

		file.close();

		return true;
	}

	static Scene load(const std::string& filename) {
		std::ifstream file(filename);
		if(file.fail()) throw std::runtime_error("invalid filename");

		Scene scene;

		//index -> shape
		std::vector<Shape*> shapes;
		
		//read file line by line
		for(std::string line; std::getline(file, line);) {
			//parse type
			std::stringstream line_str(line);
			std::string type; line_str>>type;

			if(type=="shp") {
				//parse shape header
				int num_p, num_c, num_t, num_e;
				line_str>>num_p>>num_c>>num_t>>num_e;

				//check nums
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
					line_str>>a>>b;
					if(a<0||b<0) throw std::runtime_error("invalid particle index");

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
					line_str>>it.a>>it.b>>it.c;
					if(it.a<0||it.b<0||it.c<0) throw std::runtime_error("invalid particle index");

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
					line_str>>a>>b;
					if(a<0||b<0) throw std::runtime_error("invalid particle index");

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

				//add
				scene.shapes.push_back(s);
				shapes.push_back(&scene.shapes.back());
			}

			//parse joint
			else if(type=="jnt") {
				//check
				int a=0, b=0;
				line_str>>a>>b;
				if(a<0||b<0) throw std::runtime_error("invalid shape index");

				Joint j;
				j.shp_a=shapes[a];
				j.shp_b=shapes[b];

				//check
				std::getline(file, line);
				line_str.str(line), line_str.clear();
				char elem; line_str>>elem;
				if(elem!='a') throw std::runtime_error("expected an a");
				for(int ia; line_str>>ia;) j.ix_a.push_back(ia);

				//check
				std::getline(file, line);
				line_str.str(line), line_str.clear();
				line_str>>elem;
				if(elem!='b') throw std::runtime_error("expected a b");
				for(int ib; line_str>>ib;) j.ix_b.push_back(ib);

				//add
				scene.joints.push_back(j);
			}
		}

		return scene;
	}
};
#endif