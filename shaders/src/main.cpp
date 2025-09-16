#define OLC_GFX_OPENGL33
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_PGEX_SHADERS
#include "olcPGEX_Shaders.h"

#include <fstream>
#include <sstream>

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

	olc::Renderable source1, source2;
	olc::Renderable source12;
	olc::Renderable target;

	olc::Shade shader;
	olc::Effect uv_effect, identity_effect;
	olc::Effect avg_effect;

	bool OnUserCreate() override {
		//initialize buffers
		source1.Create(ScreenWidth(), ScreenHeight());
		source2.Create(ScreenWidth(), ScreenHeight());
		source12.Create(2*ScreenWidth(), ScreenHeight());
		target.Create(ScreenWidth(), ScreenHeight());
		
		//red circle on source1
		SetDrawTarget(source1.Sprite());
		Clear(olc::BLACK);
		FillCircle(200, 200, 50, olc::RED);
		source1.Decal()->Update();

		//blue rect on source2
		SetDrawTarget(source2.Sprite());
		Clear(olc::BLACK);
		FillRect(100, 100, 200, 200, olc::BLUE);
		source2.Decal()->Update();

		SetDrawTarget(nullptr);

		//load effect passes
		uv_effect=shader.MakeEffect(loadEffect("assets/fx/uv.glsl"));
		if(!uv_effect.IsOK()) {
			std::cout<<"uv_effect err:\n"<<uv_effect.GetStatus();
			return false;
		}
		identity_effect=shader.MakeEffect(loadEffect("assets/fx/identity.glsl"));
		if(!identity_effect.IsOK()) {
			std::cout<<"identity_effect err:\n"<<identity_effect.GetStatus();
			return false;
		}
		avg_effect=shader.MakeEffect(loadEffect("assets/fx/avg.glsl"));
		if(!avg_effect.IsOK()) {
			std::cout<<"avg_effect err:\n"<<avg_effect.GetStatus();
			return false;
		}

		return true;
	}

	bool OnUserUpdate(float dt) override {
		//stitch sources 1&2 into source12
		shader.SetTargetDecal(source12.Decal());
		shader.Start(&identity_effect);
		shader.SetSourceDecal(source1.Decal());
		shader.DrawQuad({-1, -1}, {1, 2});
		shader.SetSourceDecal(source2.Decal());
		shader.DrawQuad({0, -1}, {1, 2});
		shader.End();

		//apply average into target
		shader.SetTargetDecal(target.Decal());
		shader.Start(&avg_effect);
		shader.SetSourceDecal(source12.Decal());
		shader.DrawQuad({-1, -1}, {2, 2});
		shader.End();

		DrawDecal({0, 0}, target.Decal());

		return true;
	}
};

int main() {
	Demo d;
	bool vsync=true;
	if(d.Construct(400, 400, 1, 1, false, vsync)) d.Start();

	return 0;
}