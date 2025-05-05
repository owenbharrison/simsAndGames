#define OLC_SOUNDWAVE
#include "olcSoundWaveEngine.h"

#include <string>
#include <chrono>
#include <thread>

int main() {
#pragma region setup
	srand(time(0));

	//init audio
	olc::sound::WaveEngine sound_engine;
	sound_engine.InitialiseAudio();
	sound_engine.SetOutputVolume(.2f);

	//load octave
	olc::sound::Wave notes[8];
	for(int i=0; i<8; i++) {
		//find corresponding note
		char c, n='1';
		switch(i) {
			case 0: c='c'; break;
			case 1: c='d'; break;
			case 2: c='e'; break;
			case 3: c='f'; break;
			case 4: c='g'; break;
			case 5: c='a'; break;
			case 6: c='b'; break;
			case 7: c='c', n++; break;
		}

		//construct filename
		std::string filename="assets/";
		filename+=c, filename+=n;
		filename+=".wav";

		//load the sucker
		if(!notes[i].LoadAudioWaveform(filename)) {
			std::cout<<"error loading "<<filename<<'\n';

			return false;
		}
	}

	//setup random combos
	unsigned char combos[26];
	for(int i=0; i<26; i++) {
		//choose two unique bits
		int a=rand()%8, b;
		do b=rand()%8;
		while(a==b);

		//bitwise create combo
		combos[i]=(1<<a)|(1<<b);

		//is it unique?
		for(int j=0; j<i; j++) {
			if(combos[j]==combos[i]) {
				//try again!
				i--;
				break;
			}
		}
	}

	//print combos
	std::cout<<"combos:\n";
	for(int i=0; i<26; i++) {
		for(int j=7; j>=0; j--) {
			if(1&(combos[i]>>j)) std::cout<<'1';
			else std::cout<<'0';
		}
		std::cout<<'\n';
	}

	auto playCombo=[&](int i) {
		//given each bit, play corresponding notes
		for(int j=0; j<8; j++) {
			if(1&(combos[i]>>j)) sound_engine.PlayWaveform(&notes[j]);
		}
	};

	auto delay=[](unsigned long ms) {
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	};
#pragma endregion

	//use command line args to send/receive

	std::cout<<"what would you like to send?\n";

	//for receive, use fourier transform to analyze waveforms.

	//prompt user
	std::string msg;
	std::getline(std::cin, msg);

	//for each letter
	for(const auto& l:msg) {
		if(l>='a'&&l<='z') playCombo(l-'a');
		else if(l>='A'&&l<='Z') playCombo(l-'A');
		else if(l==' ') delay(500);

		delay(500);
	}

	return 0;
}