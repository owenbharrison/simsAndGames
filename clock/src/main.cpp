#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_SOUNDWAVE
#include "common/olcSoundWaveEngine.h"

#include <thread>
#include <chrono>
#include <ctime>
#include <fstream>

//these might be overkill...
//but this is the easy way
#include <functional>
#include <atomic>

static const unsigned long segment_write_time=20;
static const unsigned long time_between_segments=60;

static std::atomic<bool> stop_clock=false;

void delay(unsigned long ms) {
	using namespace std::chrono;

	auto start=steady_clock::now();
	while(duration_cast<milliseconds>(steady_clock::now()-start).count()<=ms) {
		if(stop_clock) break;
	}
}

//segment writing direction
//-1 is the stop flag
const int8_t digits_fancy[10][7]={
	{00, 01, 02, 03, 04, 05, -1},//0
	{01, 02, -1, -1, -1, -1, -1},//1
	{00, 01, 06, 04, 03, -1, -1},//2
	{00, 01, 06, 02, 03, -1, -1},//3
	{05, 06, 01, 02, -1, -1, -1},//4
	{00, 05, 06, 02, 03, -1, -1},//5
	{00, 05, 04, 03, 02, 06, -1},//6
	{00, 01, 02, -1, -1, -1, -1},//7
	{00, 05, 06, 01, 04, 03, 02},//8 proto g
	{06, 05, 00, 01, 02, -1, -1}//9
};

struct Mod {
	std::atomic<uint8_t> state=0;
	vf2d pos;
	std::function<void()> click;

	void seg_on(uint8_t i) {
		uint8_t cmd=1<<i;
		//if was off, click
		if(!(cmd&state)) click();
		state|=cmd;
	}

	void seg_off(uint8_t i) {
		uint8_t cmd=1<<i;
		//if was on, click
		if(cmd&state) click();
		state&=~cmd;
	}

	void clear() {
		for(uint8_t s=0; s<7; s++) {
			seg_off(s);
			delay(segment_write_time);
		}
	}

	void displayDigit(uint8_t dig, bool onoff) {
		//for each step
		for(uint8_t i=0; i<7; i++) {
			//get segment
			int8_t s=digits_fancy[dig][i];
			if(s==-1) break;

			if(onoff) seg_on(s);
			else seg_off(s);
			delay(segment_write_time);
			delay(time_between_segments);
		}

		delay(100);
	}
};

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="7 Segment Clock";
	}

	//graphics stuff
	vf2d mod_sz;
	olc::Sprite* seg_spr[7]{0, 0, 0, 0, 0, 0, 0};
	olc::Decal* seg_dec[7]{0, 0, 0, 0, 0, 0, 0};

	//sound stuff
	olc::sound::WaveEngine sound_engine;
	olc::sound::Wave click_sound[10];

	//thread & flag
	std::thread clock_prog;
	std::atomic<bool> start_loop=false;

#pragma region CLOCK PROGRAM
	Mod mod[4];

	//write extended 0
	void doLoop(uint8_t st, uint8_t en) {
		//write, then clear
		for(int8_t f=1; f>=0; f--) {
			int8_t m=st;
			for(int8_t s=0; s<6; s++) {
				//pulse seg
				if(f) mod[m].seg_on(s);
				else mod[m].seg_off(s);
				delay(segment_write_time);
				delay(time_between_segments);

				//segment halt logic
				if(s==0&&m!=en) s--, m++;//top
				else if(s==3&&m!=st) s--, m--;//bottom

				if(stop_clock) return;
			}
		}
	}

	void getTime(uint8_t& hour, uint8_t& minute) {
		std::time_t t=std::time(nullptr);
		std::tm local_tm;

#ifdef _WIN32
		localtime_s(&local_tm, &t); // Windows-safe localtime
#elif defined(__linux__) || defined(__APPLE__)
		localtime_r(&t, &local_tm); // Linux/macOS-safe localtime
#endif

		hour=local_tm.tm_hour;
		minute=local_tm.tm_min;
	}
	
	//low level formatting
	uint8_t hour_high=0, hour_low=0, minute_high=0, minute_low=0;
	void formatTime(uint8_t hour, uint8_t minute, uint8_t& hh, uint8_t& hl, uint8_t& mh, uint8_t& ml) {
		//24 hour time -> 12 hour time
		if(hour>12) hour-=12;

		//tens and ones places
		hh=hour/10;
		hl=hour%10;
		mh=minute/10;
		ml=minute%10;
	}

	void clock_setup() {
		//clear old
		for(uint8_t m=0; m<4; m++) {
			mod[m].clear();
		}

		//fancy loops
		doLoop(0, 1);
		doLoop(2, 3);

		//init time
		uint8_t hour, minute;
		getTime(hour, minute);
		formatTime(hour, minute, hour_high, hour_low, minute_high, minute_low);

		if(hour_high) mod[0].displayDigit(hour_high, true);
		mod[1].displayDigit(hour_low, true);
		mod[2].displayDigit(minute_high, true);
		mod[3].displayDigit(minute_low, true);
	}

	void clock_loop() {
		//get time
		uint8_t hour, minute;
		getTime(hour, minute);

		//if between 8AM and 10PM: update every min
		//else: update every hour
		if((hour>=8&&hour<=22)||minute==0) {
			//format time
			uint8_t hh, hl, mh, ml;
			formatTime(hour, minute, hh, hl, mh, ml);

			//set if changed
			bool set_hh=hh!=hour_high;
			bool set_hl=hl!=hour_low;
			bool set_mh=mh!=minute_high;
			bool set_ml=ml!=minute_low;

			//clear old backwards
			if(set_ml) mod[3].displayDigit(minute_low, false);
			if(set_mh) mod[2].displayDigit(minute_high, false);
			if(set_hl) mod[1].displayDigit(hour_low, false);
			if(set_hh&&hour_high) mod[0].displayDigit(hour_high, false);

			//set new
			hour_high=hh;
			hour_low=hl;
			minute_high=mh;
			minute_low=ml;

			//write new
			if(set_hh&&hour_high) mod[0].displayDigit(hour_high, true);
			if(set_hl) mod[1].displayDigit(hour_low, true);
			if(set_mh) mod[2].displayDigit(minute_high, true);
			if(set_ml) mod[3].displayDigit(minute_low, true);
		}

		//this will be off by a second at most
		delay(1000);
	}
