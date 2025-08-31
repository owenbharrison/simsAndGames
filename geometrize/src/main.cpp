#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "common/utils.h"

#include "shapes/ellipse.h"

#include "shapes/triangle.h"

#include "shapes/rectangle.h"

//get normalized difference between two images.
float getFitness(olc::Sprite* a, olc::Sprite* b) {
	//these should be the same size, but im just checking
	int width=std::min(a->width, b->width);
	int height=std::min(a->height, b->height);

	//sum differences between two images.
	float fitness=0;
	for(int i=0; i<width; i++) {
		for(int j=0; j<height; j++) {
			const auto& p_a=a->GetPixel(i, j);
			const auto& p_b=b->GetPixel(i, j);
			fitness+=std::abs(p_a.r-p_b.r);
			fitness+=std::abs(p_a.g-p_b.g);
			fitness+=std::abs(p_a.b-p_b.b);
		}
	}

	//3 channels
	fitness/=3;

	//255 colors
	fitness/=255;

	//width*height pixels
	fitness/=width*height;

	//final normalized value
	return fitness;
}

class GeometrizeUI : public olc::PixelGameEngine {
	olc::Sprite* target=nullptr;
	olc::Sprite* approx=nullptr;
	olc::Sprite* guess=nullptr;

public:
	GeometrizeUI() {
		sAppName="Geometrize";
	}

	float prev_fitness=1;

	bool OnUserCreate() override {
		std::srand(std::time(0));

		//initialize images
		target=new olc::Sprite(ScreenWidth(), ScreenHeight());
		approx=new olc::Sprite(ScreenWidth(), ScreenHeight());
		guess=new olc::Sprite(ScreenWidth(), ScreenHeight());
		
		//load input & use uvs to resample
		{
			const olc::Sprite image("assets/tom_and_jerry.png");
			const float u_step=1.f/target->width;
			const float v_step=1.f/target->height;
			for(int i=0; i<target->width; i++) {
				for(int j=0; j<target->height; j++) {
					float u=u_step*i, v=v_step*j;
					target->SetPixel(i, j, image.Sample(u, v));
				}
			}
		}

		return true;
	}

	bool OnUserDestroy() {
		delete target;
		delete approx;
		delete guess;
		
		return true;
	}

	bool OnUserUpdate(float dt) override {
		//set guess to approx
		SetDrawTarget(guess);
		DrawSprite(0, 0, approx);
		SetDrawTarget(nullptr);

		//generate some random shape
		{
			ShapePrimitive* curr_shape=nullptr;
			switch(std::rand()%3) {
				//just being careful
				default: curr_shape=new EllipseShape(); break;
				case 1: curr_shape=new TriangleShape(); break;
				case 2: curr_shape=new RectangleShape(); break;
			}
			curr_shape->randomize(GetScreenSize());
			curr_shape->addToImage(guess);

			//if that shape made it better, add it to approx
			float curr_fitness=getFitness(target, guess);
			if(curr_fitness<prev_fitness) {
				curr_shape->addToImage(approx);
				prev_fitness=curr_fitness;
				std::cout<<curr_fitness<<'\n';
			}

			delete curr_shape;
		}

		//show approx image
		DrawSprite(0, 0, approx);

		return true;
	}
};

int main() {
	GeometrizeUI gui;
	bool vsync=false;
	if(gui.Construct(640, 360, 1, 1, false, vsync)) gui.Start();

	return 0;
}