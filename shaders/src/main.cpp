#define OLC_GFX_OPENGL33
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_SHADERS
#include "olcPGEX_Shaders.h"

#include <fstream>
#include <exception>
#include <sstream>

olc::EffectConfig loadEffect(const std::string& filename) {
	//get file
	std::ifstream file(filename);
	if(file.fail()) throw std::runtime_error("invalid filename");

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

	olc::Renderable prev_src, curr_src;
	olc::Renderable stitch_src;
	olc::Renderable target;

	olc::Shade shader;
	olc::Effect uv_effect, identity_effect;
	olc::Effect avg_effect, difference_effect;

	bool OnUserCreate() override {
		//initialize buffers
		prev_src.Create(ScreenWidth(), ScreenHeight());
		curr_src.Create(ScreenWidth(), ScreenHeight());
		stitch_src.Create(2*ScreenWidth(), ScreenHeight());
		target.Create(ScreenWidth(), ScreenHeight());

		//load effects
		try {
			struct EffectLoader {olc::Effect* eff; std::string str; };
			const std::vector<EffectLoader> effect_loaders{
				{&uv_effect, "assets/fx/uv.glsl"},
				{&identity_effect, "assets/fx/identity.glsl"},
				{&avg_effect, "assets/fx/avg.glsl"},
				{&difference_effect, "assets/fx/diff.glsl"}
			};
			for(auto& el:effect_loaders) {
				*el.eff=shader.MakeEffect(loadEffect(el.str));
				if(!el.eff->IsOK()) {
					std::cerr<<"  "<<el.str<<":\n"<<uv_effect.GetStatus();
					return false;
				}
			}
		} catch(const std::exception& e) {
			std::cerr<<"  "<<e.what()<<'\n';
			return false;
		}

		return true;
	}

	bool OnUserUpdate(float dt) override {
		//prev=curr
		shader.SetTargetDecal(prev_src.Decal());
		shader.Start(&identity_effect);
		shader.SetSourceDecal(curr_src.Decal());
		shader.DrawQuad({-1, -1}, {2, 2});
		shader.End();
		
		//draw to curr
		SetDrawTarget(curr_src.Sprite());
		//black background
		Clear(olc::BLACK);
		//circle at mouse
		FillCircle(GetMouseX(), GetMouseY(), 50, olc::WHITE);
		//draw to regular
		SetDrawTarget(nullptr);

		//update curr decal
		curr_src.Decal()->Update();

		//stitch sources together side by side
		shader.SetTargetDecal(stitch_src.Decal());
		shader.Start(&identity_effect);
		shader.SetSourceDecal(prev_src.Decal());
		shader.DrawQuad({-1, -1}, {1, 2});
		shader.SetSourceDecal(curr_src.Decal());
		shader.DrawQuad({0, -1}, {1, 2});
		shader.End();

		//apply difference
		shader.SetTargetDecal(target.Decal());
		shader.Start(&difference_effect);
		shader.SetSourceDecal(stitch_src.Decal());
		shader.DrawQuad({-1, -1}, {2, 2});
		shader.End();

		//show it
		DrawDecal({0,0}, target.Decal());

		return true;
	}
};

int main() {
	Demo d;
	bool vsync=true;
	if(d.Construct(640, 480, 1, 1, false, vsync)) d.Start();

	return 0;
}