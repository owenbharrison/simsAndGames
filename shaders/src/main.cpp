#define OLC_GFX_OPENGL33
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include "shade_shader.h"

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
	olc::Effect pass1, pass2, pass3;

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
		//initialize buffers
		source.spr=new olc::Sprite("assets/london.jpg");
		source.dec=new olc::Decal(source.spr);
		buf_a.spr=new olc::Sprite(2*source.spr->width, source.spr->height);
		buf_a.dec=new olc::Decal(buf_a.spr);
		buf_b.spr=new olc::Sprite(2*source.spr->width, source.spr->height);
		buf_b.dec=new olc::Decal(buf_b.spr);
		buf_c.spr=new olc::Sprite(source.spr->width, source.spr->height);
		buf_c.dec=new olc::Decal(buf_c.spr);
		
		//load effect passes
		pass1=shade.MakeEffect(loadEffect("fx/eeb/calc_st.txt"));
		if(!pass1.IsOK()) {
			std::cout<<"pass1 err:\n"<<pass1.GetStatus();
			return false;
		}
		pass2=shade.MakeEffect(loadEffect("fx/eeb/blur_st.txt"));
		if(!pass2.IsOK()) {
			std::cout<<"pass2 err:\n"<<pass2.GetStatus();
			return false;
		}
		pass3=shade.MakeEffect(loadEffect("fx/eeb/lic.txt"));
		if(!pass3.IsOK()) {
			std::cout<<"pass3 err:\n"<<pass3.GetStatus();
			return false;
		}

		//apply them
		shade.SetSourceDecal(source.dec);
		shade.SetTargetDecal(buf_a.dec);
		shade.Start(&pass1);
		shade.DrawQuad({-1, -1}, {2, 2});
		shade.End();

		shade.SetSourceDecal(buf_a.dec);
		shade.SetTargetDecal(buf_b.dec);
		shade.Start(&pass2);
		shade.DrawQuad({-1, -1}, {2, 2});
		shade.End();

		shade.SetSourceDecal(buf_b.dec);
		shade.SetTargetDecal(buf_c.dec);
		shade.Start(&pass3);
		shade.DrawQuad({-1, -1}, {2, 2});
		shade.End();

		return true;
	}

	bool OnUserDestroy() override {
		return true;
	}

	bool OnUserUpdate(float dt) override {
		Clear(olc::BLACK);

		if(GetKey(olc::Key::SPACE).bHeld) DrawDecal({0, 0}, source.dec);
		else DrawDecal({0, 0}, buf_c.dec);

		return true;
	}
};

int main() {
	Demo d;
	bool vsync=true;
	if(d.Construct(700, 400, 1, 1, false, vsync)) d.Start();

	return 0;
}