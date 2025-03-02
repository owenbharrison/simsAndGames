#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

struct Example : olc::PixelGameEngine {
	Example() {
		sAppName="Example";
	}

	olc::Sprite* seg_spr[7]{0, 0, 0, 0, 0, 0, 0};
	olc::Decal* seg_dec[7]{0, 0, 0, 0, 0, 0, 0};

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

	int count=0;
	float count_timer=0;

	bool OnUserCreate() override {
		for(int i=0; i<7; i++) {
			std::string file="assets/seg_"+std::to_string(1+i)+".png";
			seg_spr[i]=new olc::Sprite(file);
			seg_dec[i]=new olc::Decal(seg_spr[i]);
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
		//increment every second logic
		if(count_timer>1) {
			count_timer-=1;

			count++;
			if(count>=9) count=0;
		}
		count_timer+=dt;
		
		Clear(olc::GREY);

		//show corresponding digits
		for(int i=0; i<7; i++) {
			if((1<<i)&digits[count]) DrawDecal({0, 0}, seg_dec[i]);
		}

		return true;
	}
};

int main() {
	Example demo;
	if(demo.Construct(600, 300, 1, 1, false, true)) demo.Start();

	return 0;
}