#pragma once
#ifndef SCENE_CLASS_H
#define SCENE_CLASS_H

#include <sstream>
#include <fstream>

#include <exception>

class Scene {
	void copyFrom(const Scene&);

public:
	std::list<Shape*> shapes;
	cmn::AABB phys_bounds{{0, 0}, {1, 1}};
	vf2d gravity{0, 9.8};

	Scene() {}

	//1
	Scene(const Scene& s) {
		copyFrom(s);
	}

	//2
	Scene& operator=(const Scene& s) {
		if(&s==this) return *this;

		clear();

		copyFrom(s);

		return *this;
	}

	//3
	~Scene() {
		clear();
	}

	void clear() {
		for(const auto& s:shapes) delete s;
		shapes.clear();
	}

	void removeShapeAt(const vf2d& pos) {
		for(auto sit=shapes.begin(); sit!=shapes.end();) {
			const auto& s=*sit;
			if(s->contains(pos)) {
				delete s;
				sit=shapes.erase(sit);
			} else sit++;
		}
	}

	void update(float dt) {
		//sanitize?
		for(auto it=shapes.begin(); it!=shapes.end();) {
			if((*it)->getArea()<0) it=shapes.erase(it);
			else it++;
		}

		//shape collision
		for(const auto& a:shapes) {
			for(const auto& b:shapes) {
				if(b==a) continue;

				a->collide(*b);
			}
		}

		//gravity, update, check bounds
		for(const auto& shp:shapes) {
			shp->accelerate(gravity);

			shp->update(dt);

			shp->keepIn(phys_bounds);
		}
	}

	void save(const std::string& filename) const {
		if(filename.empty()) {
			throw std::runtime_error("no filename");
		}

		std::ofstream file_out(filename);
		if(file_out.fail()) {
			throw std::runtime_error("invalid filename");
		}

		//print configuration
		file_out<<"bnd "<<
			phys_bounds.min.x<<' '<<
			phys_bounds.min.y<<' '<<
			phys_bounds.max.x<<' '<<
			phys_bounds.max.y<<'\n';

		//print shapes line by line
		for(const auto& shp:shapes) {
			//shape designation
			//shp <#pts> <#csts> <#sprs>
			file_out<<"shp "<<
				shp->getNum()<<' '<<
				shp->shell.size()<<' '<<
				shp->springs.size()<<'\n';
			//print points
			//p <x> <y> <mass> <?locked>
			for(int i=0; i<shp->getNum(); i++) {
				const auto& p=shp->points[i];
				file_out<<"  p "<<
					p.pos.x<<' '<<
					p.pos.y<<' '<<
					p.mass<<' '<<
					p.locked<<'\n';
			}

			//make lookup table
			std::unordered_map<PointMass*, int> indexes;
			for(int i=0; i<shp->getNum(); i++) {
				indexes[&shp->points[i]]=i;
			}

			//print constraints
			//c <a> <b> <len>
			for(const auto& c:shp->shell) {
				file_out<<"  c "<<
					indexes[c.a]<<' '<<
					indexes[c.b]<<' '<<
					c.len_rest<<'\n';
			}

			//print springs
			//s <a> <b> <len> <stiff> <damp>
			for(const auto& s:shp->springs) {
				file_out<<"  s "<<
					indexes[s.a]<<' '<<
					indexes[s.b]<<' '<<
					s.len_rest<<' '<<
					s.stiffness<<' '<<
					s.damping<<'\n';
			}

			//print anchors
			//a <x> <y>
			for(int i=0; i<shp->getNum(); i++) {
				const auto& a=shp->anchors[i];
				file_out<<"  a "<<
					a.x<<' '<<
					a.y<<'\n';
			}

			//print orientation
			//o <x> <y> <rot> <?anchored>
			file_out<<"  o "<<
				shp->anchor_pos.x<<' '<<
				shp->anchor_pos.y<<' '<<
				shp->anchor_rot<<' '<<
				shp->anchored<<'\n';

			//print tint
			//t <r> <g> <b>
			file_out<<"  t "<<
				int(shp->col.r)<<' '<<
				int(shp->col.g)<<' '<<
				int(shp->col.b)<<'\n';
		}
	}

