#define OLC_GFX_OPENGL33
#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"

#include "common/shade_shader.h"

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
		olc::EffectConfig rainbow_sobel{
			DEFAULT_VS,
			//x&y sobel kernel
			"const int sobel[9]=int[9](-1,0,1,-2,0,2,-1,0,1);\n"
			//three phase shifted sine waves
			"const float Pi=3.1415927;\n"
			"vec3 colorWheel(float angle) {\n"
			"  return .5+.5*cos(angle+Pi*vec3(0,.667,-.667));\n"
			"}\n"
			"void main() {\n"
			"  vec2 sz=1./vec2(textureSize(tex1, 0));\n"
			"  vec2 sum=vec2(0);\n"
					//accumulate convolution
			"  for(int i=0; i<3; i++) {\n"
			"    for(int j=0; j<3; j++) {\n"
			"      vec2 offset_uv=sz*vec2(i, j);\n"
			"      vec4 col=texture(tex1, xUV1+offset_uv);\n"
							//luminance formula
			"      float lum=dot(col.rgb, vec3(.299, .587, .114));\n"
			"      sum+=lum*vec2(sobel[i+3*j], sobel[j+3*i]);\n"
			"    }\n"
			"  }\n"
			"  float mag=length(sum);\n"
					//which angle is edge?
			"  float angle=atan(sum.y, sum.x);\n"
			"  pix_out=vec4(mag*colorWheel(angle), 1);\n"
			"}\n",
			1,
			1,
		};
		effect=shade.MakeEffect(rainbow_sobel);
		if(!effect.IsOK()) {
			std::cout<<effect.GetStatus();

			return false;
		}

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
		//set ins/outs
		shade.SetSourceDecal(source_dec);
		shade.SetTargetDecal(target_dec);
		//render?
		shade.DrawDecal({0, 0}, source_dec);
		shade.End();

		Clear(olc::GREY);

		//show final render.
		DrawDecal({0, 0}, target_dec);

		return true;
	}
};

int main() {
	Demo d;
	bool vsync=false;
	if(d.Construct(400, 400, 1, 1, false, vsync)) d.Start();

	return 0;
}