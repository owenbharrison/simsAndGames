#define OLC_GFX_OPENGL33
#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "common/shade_shader.h"

#include <fstream>

olc::EffectConfig loadEffect(const std::string& filename) {
	//get file
	std::ifstream file(filename);
	if(file.fail()) return olc::fx::FX_NORMAL;

	//dump contents into str stream
	std::stringstream mid;
	mid<<file.rdbuf();

	return {
		DEFAULT_VS,
		mid.str(),
		1,
		1
	};
}

struct Demo : public olc::PixelGameEngine {
	Demo() {
		sAppName="Shaders";
	}

	olc::Shade shade;
	olc::Effect pass1, pass2;

	struct Buffer {
		olc::Sprite* spr=nullptr;
		olc::Decal* dec=nullptr;

		~Buffer() {
			delete spr;
			delete dec;
		}
	};

	Buffer source, buf_a, buf_b, buf_c;

	bool OnUserCreate() override {
		//source
		olc::Sprite* lenna=new olc::Sprite("assets/lenna.png");
		source.spr=new olc::Sprite(lenna->width/2, lenna->height/2);
		for(int i=0; i<source.spr->width; i++) {
			for(int j=0; j<source.spr->height; j++) {
				source.spr->SetPixel(i, j, lenna->GetPixel(2*i, 2*j));
			}
		}
		delete lenna;
		source.dec=new olc::Decal(source.spr);

		//1st pass
		buf_a.spr=new olc::Sprite(source.spr->width, source.spr->height);
		buf_a.dec=new olc::Decal(buf_a.spr);

		pass1=shade.MakeEffect(loadEffect("fx/test/flip_x.txt"));
		if(!pass1.IsOK()) {
			std::cout<<"pass1 err:\n"<<pass1.GetStatus();
			return false;
		}

		shade.SetSourceDecal(source.dec);
		shade.SetTargetDecal(buf_a.dec);
		shade.Start(&pass1);
		shade.DrawQuad({-1, -1}, {2, 2});
		shade.End();

		//1st pass
		buf_b.spr=new olc::Sprite(source.spr->width, source.spr->height);
		buf_b.dec=new olc::Decal(buf_b.spr);

		pass2=shade.MakeEffect(loadEffect("fx/test/flip_y.txt"));
		if(!pass2.IsOK()) {
			std::cout<<"pass2 err:\n"<<pass2.GetStatus();
			return false;
		}

		shade.SetSourceDecal(buf_a.dec);
		shade.SetTargetDecal(buf_b.dec);
		shade.Start(&pass2);
		shade.DrawQuad({-1, -1}, {2, 2});
		shade.End();

		return true;
	}

	bool OnUserDestroy() override {
		return true;
	}

	bool OnUserUpdate(float dt) override {
		Clear(olc::BLACK);

		DrawDecal(vf2d(0, 0), source.dec);
		DrawDecal(vf2d(0, 200), buf_a.dec);
		DrawDecal(vf2d(0, 400), buf_b.dec);

		return true;
	}
};

int main() {
	Demo d;
	bool vsync=true;
	if(d.Construct(200, 600, 1, 1, false, vsync)) d.Start();

	return 0;
}