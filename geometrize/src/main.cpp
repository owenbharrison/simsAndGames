#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "common/utils.h"

#include "shapes/shape.h"

#include "shapes/ellipse.h"
#include "shapes/triangle.h"
#include "shapes/rectangle.h"

//get normalized difference between two images.
float getCost(olc::Sprite* a, olc::Sprite* b) {
	//these should be the same size, but im just checking
	int width=std::min(a->width, b->width);
	int height=std::min(a->height, b->height);

	//sum differences between two images.
	float cost=0;
	for(int i=0; i<width; i++) {
		for(int j=0; j<height; j++) {
			const auto& p_a=a->GetPixel(i, j);
			const auto& p_b=b->GetPixel(i, j);
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

class GeometrizeUI : public olc::PixelGameEngine {
	int image_width=0;
	int image_height=0;
	olc::Sprite* target_spr=nullptr;
	olc::Sprite* approx_spr=nullptr;
	olc::Decal* approx_dec=nullptr;
	olc::Sprite* guess_spr=nullptr;

public:
	GeometrizeUI() {
		sAppName="Geometrize";
	}

	float prev_cost=1;

	bool OnUserCreate() override {
		std::srand(std::time(0));

		//initialize images
		image_width=ScreenWidth();
		image_height=ScreenHeight();
		target_spr=new olc::Sprite(image_width, image_height);
		approx_spr=new olc::Sprite(image_width, image_height);
		approx_dec=new olc::Decal(approx_spr);
		guess_spr=new olc::Sprite(image_width, image_height);
		
		//load input & use uvs to resample
		{
			const olc::Sprite image("assets/tom_and_jerry.png");
			const float u_step=1.f/image_width;
			const float v_step=1.f/image_height;
			for(int i=0; i<image_width; i++) {
				for(int j=0; j<image_height; j++) {
					float u=u_step*i, v=v_step*j;
					target_spr->SetPixel(i, j, image.Sample(u, v));
				}
			}
		}

		return true;
	}

	bool OnUserDestroy() {
		delete target_spr;
		delete approx_dec;
		delete approx_spr;
		delete guess_spr;
		
		return true;
	}

	bool OnUserUpdate(float dt) override {
		for(int i=0; i<10; i++) {
			//set guess to approx
			std::memcpy(guess_spr->GetData(), approx_spr->GetData(), sizeof(olc::Pixel)*image_width*image_height);

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
				curr_shape->chooseColor(target_spr);
				curr_shape->addToImage(guess_spr);

				//if that shape made it better, add it to approx
				float curr_cost=getCost(target_spr, guess_spr);
				if(curr_cost<prev_cost) {
					curr_shape->addToImage(approx_spr);
					prev_cost=curr_cost;
				}

				delete curr_shape;
			}
		}

		//print cost
		std::cout<<"cost: "<<prev_cost<<'\n';

		//show approx image
		approx_dec->Update();
		DrawDecal({0, 0}, approx_dec);

		return true;
	}
};

int main() {
	GeometrizeUI gui;
	bool vsync=false;
	if(gui.Construct(640, 360, 1, 1, false, vsync)) gui.Start();

	return 0;
}