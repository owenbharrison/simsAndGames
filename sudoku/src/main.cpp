#include "sudoku_ui.h"

int main() {
	SudokuUI game;
	if(game.Construct(400, 400, 1, 1, false, true)) game.Start();

	return 0;
}