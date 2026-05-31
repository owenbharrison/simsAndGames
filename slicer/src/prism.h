#pragma once
#ifndef PRISM_STRUCT_H
#define PRISM_STRUCT_H

struct Prism {
	//sorted by decreasing area
	static constexpr int sizes[][2]{
		{2, 10},
		{2, 8},
		{2, 6},
		{1, 10},
		{1, 8},
		{2, 4},
		{1, 6},
		{2, 3},
		{1, 4},
		{2, 2},
		{1, 3},
		{1, 2},
		{1, 1}
	};
	static constexpr int num_sizes=sizeof(sizes)/sizeof(sizes[0]);
	
	int x=0, y=0, z=0;
	int w=0, d=0;
};
#endif