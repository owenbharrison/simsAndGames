#pragma once
#ifndef POISSON_UTIL_H
#define POISSON_UTIL_H

#include <vector>
#include <list>

#include "cmn/utils.h"

//https://www.youtube.com/watch?v=7WcmyxyFO7o
std::vector<cmn::vf2d> poissonDiscSample(const cmn::vf2d& min, const cmn::vf2d& max, float rad) {
	//determine spacing
	float cell_sz=rad/std::sqrtf(2);
	int width=1+(max.x-min.x)/cell_sz;
	int height=1+(max.y-min.y)/cell_sz;
	int* grid=new int[width*height];
	for(int i=0; i<width*height; i++) grid[i]=-1;

	//where can i spawn from?
	std::list<cmn::vf2d> spawn_pts{(min+max)/2};

	//as long as there are spawnable pts,
	std::vector<cmn::vf2d> pts;
	while(spawn_pts.size()) {
		//choose random spawn pt
		auto it=spawn_pts.begin();
		std::advance(it, std::rand()%spawn_pts.size());
		const auto& spawn=*it;

		//try n times to...
		int k=0;
		const int samples=20;
		for(; k<samples; k++) {
			//add candidate near spawn pt
			float angle=cmn::randFloat(2*cmn::Pi);
			float dist=rad*cmn::randFloat(1, 2);
			cmn::vf2d cand=spawn+cmn::polar<cmn::vf2d>(dist, angle);
			if(cand.x<min.x||cand.y<min.y) continue;
			if(cand.x>max.x||cand.y>max.y) continue;

			//check 5x5 region around candidate
			bool valid=true;
			int ci=(cand.x-min.x)/cell_sz;
			int cj=(cand.y-min.y)/cell_sz;
			for(int di=-2; di<=2; di++) {
				for(int dj=-2; dj<=2; dj++) {
					int i=ci+di, j=cj+dj;
					if(i<0||j<0||i>=width||j>=height) continue;

					//if there is a point, and its too close,
					const auto& idx=grid[i+width*j];
					if(idx!=-1&&(pts[idx]-cand).mag_sq()<rad*rad) {
						valid=false;
						break;
					}
				}
				if(!valid) break;
			}

			//if no points too close, add the sucker
			if(valid) {
				if(ci<0||cj<0||ci>=width||cj>=height) continue;

				pts.push_back(cand);
				grid[ci+width*cj]=pts.size()-1;
				spawn_pts.push_back(cand);
				break;
			}
		}

		//not spawnable enough, remove
		if(k==samples) spawn_pts.erase(it);
	}

	delete[] grid;

	return pts;
}
#endif