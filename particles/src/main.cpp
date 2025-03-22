#include "particles_ui.h"

int main() {
	ParticlesUI pui;
	if(pui.Construct(720, 640, 1, 1, false, true)) pui.Start();

	return 0;
}