	static Scene load(std::string& filename) {
		if(filename.empty()) {
			throw std::runtime_error("no filename");
		}

		std::ifstream file_in(filename);
		if(file_in.fail()) {
			throw std::runtime_error("invalid filename");
		}

		Scene scn;
		bool has_bounds=false;

		//parse file line by line
		std::string line;
		while(std::getline(file_in, line)) {
			//get header
			std::stringstream line_str(line);

			//is this a shape?
			std::string type;
			line_str>>type;

			if(type=="bnd") {
				line_str>>
					scn.phys_bounds.min.x>>
					scn.phys_bounds.min.y>>
					scn.phys_bounds.max.x>>
					scn.phys_bounds.max.y;
				has_bounds=true;
			}

			if(type=="shp") {
				//parse header
				int num_pts=-1, num_csts=-1, num_sprs=-1;
				line_str>>
					num_pts>>
					num_csts>>
					num_sprs;
				if(num_pts<=0||num_csts<0||num_sprs<0) {
					throw std::runtime_error("invalid header");
				}

				//construct
				Shape shp(num_pts);

				//points
				for(int i=0; i<num_pts; i++) {
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="p") {
						throw std::runtime_error("expected point");
					}

					//parse data & construct
					PointMass& p=shp.points[i];
					line_str>>
						p.pos.x>>
						p.pos.y>>
						p.mass>>
						p.locked;
					p.oldpos=p.pos;
				}

				//constraints
				for(int i=0; i<num_csts; i++) {
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="c") {
						throw std::runtime_error("expected constraint");
					}

					//parse data
					int a, b;
					float l;
					line_str>>a>>b>>l;
					if(a<0||b<0||a>=num_pts||b>=num_pts) {
						throw std::runtime_error("invalid constraint");
					}

					//construct
					Constraint c;
					c.a=&shp.points[a];
					c.b=&shp.points[b];
					c.len_rest=l;
					shp.shell.push_back(c);
				}

				//springs
				for(int i=0; i<num_sprs; i++) {
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="s") {
						throw std::runtime_error("expected spring");
					}

					//parse data
					int a, b;
					float l, k, d;
					line_str>>a>>b>>l>>k>>d;
					if(a<0||b<0||a>=num_pts||b>=num_pts) {
						throw std::runtime_error("invalid spring");
					}

					//construct
					Spring s;
					s.a=&shp.points[a];
					s.b=&shp.points[b];
					s.len_rest=l;
					s.stiffness=k;
					s.damping=d;
					shp.springs.push_back(s);
				}

				//anchors
				for(int i=0; i<num_pts; i++) {
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="a") {
						throw std::runtime_error("expected anchor");
					}

					//parse data
					vf2d& a=shp.anchors[i];
					line_str>>
						a.x>>
						a.y;
				}

				//orientation
				{
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="o") {
						throw std::runtime_error("expected orientation");
					}

					//parse data
					line_str>>
						shp.anchor_pos.x>>
						shp.anchor_pos.y>>
						shp.anchor_rot>>
						shp.anchored;
				}

				//tint
				{
					std::getline(file_in, line);
					line_str.str(line), line_str.clear();

					//ensure type
					line_str>>type;
					if(type!="t") {
						throw std::runtime_error("expected tint");
					}

					//parse data
					int r, g, b;
					line_str>>r>>g>>b;

					//construct
					shp.col.r=r;
					shp.col.g=g;
					shp.col.b=b;
				}

				//add to list
				scn.shapes.push_back(new Shape(shp));
			}
		}

		//if bounds not provided, shrinkwrap.
		if(!has_bounds) {
			cmn::AABB box;
			for(const auto& s:scn.shapes) {
				cmn::AABB s_box=s->getAABB();
				box.fitToEnclose(s_box.min);
				box.fitToEnclose(s_box.max);
			}
			scn.phys_bounds=box;
		}

		return scn;
	}
};

void Scene::copyFrom(const Scene& scn) {
	for(const auto& s:scn.shapes) {
		shapes.push_back(new Shape(*s));
	}

	phys_bounds=scn.phys_bounds;
	gravity=scn.gravity;
}
#endif