#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include <fstream>
#include <sstream>
#include <list>
#include <algorithm>

float map(float x, float a, float b, float c, float d) {
	float t=(x-a)/(b-a);
	return c+t*(d-c);
}

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

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Wordle";
	}

	std::list<std::string> words;
	bool isWord(char word[5]) {
		//check every word
		for(const auto& w:words) {
			//check each letter
			bool is_word=true;
			for(int i=0; i<5; i++) {
				if(word[i]!=w[i]) {
					is_word=false;
					break;
				}
			}
			if(is_word) return true;
		}
		return false;
	}

	char target[5];
	char guesses[6][5];
	olc::Pixel guesses_col[6][5];
	int guess_index=0, word_index=0;

	bool show_cursor=true;
	float cursor_timer=0;

	enum {
		PLAYING,
		WON,
		LOST
	};

	int state=PLAYING;

	bool updateColors(int g) {
		int good=0;
		for(int i=0; i<5; i++) {
			char c=guesses[g][i];
			auto& col=guesses_col[g][i];
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
			}
		}
		return good==5;
	}

	void initGuesses() {
		for(int j=0; j<6; j++) {
			for(int i=0; i<5; i++) {
				guesses[j][i]=' ';
				guesses_col[j][i]=olc::GREY;
			}
		}
		guess_index=0;
		word_index=0;
	}

	bool OnUserCreate() override {
		initGuesses();

		//load all words
		std::ifstream file("assets/words.txt");
		if(file.fail()) {
			std::cout<<"error loading words\n";

			return false;
		}

		//read file into list
		for(std::string line; std::getline(file, line); ) {
			words.emplace_back(line);
		}
		file.close();

		//ask user to load previous game
		std::cout<<"do you want to resume? [y/n]\n";
		std::string resp; std::getline(std::cin, resp);
		if(resp=="y") {
			std::ifstream file("assets/save.txt");
			if(file.fail()) {
				std::cout<<"couldn't load save file\n";
				goto default_setup;
			}

			//get target
			std::string line;
			std::getline(file, line);
			std::stringstream line_str(line);
			int tar=0;
			for(int d; tar<5&&line_str>>d; tar++) {
				target[tar]=decrypt(d);
			}
			//bail if count is off
			if(tar!=5) {
				std::cout<<"invalid target\n";
				goto default_setup;
			}

			//load guesses
			int gue=0;
			for(; gue<6&&std::getline(file, line); gue++) {
				int num=line.length();
				if(num>5) {
					std::cout<<"invalid guess\n";
					goto default_setup;
				}
				for(int i=0; i<num; i++) {
					guesses[gue][i]=line[i];
					word_index=i;
				}
			}
			//bail if count is off
			if(gue!=6) {
				std::cout<<"invalid guesslist\n";
				goto default_setup;
			}

			//load indexes
			int idx=0;
			for(; idx<2&&std::getline(file, line); idx++) {
				if(!isNumber(line)) {
					std::cout<<"invalid index\n";
					goto default_setup;
				}
				int val=std::stoi(line);
				if(idx==0) guess_index=val;
				else word_index=val;
			}
			//bail if count is off
			if(idx!=2) {
				std::cout<<"invalid indexlist\n";
				goto default_setup;
			}

			//update all colors
			for(int i=0; i<guess_index; i++) {
				updateColors(i);
			}

			file.close();
			return true;
		}

default_setup:
		initGuesses();

		//choose random word
		srand(time(0));
		std::string rand_word;
		do {
			auto it=words.begin();
			std::advance(it, rand()%words.size());
			rand_word=*it;
		//ensure its 5 letters
		} while(rand_word.length()!=5);

		//copy into target
		for(int i=0; i<5; i++) target[i]=rand_word[i];

		return true;
	}

	bool OnUserDestroy() override {
		//save progress
		if(state==PLAYING) {
			std::ofstream file("assets/save.txt");
			if(file.fail()) {
				std::cout<<"couldn't save progress\n";
				return true;
			}

			std::cout<<"saving progress\n";

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

			file.close();
		}

		return true;
	}

	bool OnUserUpdate(float dt) override {
		Clear(olc::WHITE);

		//only allow input if playing
		if(state==PLAYING) {
			//check if letter pressed
			for(int i=0; i<26; i++) {
				auto k=(olc::Key)(olc::Key::A+i);
				if(GetKey(k).bPressed) {
					if(word_index<5) {
						guesses[guess_index][word_index]='a'+i;
						word_index++;
					}
				}
			}

			//guess
			if(GetKey(olc::ENTER).bPressed) {
				//ensure 5 letters
				if(word_index==5) {
					//is it even a word?
					if(isWord(guesses[guess_index])) {
						//update colors
						if(updateColors(guess_index)) state=WON;

						//reset & increment indexes
						word_index=0, guess_index++;
						if(state!=WON&&guess_index==6) state=LOST;
					} else std::cout<<"invalid word\n";
				}
			}

			//backspace
			if(GetKey(olc::Key::BACK).bPressed) {
				guesses[guess_index][word_index]=' ';
				if(word_index>0) word_index--;
			}

			//reset guess
			if(GetKey(olc::Key::DEL).bPressed) {
				word_index=0;
				for(int i=0; i<5; i++) guesses[guess_index][i]=' ';
			}
		}

		//cursor update
		if(cursor_timer<0) {
			cursor_timer+=1;
			show_cursor^=true;
		}
		cursor_timer-=dt;

		//RENDER
		switch(state) {
			case PLAYING: Clear(olc::WHITE); break;
			case LOST: Clear(olc::RED); break;
			case WON: Clear(olc::BLUE); break;
		}

		//show blocks and letters
		float dx=ScreenWidth()/5.f;
		float sx=.5f*dx;
		float dy=ScreenHeight()/6.f;
		float sy=.5f*dy;
		const vf2d box_sz(60, 60);
		const float scl=6;
		for(int i=0; i<5; i++) {
			float x=sx+dx*i;
			for(int j=0; j<6; j++) {
				float y=sy+dy*j;

				//show box
				FillRectDecal(vf2d(x, y)-box_sz/2, box_sz, guesses_col[j][i]);

				//show old words
				bool show_let=j<guess_index;
				//show current typing letters
				if(!show_let&&j==guess_index&&i<word_index) show_let=true;
				if(show_let) {
					DrawStringDecal(vf2d(x-4*scl, y-4*scl), std::string(1, guesses[j][i]), olc::BLACK, {scl, scl});
				}
			}
		}

		//show blinking cursor
		if(show_cursor) {
			float x=sx+dx*word_index;
			float y=sy+dy*guess_index;
			DrawStringDecal(vf2d(x-4*scl, y-4*scl), "_", olc::BLACK, {scl, scl});
		}

		return true;
	}
};

int main() {
	Example demo;
	if(demo.Construct(400, 500, 1, 1, false, true)) demo.Start();

	return 0;
}