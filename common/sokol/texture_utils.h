#pragma once
#ifndef CMN_TEXTURE_UTIL_H
#define CMN_TEXTURE_UTIL_H

//for memcpy
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb/include/stb_image.h"

#include <cstdint>

namespace cmn {
	static sg_view makeTextureFromPixels(std::uint8_t* pixels, int width, int height) {
		sg_image_desc image_desc{};
		image_desc.width=width;
		image_desc.height=height;
		image_desc.data.mip_levels[0].ptr=pixels;
		image_desc.data.mip_levels[0].size=4*width*height;
		sg_image image=sg_make_image(image_desc);

		sg_view_desc view_desc{};
		view_desc.texture.image=image;
		return sg_make_view(view_desc);
	}

	static sg_view makeBlankTexture() {
		std::uint8_t pixel[4]{255, 255, 255, 255};
		return makeTextureFromPixels(pixel, 1, 1);
	}

	static sg_view makeUVTexture(int width, int height) {
		std::uint8_t* pixels=new std::uint8_t[4*width*height];
		for(int j=0; j<height; j++) {
			for(int i=0; i<width; i++) {
				float u=(.5f+i)/width;
				float v=(.5f+j)/height;

				int k=i+width*j;
				pixels[0+4*k]=255*u;
				pixels[1+4*k]=255*v;
				pixels[2+4*k]=127;
				pixels[3+4*k]=255;
			}
		}

		sg_view view=makeTextureFromPixels(pixels, width, height);
		delete[] pixels;

		return view;
	}

	static bool makeTextureFromFile(sg_view& view, const std::string& filename) {
		int width, height, channels;
		auto pixels=stbi_load(filename.c_str(), &width, &height, &channels, 4);
		if(!pixels) return false;

		view=makeTextureFromPixels(pixels, width, height);
		delete[] pixels;

		return true;
	}
}
#endif