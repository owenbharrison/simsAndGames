#include "wordle_ui.h"

int main() {
	WordleUI wg;
	bool vsync=true;
	if(wg.Construct(400, 500, 1, 1, false, vsync)) wg.Start();

	return 0;
}