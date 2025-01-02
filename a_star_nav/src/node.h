#pragma once
#ifndef NODE_STRUCT_H
#define NODE_STRUCT_H

struct Node {
	std::list<Node*> links;
	vf2d pos;
	float rad;
	int id=-1;
	float g_cost=0, h_cost=0, f_cost=0;
	Node* parent=nullptr;

	Node()=delete;

	Node(vf2d p, float r=3) : pos(p), rad(r) {}

	//dont copy links?
	//negative id to differentiate original, and copy
	Node(const Node& n) : pos(n.pos), rad(n.rad), id(-n.id) {}
};
#endif