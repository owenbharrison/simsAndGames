#include "pixel_game.h"

int main() {
	PixelGame t;
	bool vsync=true;
	if(t.Construct(800, 640, 1, 1, false, vsync)) t.Start();

	return 0;
}