#define OLC_GFX_OPENGL33
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#define OLC_PGEX_DEAR_IMGUI_IMPLEMENTATION
#include "imgui/imgui_impl_pge.h"

class Example : public olc::PixelGameEngine {
	olc::imgui::PGE_ImGUI pge_imgui;
	int game_layer=0;

public:
	//PGE_ImGui can automatically call the SetLayerCustomRenderFunction by passing
	//true into the constructor.  false is the default value.
	Example() : pge_imgui(false) {
		sAppName="Test Application";
	}

public:
	bool OnUserCreate() override {
		//Create a new Layer which will be used for the game
		game_layer=CreateLayer();
		//The layer is not enabled by default,  so we need to enable it
		EnableLayer(game_layer, true);

		//Set a custom render function on layer 0.  Since DrawUI is a member of
		//our class, we need to use std::bind
		//If the pge_imgui was constructed with _register_handler = true, this line is not needed
		SetLayerCustomRenderFunction(0, std::bind(&Example::DrawUI, this));

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override {
		//Change the Draw Target to not be Layer 0
		SetDrawTarget((uint8_t)game_layer);

		//Create and react to your UI here, it will be drawn during the layer draw function
		ImGui::ShowDemoWindow();

		return true;
	}

	void DrawUI(void) {
		//This finishes the Dear ImGui and renders it to the screen
		pge_imgui.ImGui_ImplPGE_Render();
	}
};

int main() {
	Example demo;
	if(demo.Construct(640, 360, 2, 2)) demo.Start();

	return 0;
}