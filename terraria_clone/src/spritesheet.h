#pragma once
#ifndef SPRITESHEET_STRUCT_H
#define SPRITESHEET_STRUCT_H

//uhh how do i use this?

//rule of three?
struct SpriteSheet {
	size_t width, height, num;

	olc::Sprite* spr=nullptr;
	//decal?

	SpriteSheet(size_t w, size_t h, size_t n, std::string str) : width(w), height(h), num(n) {
		spr=new olc::Sprite(str);
	}

	~SpriteSheet() {
		delete spr;
	}

	/*narrowing conversion?
	olc::vi2d which(int n) const {
		if(n<0||n>=num) return {0, 0};

		int j=n/width;
		return {n-width*j, j};
	}
	*/
};
#endif