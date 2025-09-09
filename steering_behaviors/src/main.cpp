#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

class SteeringBehaviorsUI : public olc::PixelGameEngine {
	olc::vi2d letter_sizes[26];
	std::vector<vf2d> letter_points[26];

	std::vector<vf2d> word;

public:
	SteeringBehaviorsUI() {
		sAppName="Steering Behaviors";
	}

	bool OnUserCreate() override {
		//for each letter
		for(int l=0; l<26; l++) {
			//load letter sprite
			std::string filename="assets/"+std::string(1, 'a'+l)+".png";
			olc::Sprite* spr=new olc::Sprite(filename);
			
			//store sprite size
			letter_sizes[l]={spr->width, spr->height};
			
			//for each pixel
			for(int i=0; i<spr->width-1; i++) {
				for(int j=0; j<spr->height-1; j++) {
					const auto& curr=spr->GetPixel(i, j);

					//check right
					const auto& right=spr->GetPixel(1+i, j);
					float cr_dr=(curr.r-right.r)/255.f;
					float cr_dg=(curr.g-right.g)/255.f;
					float cr_db=(curr.b-right.b)/255.f;
					float cr_dist=std::sqrt(cr_dr*cr_dr+cr_dg*cr_dg+cr_db*cr_db);
					
					//check down
					const auto& down=spr->GetPixel(i, 1+j);
					float cd_dr=(curr.r-down.r)/255.f;
					float cd_dg=(curr.g-down.g)/255.f;
					float cd_db=(curr.b-down.b)/255.f;
					float cd_dist=std::sqrt(cd_dr*cd_dr+cd_dg*cd_dg+cd_db*cd_db);

					//if different: likely an edge
					if(cr_dist>.5f||cd_dist>.5f) {
						//use uv coords
						vf2d uv((.5f+i)/spr->width, (.5f+j)/spr->height);
						
						//only place new point so close to any other
						bool unique=true;
						for(const auto& p:letter_points[l]) {
							if((uv-p).mag2()<.006f) {
								unique=false;
								break;
							}
						}
						if(unique) letter_points[l].push_back(uv);
					}
				}
			}
			delete spr;
		}

		word=textToDots({50, 50}, " HELLO\nWORLD", 120);

		return true;
	}

	//fit in aabb next
	std::vector<vf2d> textToDots(const vf2d& pos, const std::string& str, float height) {
		//for every character in string
		vf2d offset=pos;
		std::vector<vf2d> pts;
		for(const auto& c:str) {
			//space spacing :D
			if(c==' ') offset.x+=.5f*height;
			//newline formatting
			else if(c=='\n') offset.x=pos.x, offset.y+=height;
			//letter spacing...
			else if(c>='A'&&c<='Z') {
				int ix=c-'A';
				//aspect ratio
				const auto& sz=letter_sizes[ix];
				vf2d scaled(height*sz.x/sz.y, height);
				for(const auto& uv:letter_points[ix]) {
					pts.push_back(offset+scaled*uv);
				}
				offset.x+=scaled.x;
			}
		}
		return pts;
	}

	bool OnUserUpdate(float dt) override {
		Clear(olc::BLACK);

		for(const auto& p:word) {
			FillCircle(p, 2);
		}

		return true;
	}
};

int main() {
	SteeringBehaviorsUI sbui;
	bool vsync=false;
	if(sbui.Construct(640, 360, 1, 1, false, vsync)) sbui.Start();

	return 0;
}