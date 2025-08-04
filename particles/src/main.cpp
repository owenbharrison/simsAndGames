#include "particles_ui.h"

int main() {
	ParticlesUI pui;
	bool vsync=false;
	if(pui.Construct(720, 480, 1, 1, false, vsync)) pui.Start();

	return 0;
}