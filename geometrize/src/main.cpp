#define OLC_PGE_APPLICATION
#include "olc/include/olcPixelGameEngine.h"
using olc::vf2d;

#include "cmn/utils.h"

#include "shapes/shape.h"

#include "shapes/ellipse.h"
#include "shapes/triangle.h"
#include "shapes/rectangle.h"

//for memset
#include <string>

#include <random>

class GeometrizeUI : public olc::PixelGameEngine {
	int image_width=0;
	int image_height=0;
	olc::Renderable target, approx, guess;
	
	float prev_cost=1;

public:
	GeometrizeUI() {
		sAppName="Geometrize";
	}

	bool OnUserCreate() override {
		std::srand(std::time(0));

		//initialize images
		image_width=ScreenWidth();
		image_height=ScreenHeight();
		target.Create(image_width, image_height);
		approx.Create(image_width, image_height);
		guess.Create(image_width, image_height);
		
		{
			//choose random image
			const std::vector<std::string> filenames{
				"assets/giraffe.png",
				"assets/porsche.png",
				"assets/seagull.png",
				"assets/snoopy.png",
				"assets/tom_and_jerry.png",
				"assets/white_tiger.png"
			};
			//wasnt random enough
			std::random_device dev;
			std::mt19937 rng(dev());
			std::uniform_int_distribution<std::mt19937::result_type> dist(0, filenames.size()-1);
			const olc::Sprite image(filenames[dist(rng)]);

			//resample img
			const float u_step=1.f/image_width;
			const float v_step=1.f/image_height;
			for(int i=0; i<image_width; i++) {
				for(int j=0; j<image_height; j++) {
					float u=u_step*i, v=v_step*j;
					target.Sprite()->SetPixel(i, j, image.SampleBL(u, v));
				}
			}

			target.Decal()->Update();
		}

		return true;
	}

#pragma region UPDATE HELPERS
	void copyImage(olc::Renderable& dst, olc::Renderable& src) {
		auto dst_spr=dst.Sprite(), src_spr=src.Sprite();
		int width=dst_spr->width, height=dst_spr->height;
		if(src_spr->width!=width||src_spr->height!=height) return;
		std::memcpy(dst_spr->GetData(), src_spr->GetData(), sizeof(olc::Pixel)*width*height);
	}

	//get normalized difference between two images.
	float getCost(const olc::Renderable& a, const olc::Renderable& b) {
		auto a_spr=a.Sprite(), b_spr=b.Sprite();
		//these should be the same size, but im just checking
		int width=std::min(a_spr->width, b_spr->width);
		int height=std::min(a_spr->height, b_spr->height);

		//sum differences between two images.
		float cost=0;
		for(int i=0; i<width; i++) {
			for(int j=0; j<height; j++) {
				const auto& p_a=a_spr->GetPixel(i, j);
				const auto& p_b=b_spr->GetPixel(i, j);
				int dr=p_a.r-p_b.r;
				int dg=p_a.g-p_b.g;
				int db=p_a.b-p_b.b;
				cost+=std::sqrt(dr*dr+dg*dg+db*db);
			}
		}

		//cube diagonal
		cost/=255*std::sqrt(3);

		//width*height pixels
		cost/=width*height;

		//final normalized value
		return cost;
	}
#pragma endregion

	bool OnUserUpdate(float dt) override {
		//add ten shapes per frame
		for(int i=0; i<10; i++) {
			//set guess to approx
			copyImage(guess, approx);

			//generate some random shape
			{
				ShapePrimitive* curr_shape=nullptr;
				switch(std::rand()%3) {
					//just being careful
					default: curr_shape=new EllipseShape(); break;
					case 1: curr_shape=new TriangleShape(); break;
					case 2: curr_shape=new RectangleShape(); break;
				}
				curr_shape->randomizeGeometry(vf2d(image_width, image_height));
				curr_shape->chooseColor(target.Sprite());
				curr_shape->addToImage(guess.Sprite());

				//if that shape made it better, add it to approx
				float curr_cost=getCost(target, guess);
				if(curr_cost<prev_cost) {
					curr_shape->addToImage(approx.Sprite());
					prev_cost=curr_cost;
				}

				delete curr_shape;
			}
		}

		//print cost
		//std::cout<<"cost: "<<prev_cost<<'\n';

		//update approx image
		approx.Decal()->Update();
		DrawDecal({0, 0}, approx.Decal());

		//draw target preview
		if(GetMouse(olc::Mouse::LEFT).bHeld) {
			float mx=GetMouseX();
			float u=mx/ScreenWidth();
			vf2d source_pos(u*target.Sprite()->width, 0);
			vf2d source_sz(
				(1-u)*target.Sprite()->width,
				target.Sprite()->height
			);
			DrawPartialDecal({mx, 0}, target.Decal(), source_pos, source_sz);
			
			//draw dividing line
			FillRectDecal({mx-2, 0}, vf2d(4, ScreenHeight()), olc::BLACK);
		}

		return true;
	}
};

int main() {
	GeometrizeUI gui;
	bool vsync=false;
	if(gui.Construct(700, 500, 1, 1, false, vsync)) gui.Start();

	return 0;
}