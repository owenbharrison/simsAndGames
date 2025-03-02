#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_SOUNDWAVE
#include "common/olcSoundWaveEngine.h"

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Example";
	}

	vf2d mod_sz;
	olc::Sprite* seg_spr[7]{0, 0, 0, 0, 0, 0, 0};
	olc::Decal* seg_dec[7]{0, 0, 0, 0, 0, 0, 0};

	olc::sound::WaveEngine sound_engine;
	olc::sound::Wave click_sound[10];

	//which segments ON for a given digit?
	const unsigned char digits[10]{
		0b00111111,//0
		0b00000110,//1
		0b01011011,//2
		0b01001111,//3
		0b01100110,//4
		0b01101101,//5
		0b01111101,//6
		0b00000111,//7
		0b01111111,//8
		0b01100111//9
	};

	struct Mod {
		unsigned char state=0;
		vf2d pos;
	};
	Mod mod[4];

	float count_timer=0;
	int count=0;

	bool OnUserCreate() override {
		//load sprites for segments
		for(int i=0; i<7; i++) {
			std::string file="assets/seg"+std::to_string(i)+".png";
			seg_spr[i]=new olc::Sprite(file);
			seg_dec[i]=new olc::Decal(seg_spr[i]);
		}
		mod_sz=vf2d(seg_spr[0]->width, seg_spr[0]->height);

		//load noise
		sound_engine.InitialiseAudio();
		sound_engine.SetOutputVolume(.7f);
		for(int i=0; i<10; i++) {
			std::string file="assets/click"+std::to_string(i)+".wav";
			if(!click_sound[i].LoadAudioWaveform(file)) {
				std::cout<<"error loading audio\n";

				return false;
			}
		}

		//initialize modules
		float dx=ScreenWidth()/4.f;
		float x=.5f*dx-.5f*mod_sz.x;
		float y=.5f*ScreenHeight()-.5f*mod_sz.y;
		for(int i=0; i<4; i++) {
			mod[i]={0, vf2d(x, y)};
			x+=dx;
		}

		return true;
	}

	bool OnUserDestroy() override {
		for(int i=0; i<7; i++) {
			delete seg_spr[i];
			delete seg_dec[i];
		}

		return true;
	}

	bool OnUserUpdate(float dt) override {
		//increment every now and then
		if(count_timer<0) {
			count_timer+=.25f;

			count++;
			if(count>=10000) count=0;
		}
		count_timer-=dt;
		
		//update states
		for(int m=0; m<4; m++) {
			//low level formatting
			int n;
			switch(m) {
				case 0: n=count/1000; break;
				case 1: n=(count%1000)/100; break;
				case 2: n=(count%100)/10; break;
				case 3: n=count%10; break;
			}
			//play sound if changed?
			if(mod[m].state!=digits[n]) {
				int r=rand()%10;
				sound_engine.PlayWaveform(&click_sound[r]);
			}
			mod[m].state=digits[n];
		}

		Clear(olc::GREY);

		//show corresponding digits
		for(int m=0; m<4; m++) {
			FillRectDecal(mod[m].pos, mod_sz, olc::BLACK);
			for(int s=0; s<7; s++) {
				if((1<<s)&mod[m].state) DrawDecal(mod[m].pos, seg_dec[s]);
			}
		}

		return true;
	}
};

int main() {
	Example demo;
	if(demo.Construct(600, 250, 1, 1, false, true)) demo.Start();

	return 0;
}