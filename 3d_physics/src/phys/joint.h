#pragma once
#ifndef JOINT_STRUCT_H
#define JOINT_STRUCT_H

//basically a weighted constraint
struct Joint {
	Shape* shp_a=nullptr;
	std::list<int> ix_a;
	Shape* shp_b=nullptr;
	std::list<int> ix_b;

	void update() {
		//find side centers
		vf3d avg_a;
		for(const auto& ia:ix_a) avg_a+=shp_a->particles[ia].pos;
		avg_a/=ix_a.size();
		vf3d avg_b;
		for(const auto& ib:ix_b) avg_b+=shp_b->particles[ib].pos;
		avg_b/=ix_b.size();

		//update each side halfway
		vf3d axis=avg_b-avg_a;
		vf3d delta=.5f*axis;
		for(const auto& ia:ix_a) {
			auto& a=shp_a->particles[ia];
			if(!a.locked) a.pos+=delta;
		}
		for(const auto& ib:ix_b) {
			auto& b=shp_b->particles[ib];
			if(!b.locked) b.pos-=delta;
		}
	}
};
#endif