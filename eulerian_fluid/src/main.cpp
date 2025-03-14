#include "fluid_ui.h"

int main() {
	FluidUI f;
	bool vsync=true;
	if(f.Construct(480, 360, 1, 1, false, vsync)) f.Start();

	return 0;
}