#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include <fstream>
#include <list>
#include <algorithm>

float map(float x, float a, float b, float c, float d) {
	float t=(x-a)/(b-a);
	return c+t*(d-c);
}

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Example";
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

	bool OnUserCreate() override {
		std::ifstream file("assets/words.txt");
		if(file.fail()) {
			std::cout<<"error loading file\n";
			return true;
		}

		//read file into list
		for(std::string line; std::getline(file, line); ) {
			words.emplace_back(line);
		}

		//choose random word
		//ensure its 5 letters
		srand(time(0));
		std::string rand_word;;
		do {
			auto it=words.begin();
			std::advance(it, rand()%words.size());
			rand_word=*it;
		} while(rand_word.length()!=5);

		//init guesses
		for(int j=0; j<6; j++) {
			for(int i=0; i<5; i++) {
				guesses[j][i]=' ';
				guesses_col[j][i]=olc::GREY;
			}
		}

		//copy into target
		for(int i=0; i<5; i++) target[i]=rand_word[i];

		return true;
	}

	bool OnUserUpdate(float dt) override {
		Clear(olc::WHITE);
		
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
			//are we out of guesses?
			
			//ensure 5 letters
			if(word_index==5) {
				//is it even a word?
				if(isWord(guesses[guess_index])) {
					//update colors
					for(int i=0; i<5; i++) {
						guesses_col[guess_index][i]=olc::YELLOW;
					}

					//reset & increment indexes
					word_index=0, guess_index++;
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

		//cursor update
		if(cursor_timer<0) {
			cursor_timer+=1;
			show_cursor^=true;
		}
		cursor_timer-=dt;

		Clear(olc::WHITE);

		//show blocks and letters
		float dx=ScreenWidth()/5.f;
		float sx=.5f*dx;
		float dy=ScreenHeight()/6.f;
		float sy=.5f*dy;
		const vf2d box_sz(50, 50);
		const float scl=5;
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