#define OLC_GFX_OPENGL33
#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"

#include "shade_shader.h"

struct Demo : public olc::PixelGameEngine {
	Demo() {
		sAppName="Shaders";
	}

	olc::Shade shade;
	olc::Effect effect;

	olc::Sprite* source_spr=nullptr;
	olc::Decal* source_dec=nullptr;
	olc::Sprite* target_spr=nullptr;
	olc::Decal* target_dec=nullptr;

	bool OnUserCreate() override {
		effect=shade.MakeEffect(olc::fx::FX_SOBEL);

		source_spr=new olc::Sprite("assets/lenna.png");
		source_dec=new olc::Decal(source_spr);
		target_spr=new olc::Sprite(source_spr->width, source_spr->height);
		target_dec=new olc::Decal(target_spr);

		return true;
	}

	bool OnUserDestroy() override {
		delete source_spr;
		delete source_dec;
		delete target_spr;
		delete target_dec;

		return true;
	}

	bool OnUserUpdate(float dt) override {
		shade.Start(&effect);
		shade.SetSourceDecal(source_dec);
		shade.SetTargetDecal(target_dec);
		shade.DrawDecal({0, 0}, source_dec);
		shade.End();

		Clear(olc::GREY);

		DrawDecal({0, 0}, target_dec);

		return true;
	}
};

int main() {
	Demo d;
	bool vsync=true;
	if(d.Construct(400, 400, 1, 1, false, vsync)) d.Start();

	return 0;
}