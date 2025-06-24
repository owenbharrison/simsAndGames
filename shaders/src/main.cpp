#define OLC_GFX_OPENGL33
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_SHADERS
#include "olcPGEX_Shaders.h"

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
	olc::Effect calc_st_pass, blur_st_pass, lic_pass;

	struct Buffer {
		olc::Sprite* spr=nullptr;
		olc::Decal* dec=nullptr;

		~Buffer() {
			delete spr;
			delete dec;
		}
	};

	Buffer source, buf_a, buf_b, target;

	bool OnUserCreate() override {
		//initialize buffers
		source.spr=new olc::Sprite("assets/london.jpg");
		source.dec=new olc::Decal(source.spr);
		buf_a.spr=new olc::Sprite(2*source.spr->width, source.spr->height);
		buf_a.dec=new olc::Decal(buf_a.spr);
		buf_b.spr=new olc::Sprite(2*source.spr->width, source.spr->height);
		buf_b.dec=new olc::Decal(buf_b.spr);
		target.spr=new olc::Sprite(source.spr->width, source.spr->height);
		target.dec=new olc::Decal(target.spr);
		
		//load effect passes
		calc_st_pass=shade.MakeEffect(loadEffect("assets/fx/eeb/calc_st.glsl"));
		if(!calc_st_pass.IsOK()) {
			std::cout<<"calc st err:\n"<<calc_st_pass.GetStatus();
			return false;
		}
		blur_st_pass=shade.MakeEffect(loadEffect("assets/fx/eeb/blur_st.glsl"));
		if(!blur_st_pass.IsOK()) {
			std::cout<<"blur st err:\n"<<blur_st_pass.GetStatus();
			return false;
		}
		lic_pass=shade.MakeEffect(loadEffect("assets/fx/eeb/lic.glsl"));
		if(!lic_pass.IsOK()) {
			std::cout<<"lic err:\n"<<lic_pass.GetStatus();
			return false;
		}

		//apply them
		shade.SetSourceDecal(source.dec);
		shade.SetTargetDecal(buf_a.dec);
		shade.Start(&calc_st_pass);
		shade.DrawQuad({-1, -1}, {2, 2});
		shade.End();

		shade.SetSourceDecal(buf_a.dec);
		shade.SetTargetDecal(buf_b.dec);
		shade.Start(&blur_st_pass);
		shade.DrawQuad({-1, -1}, {2, 2});
		shade.End();

		shade.SetSourceDecal(buf_b.dec);
		shade.SetTargetDecal(target.dec);
		shade.Start(&lic_pass);
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
		else DrawDecal({0, 0}, target.dec);

		return true;
	}
};

int main() {
	Demo d;
	bool vsync=true;
	if(d.Construct(700, 400, 1, 1, false, vsync)) d.Start();

	return 0;
}