#pragma endregion 

	bool OnUserCreate() override {
#pragma region SIMULATION ASSETS
		//load segment sprites
		for(uint8_t s=0; s<7; s++) {
			std::string file="assets/seg"+std::to_string(s)+".png";
			seg_spr[s]=new olc::Sprite(file);
			seg_dec[s]=new olc::Decal(seg_spr[s]);
		}
		mod_sz=vf2d(seg_spr[0]->width, seg_spr[0]->height);

		//load click sounds
		sound_engine.InitialiseAudio();
		sound_engine.SetOutputVolume(.7f);
		for(uint8_t i=0; i<10; i++) {
			std::string file="assets/click"+std::to_string(i)+".wav";
			if(!click_sound[i].LoadAudioWaveform(file)) {
				std::cout<<"error loading audio\n";
				return false;
			}
		}

		//load old states
		std::ifstream file("assets/mod_states.txt");
		if(file.fail()) {
			std::cout<<"couldn't load states\n";
			return false;
		}
		for(uint8_t m=0; m<4; m++) {
			std::string line;
			if(!std::getline(file, line)||line.length()!=7) {
				std::cout<<"error loading state\n";
				return false;
			}
			for(uint8_t s=0; s<7; s++) {
				if(line[s]=='1') mod[m].state|=1<<s;
			}
		}
#pragma endregion

		//init module positions
		float dx=ScreenWidth()/4.f;
		float x=.5f*dx;
		float y=.5f*ScreenHeight();
		for(uint8_t m=0; m<4; m++) {
			mod[m].pos=vf2d(x, y)-.5f*mod_sz;
			x+=dx;
		}

		//init module callbacks
		auto click_cb=[this] () {
			//play random segment flip noise
			int i=rand()%10;
			sound_engine.PlayWaveform(&click_sound[i]);
		};
		for(uint8_t m=0; m<4; m++) {
			mod[m].click=click_cb;
		}

		//start clock_prog
		clock_prog=std::thread([&] () {
			clock_setup();
			start_loop=true;
		});

		return true;
	}

	bool OnUserDestroy() override {
		//close clock_prog
		stop_clock=true;
		clock_prog.join();

		//save module states
		std::ofstream file("assets/mod_states.txt");
		if(file.fail()) {
			std::cout<<"couldn't save state\n";
			return true;
		}
		for(uint8_t m=0; m<4; m++) {
			for(uint8_t s=0; s<7; s++) {
				if((1<<s)&mod[m].state) file<<'1';
				else file<<'0';
			}
			file<<'\n';
		}

		//free sprites
		for(uint8_t s=0; s<7; s++) {
			delete seg_spr[s];
			delete seg_dec[s];
		}

		return true;
	}

	bool OnUserUpdate(float dt) override {
		Clear(olc::GREY);

		//show corresponding digits
		for(uint8_t m=0; m<4; m++) {
			FillRectDecal(mod[m].pos, mod_sz, olc::BLACK);
			for(uint8_t s=0; s<7; s++) {
				if((1<<s)&mod[m].state) DrawDecal(mod[m].pos, seg_dec[s]);
			}
		}

		//if setup over, start loop
		if(start_loop) {
			start_loop=false;

			clock_prog.join();
			clock_prog=std::thread([&] () {
				while(!stop_clock) clock_loop();
			});
		}

		return true;
	}
};

int main() {
	Example demo;
	if(demo.Construct(600, 250, 1, 1, false, true)) demo.Start();

	return 0;
}