#pragma once
#ifndef GRAPH_CLASS_H
#define GRAPH_CLASS_H

#include "node.h"

#include <list>
#include <string>
#include <fstream>
#include <sstream>

struct Graph {
	std::list<Node*> nodes;

	Graph() {}

	//this will change g's nodes' ids.
	void copyFrom(const Graph& g) {
		//copy over nodes
		int id=1;
		for(const auto& n:g.nodes) {
			n->id=id++;
			nodes.emplace_back(new Node(*n));
		}

		//copy over links
		for(const auto& g_n:g.nodes) {
			Node* n=getNodeById(g_n->id);
			for(const auto& g_l:g_n->links) {
				Node* l=getNodeById(g_n->id);
				addLink(n, l);
			}
		}
	}

	//1
	Graph(const Graph& g) {
		copyFrom(g);
	}

	void clear() {
		for(const auto& n:nodes) delete n;
		nodes.clear();
	}
	
	//2
	~Graph() {
		clear();
	}

	//3
	Graph& operator=(const Graph& g) {
		if(&g==this) return *this;

		clear();

		copyFrom(g);

		return *this;
	}

	Node* getNode(const vf2d& pos) const {
		for(const auto& n:nodes) {
			if((n->pos-pos).mag2()<n->rad*n->rad) {
				return n;
			}
		}

		return nullptr;
	}

	Node* getNodeById(int id) const {
		for(const auto& n:nodes) {
			if(n->id==id) return n;
		}
		return nullptr;
	}

	bool contains(Node* o) const {
		if(!o) return false;

		for(const auto& n:nodes) {
			if(n==o) return true;
		}
		return false;
	}

	bool addNode(const vf2d& pos) {
		//inside another node?
		if(getNode(pos)) return false;

		nodes.emplace_back(new Node(pos));

		return true;
	}

	bool addLink(Node* a, Node* b) {
		if(!a||!b||a==b) return false;

		//does this have it?
		if(!contains(a)||!contains(b)) return false;

		//does it already exist?
		for(const auto& n:a->links) {
			if(n==b) return false;
		}

		a->links.emplace_back(b);

		return true;
	}

	bool removeNodesAt(const vf2d& pos) {
		bool removed=false;
		for(auto nit=nodes.begin(); nit!=nodes.end();) {
			const auto& n=*nit;
			if((n->pos-pos).mag2()<n->rad*n->rad) {
				//remove corresponding links
				for(const auto& o:nodes) {
					if(o==n) continue;

					for(auto lit=o->links.begin(); lit!=o->links.end();) {
						if(*lit==n) lit=o->links.erase(lit);
						else lit++;
					}
				}

				delete n;
				nit=nodes.erase(nit);
				removed=true;
			} else nit++;
		}
		return removed;
	}

	bool removeLinksAt(const vf2d& pos) {
		bool removed=false;
		for(const auto& n:nodes) {
			for(auto lit=n->links.begin(); lit!=n->links.end();) {
				const auto& l=*lit;

				//find closest point on segment
				auto pa=pos-n->pos, ba=l->pos-n->pos;
				float t=cmn::clamp(pa.dot(ba)/ba.dot(ba), 0.f, 1.f);
				vf2d close_pt=n->pos+t*ba;

				float rad=.5f*(n->rad+l->rad);
				if((close_pt-pos).mag2()<rad*rad) {
					lit=n->links.erase(lit);
					removed=true;
				} else lit++;
			}
		}
		return removed;
	}

	void seperate(Node* o) {
		if(!contains(o)) return;

		for(const auto& n:nodes) {
			if(n==o) continue;

			float t_rad=n->rad+o->rad;
			vf2d sub=o->pos-n->pos;
			float d_sq=sub.mag2();
			if(d_sq<t_rad*t_rad) {
				if(d_sq==0) sub={1, 0};
				o->pos+=t_rad*sub.norm();
			}
		}
	}

	std::string getInfo() const {
		std::stringstream str;
		str<<nodes.size()<<" node";
		if(nodes.size()!=1) str<<'s';

		str<<" and ";

		int links=0;
		for(const auto& n:nodes) links+=n->links.size();
		str<<links<<" link";
		if(links!=1) str<<'s';

		return str.str();
	}

	bool exportAs(const std::string& filename) {
		std::ofstream file(filename);
		if(file.fail()) return false;

		//print line by line while updating ids.
		int id=0;
		for(auto& n:nodes) {
			file<<"n "<<n->pos.x<<' '<<n->pos.y<<' '<<n->rad<<'\n';
			n->id=id++;
		}

		//just like an OBJ file
		//except 0-based indexing
		for(const auto& n:nodes) {
			for(const auto& l:n->links) {
				file<<"l "<<n->id<<' '<<l->id<<'\n';
			}
		}

		//not necessary
		file.close();
		return true;
	}

	bool importAs(const std::string& filename) {
		std::ifstream file(filename);
		if(file.fail()) return false;

		clear();

		//parse file line by line
		int id=0;
		std::string line;
		while(getline(file, line)) {
			std::stringstream line_str(line);

			//first char denotes type
			char type; line_str>>type;
			switch(type) {
				case 'n':
					float x, y, r;
					line_str>>x>>y>>r;

					nodes.emplace_back(new Node({x, y}, r));
					nodes.back()->id=id++;
					break;
				case 'l':
					int i_a, i_b;
					line_str>>i_a>>i_b;

					Node* a=getNodeById(i_a), * b=getNodeById(i_b);
					if(!a||!b) return false;

					a->links.emplace_back(b);
					break;
			}
		}

		file.close();
		return true;
	}

	[[nodiscard]] std::list<Node*> route(Node* from, Node* to) {
		std::list<Node*> path;
		if(!from||!to||from==to) return path;

		from->g_cost=0;

		for(const auto& n:nodes) {
			//compute h costs
			n->h_cost=(n->pos-to->pos).mag();
			//reset parents
			n->parent=nullptr;
		}

		std::list<Node*> open{from}, closed;

		while(open.size()) {
			//node in OPEN with lowest f_cost
			float record=INFINITY;
			auto curr_it=open.end();
			for(auto oit=open.begin(); oit!=open.end(); oit++) {
				const auto& o=*oit;
				if(o->f_cost<record) {
					record=o->f_cost;
					curr_it=oit;
				}
			}

			//remove from OPEN, add to CLOSED
			open.erase(curr_it);
			Node* curr=*curr_it;
			closed.emplace_back(curr);

			//path found
			if(curr==to) break;

			for(const auto& nbr:curr->links) {
				//skip if neighbor in CLOSED
				bool found=false;
				for(const auto& c:closed) {
					if(c==nbr) {
						found=true;
						break;
					}
				}
				if(found) continue;

				//new path shorter OR neighbor NOT in OPEN
				float new_g_cost=curr->g_cost+(nbr->pos-curr->pos).mag();
				bool shorter=new_g_cost<nbr->g_cost;
				bool in_open=false;
				for(const auto& o:open) {
					if(o==nbr) {
						in_open=true;
						break;
					}
				}
				if(shorter||!in_open) {
					nbr->g_cost=new_g_cost;
					nbr->f_cost=nbr->g_cost+nbr->h_cost;
					nbr->parent=curr;
					if(!in_open) open.emplace_back(nbr);
				}
			}
		}

		//traverse backwards
		bool valid=false;
		for(Node* curr=to; curr; curr=curr->parent) {
			path.emplace_back(curr);
			if(curr==from) {
				valid=true;
				break;
			}
		}
		//ensure path contains to and from
		if(valid) path.reverse();
		else path.clear();

		return path;
	}
};
#endif