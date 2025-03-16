#pragma once
#ifndef SUDOKU_STRUCT_H
#define SUDOKU_STRUCT_H

#include <fstream>
#include <string>

struct Sudoku {
	int grid[9][9];
	bool possible[9][9][9];

	Sudoku() {
		for(int i=0; i<9; i++) {
			for(int j=0; j<9; j++) {
				grid[i][j]=0;
				for(int k=0; k<9; k++) {
					possible[i][j][k]=true;
				}
			}
		}
	}

	//solver functions?
	void collapseKnown(int ci, int cj) {
		if(grid[ci][cj]) {
			for(int i=0; i<9; i++) possible[ci][cj][i]=false;
		}
	}

	void collapseRow(int ci, int cj) {
		for(int j=0; j<9; j++) {
			int n=grid[ci][j];
			if(n) possible[ci][cj][n-1]=false;
		}
	};

	void collapseColumn(int ci, int cj) {
		for(int i=0; i<9; i++) {
			int n=grid[i][cj];
			if(n) possible[ci][cj][n-1]=false;
		}
	};

	void collapseBox(int ci, int cj) {
		int di=ci/3*3, dj=cj/3*3;
		for(int i=0; i<3; i++) {
			for(int j=0; j<3; j++) {
				int n=grid[i+di][j+dj];
				if(n) possible[ci][cj][n-1]=false;
			}
		}
	};

	bool loadFromFile(const std::string& filename) {
		std::ifstream file(filename);
		if(file.fail()) return false;

		for(int i=0; i<9; i++) {
			std::string line;
			std::getline(file, line);
			for(int j=0; j<9&&j<line.length(); j++) {
				if(line[j]>='1'&&line[j]<='9') grid[i][j]=line[j]-'0';
			}
		}

		return true;
	}

	bool saveToFile(const std::string& filename) const {
		std::ofstream file(filename);
		if(file.fail()) return false;

		for(int i=0; i<9; i++) {
			for(int j=0; j<9; j++) {
				if(grid[i][j]==0) file<<'_';
				else file<<grid[i][j];
			}
			file<<'\n';
		}

		return true;
	}
};
#endif//SUDOKU_STRUCT_H