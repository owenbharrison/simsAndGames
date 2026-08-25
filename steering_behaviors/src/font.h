//not sure the proper architecture here.
//didnt want to pollute main with this
#pragma once
#ifndef FONT_STRUCT_H
#define FONT_STRUCT_H

#include "cmn/math/v2d.h"

#include <vector>

#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/include/stb_image.h"

struct Letter {
	float asp=1;
	std::vector<cmn::vf2d> pts;
};

namespace Font {
	static const char min='!';
	static const char max='~';
	static std::unordered_map<char, Letter> letters;

	bool setup() {
		//neighbor directions
		static const int di[4]{-1, 0, 1, 0};
		static const int dj[4]{0, -1, 0, 1};
		//max allowable color diff
		static const int col_thr=200;
		//min allowable pt dist
		static const float dist_thr=6;

		for(int l=min; l<=max; l++) {
			//load image
			auto filename="assets/ascii/"+std::to_string(l)+".png";
			int width, height, comp;
			auto pixels=stbi_load(filename.c_str(), &width, &height, &comp, 4);
			if(!pixels) return false;

			//flatten 2d index
			auto ix=[width] (int i, int j) { return i+width*j; };

			const float asp=float(width)/height;

			//for each pixel
			std::vector<cmn::vf2d> pts;
			for(int i=1; i<width-1; i++) {
				for(int j=1; j<height-1; j++) {
					int k=ix(i, j);
					int r=pixels[0+4*k];
					int g=pixels[1+4*k];
					int b=pixels[2+4*k];

					//check neighboring pixels
					bool edge=false;
					for(int d=0; d<4; d++) {
						int i_c=i+di[d], j_c=j+dj[d];
						int k_c=ix(i_c, j_c);
						int r_c=pixels[0+4*k_c];
						int g_c=pixels[1+4*k_c];
						int b_c=pixels[2+4*k_c];

						//color difference
						int dr=r-r_c;
						int dg=g-g_c;
						int db=b-b_c;

						//edge detection
						int d_sq=dr*dr+dg*dg+db*db;
						if(d_sq>col_thr*col_thr) {
							edge=true;
							break;
						}
					}
					if(!edge) continue;

					//place in center of pixel
					cmn::vf2d ij(.5f+i, .5f+j);

					//check proximity to other points
					bool unique=true;
					for(const auto& p:pts) {
						cmn::vf2d sub=p-ij;
						if(dot(sub, sub)<dist_thr*dist_thr) {
							unique=false;
							break;
						}
					}

					//only place new point so close to any other
					if(unique) pts.push_back(ij);
				}
			}

			stbi_image_free(pixels);

			//turn into "uvs"
			for(auto& p:pts) p/=height;

			letters[l]={asp, pts};
		}

		return true;
	}

	//format string w/ given letter sizes & do func per char
	template<typename Func>
	void formattedStringDo(const std::string& str, Func func) {
		float ox=0, oy=0;

		for(const auto& c:str) {
			//formatting for special chars
			if(c==' ') ox+=.5f;//.5 aspect ratio
			else if(c=='\t') ox+=1;//2 spaces
			else if(c=='\n') ox=0, oy+=1;//carriage return
			else if(c>=min&&c<=max) {
				const auto& l=letters[c];

				func(l, ox, oy);

				ox+=l.asp;
			}
		}
	}

	//formatted string size w/ height=1
	cmn::vf2d stringSize(const std::string& str) {
		float sx=0, sy=0;
		formattedStringDo(str, [&] (const Letter& l, float ox, float oy) {
			//vacuform to letter
			sx=std::max(sx, ox+l.asp);
			sy=std::max(sy, oy+1);
		});
		return {sx, sy};
	}

	//accumulate letter points for formatted string
	std::vector<cmn::vf2d> stringToDots(
		const cmn::vf2d& pos,
		const std::string& str,
		float height
	) {
		//accumulate
		std::vector<cmn::vf2d> pts;
		formattedStringDo(str, [&] (const Letter& l, float ox, float oy) {
			//shift by offset
			for(const auto& p:l.pts) pts.push_back(p+cmn::vf2d(ox, oy));
		});

		//scale
		for(auto& p:pts) p=pos+height*p;
		return pts;
	}
}
#endif