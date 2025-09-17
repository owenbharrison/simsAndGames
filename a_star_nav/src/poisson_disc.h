#pragma once
#ifndef POISSON_DISC_SAMPLING_UTIL_H
#define POISSON_DISC_SAMPLING_UTIL_H

#include <vector>

//https://www.youtube.com/watch?v=7WcmyxyFO7o
std::vector<olc::vf2d> poissonDiscSample(const cmn::AABB& box, float rad) {
	//determine spacing
	float cell_size=rad/std::sqrt(2);
	int w=1+(box.max.x-box.min.x)/cell_size;
	int h=1+(box.max.y-box.min.y)/cell_size;
	olc::vf2d** grid=new olc::vf2d*[w*h];
	for(int i=0; i<w*h; i++) grid[i]=nullptr;

	//where can i spawn from?
	std::vector<olc::vf2d> spawn_pts{box.getCenter()};

	//as long as there are spawnable pts,
	std::vector<olc::vf2d> pts;
	while(spawn_pts.size()) {
		//choose random spawn pt
		auto it=spawn_pts.begin();
		std::advance(it, std::rand()%spawn_pts.size());
		const auto& spawn=*it;

		//try n times to add pt
		int k=0;
		const int samples=20;
		for(; k<samples; k++) {
			float angle=cmn::randFloat(2*cmn::Pi);
			float dist=cmn::randFloat(rad, 2*rad);
			olc::vf2d cand=spawn+cmn::polar<olc::vf2d>(dist, angle);
			if(!box.contains(cand)) continue;

			//check 3x3 region around candidate
			bool valid=true;
			int ci=(cand.x-box.min.x)/cell_size;
			int cj=(cand.y-box.min.y)/cell_size;
			int si=std::max(0, ci-2);
			int sj=std::max(0, cj-2);
			int ei=std::min(w-1, ci+2);
			int ej=std::min(h-1, cj+2);
			for(int i=si; i<=ei; i++) {
				for(int j=sj; j<=ej; j++) {
					//if there is a point, and its too close,
					const auto& idx=grid[i+w*j];
					if(idx&&(*idx-cand).mag2()<rad*rad) {
						//invalidate it
						valid=false;
						break;
					}
				}
				if(!valid) break;
			}

			//if no points too close, add the sucker
			if(valid) {
				if(ci<0||cj<0||ci>=w||cj>=h) continue;
				pts.push_back(cand);
				grid[ci+w*cj]=&pts.back();
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