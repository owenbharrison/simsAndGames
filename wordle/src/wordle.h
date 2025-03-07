#pragma once
#ifndef WORDLE_STRUCT_H
#define WORDLE_STRUCT_H

#include <fstream>
#include <sstream>

#include "words.h"

//easy to crack, but i just want to make sure
//the user doesnt accidentally see the target
int encrypt(char c) {
	return 3*c;
}
char decrypt(int i) {
	return i/3;
}

//string full of digits?
bool isNumber(const std::string& s) {
	if(s.empty()) return false;

	//if NOT num, bail
	for(const auto& c:s) {
		if(c<'0'||c>'9') return false;
	}
	return true;
}

bool isWord(char word[5]) {
	//check every word
	for(int i=0; i<NUM_WORDS; i++) {
		//check each letter
		bool is_word=true;
		for(int j=0; j<5; j++) {
			if(word[j]!=WORDS[i][j]) {
				is_word=false;
				break;
			}
		}
		if(is_word) return true;
	}
	return false;
}

struct Wordle {
	char target[5];

	char guesses[6][5];
	olc::Pixel colors[6][5];

	int guess_index=0, word_index=0;

	enum {
		PLAYING=0,
		WON,
		LOST
	};
	int state=PLAYING;

	Wordle() {
		//init guesses and colors
		for(int j=0; j<6; j++) {
			for(int i=0; i<5; i++) {
				guesses[j][i]=' ';
				colors[j][i]=olc::WHITE;
			}
		}
		for(int i=0; i<5; i++) target[i]=' ';
	}

	bool updateColors(int g) {
		int good=0;
		for(int i=0; i<5; i++) {
			char c=guesses[g][i];
			auto& col=colors[g][i];
			//if there are n of this letter,
			int num=0;
			for(int j=0; j<5; j++) {
				if(target[j]==c) num++;
			}
			//is this the 1->nth occurance?
			int occur=0;
			for(int j=0; j<=i; j++) {
				if(guesses[g][j]==c) occur++;
			}
			if(occur<=num) {
				col=olc::YELLOW;
				if(target[i]==c) good++, col=olc::GREEN;
			} else col=olc::GREY;
		}
		return good==5;
	}

	void type(char c) {
		if(word_index<5) {
			guesses[guess_index][word_index]=c;
			word_index++;
		}
	}

	void backspace() {
		guesses[guess_index][word_index]=' ';
		if(word_index>0) word_index--;
	}

	void resetGuess() {
		word_index=0;
		for(int i=0; i<5; i++) guesses[guess_index][i]=' ';
	}

	//return whether valid?
	bool guess() {
		//ensure 5 letters
		if(word_index!=5) return false;

		//is it even a word?
		if(!isWord(guesses[guess_index])) return false;

		//update colors
		if(updateColors(guess_index)) state=WON;

		//reset & increment indexes
		word_index=0, guess_index++;
		if(state!=WON&&guess_index==6) state=LOST;
	}

	bool load(const std::string& filename) {
		std::ifstream file(filename);
		if(file.fail()) return false;

		//get target
		std::string line;
		std::getline(file, line);
		std::stringstream line_str(line);
		int tar=0;
		for(int d; tar<5&&line_str>>d; tar++) {
			target[tar]=decrypt(d);
		}
		//bail if count is off
		if(tar!=5) return false;

		//load guesses
		int gue=0;
		for(; gue<6&&std::getline(file, line); gue++) {
			int num=line.length();
			if(num>5) return false;

			for(int i=0; i<num; i++) {
				guesses[gue][i]=line[i];
				word_index=i;
			}
		}
		//bail if count is off
		if(gue!=6) return false;

		//load indexes
		int idx=0;
		for(; idx<2&&std::getline(file, line); idx++) {
			if(!isNumber(line)) return false;

			int val=std::stoi(line);
			if(idx==0) guess_index=val;
			else word_index=val;
		}
		//bail if count is off
		if(idx!=2) return false;

		//update all colors
		for(int i=0; i<guess_index; i++) {
			updateColors(i);
		}

		return true;
	}

	bool save(const std::string& filename) {
		//dont save full games.
		if(state!=PLAYING) return false;

		std::ofstream file(filename);
		if(file.fail()) return false;

		//encrypt target
		for(int i=0; i<5; i++) {
			file<<encrypt(target[i])<<' ';
		}
		file<<'\n';

		//write guesses
		for(int j=0; j<6; j++) {
			for(int i=0; i<5; i++) {
				file<<guesses[j][i];
			}
			file<<'\n';
		}

		//write indexes
		file<<guess_index<<'\n';
		file<<word_index<<'\n';

		return true;
	}
};
#endif//WORDLE_STRUCT_H