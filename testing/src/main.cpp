#define OLC_GFX_OPENGL33
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#define OLC_PGEX_DEAR_IMGUI_IMPLEMENTATION
#include "olcPGEX_ImGui.h"

class Example : public olc::PixelGameEngine {
	olc::imgui::PGE_ImGUI pge_imgui;
	int game_layer=0;

	bool left_side=false;
	bool right_side=false;

	float background_col[3];

public:
	Example() : pge_imgui(true) {
		sAppName="ImGui testing";
	}

public:
	bool OnUserCreate() override {
		game_layer=CreateLayer();
		EnableLayer(game_layer, true);

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override {
		SetDrawTarget((uint8_t)game_layer);

		static float f=0.0f;
		static int counter=0;

		ImGui::Begin("Hello, world!");

		ImGui::Text("paragraph");
		ImGui::Checkbox("left_side", &left_side);
		ImGui::Checkbox("right_side", &right_side);

		ImGui::SliderFloat("float", &f, 0, 1);
		ImGui::ColorPicker3("col", background_col);

		ImGui::End();

		Clear(olc::PixelF(background_col[0], background_col[1], background_col[2]));

		{
			int margin=20;
			olc::Pixel left_col=left_side?olc::GREEN:olc::RED;
			FillRect(margin, margin, ScreenWidth()/2-3*margin/2, ScreenHeight()-2*margin, left_col);
			olc::Pixel right_col=right_side?olc::GREEN:olc::RED;
			DrawRect(ScreenWidth()/2+margin/2, margin, ScreenWidth()/2-3*margin/2, ScreenHeight()-2*margin, right_col);
		}

		return true;
	}
};

int main() {
	Example demo;
	if(demo.Construct(640, 480, 1, 1)) demo.Start();

	return 0;
}