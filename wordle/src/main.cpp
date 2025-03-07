#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "wordle.h"

float map(float x, float a, float b, float c, float d) {
	float t=(x-a)/(b-a);
	return c+t*(d-c);
}

struct WordleGame : olc::PixelGameEngine {
	WordleGame() {
		sAppName="Wordle";
	}
	
	Wordle game;

	bool show_cursor=false;
	float cursor_timer=0;

	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;

	bool OnUserCreate() override {
		//primitive to draw with
		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);

		//choose random word
		srand(time(0));
		const char* rand_word=WORDS[rand()%NUM_WORDS];

		//copy into target
		for(int i=0; i<5; i++) game.target[i]=rand_word[i];

		return true;
	}

	bool OnUserDestroy() override {
		delete prim_rect_dec;
		delete prim_rect_spr;

		return true;
	}

	void resetCursor() {
		show_cursor=false;
		cursor_timer=0;
	}

	void update(float dt) {
		if(GetKey(olc::Key::CTRL).bHeld) {
			const std::string filename="assets/save.txt";

			//saving
			if(GetKey(olc::Key::S).bPressed) {
				if(game.save(filename)) {
					std::cout<<"saved to: "<<filename<<'\n';
				} else std::cout<<"unable to save\n";
			}

			//loading
			if(GetKey(olc::Key::L).bPressed) {
				Wordle other;
				if(other.load(filename)) {
					game=other;
					std::cout<<"loaded from: "<<filename<<'\n';
				} else std::cout<<"unable to load\n";
			}
		}
		//only allow input if playing
		else if(game.state==Wordle::PLAYING) {
			//check if letter pressed
			for(int i=0; i<26; i++) {
				auto k=(olc::Key)(olc::Key::A+i);
				if(GetKey(k).bPressed) {
					resetCursor();
					game.type('a'+i);
				}
			}

			//guess
			if(GetKey(olc::ENTER).bPressed) {
				resetCursor();

				game.guess();
			}

			//backspace
			if(GetKey(olc::Key::BACK).bPressed) {
				resetCursor();
				game.backspace();
			}

			//reset guess
			if(GetKey(olc::Key::DEL).bPressed) {
				resetCursor();
				game.resetGuess();
			}
		}

		//every now and then,
		if(cursor_timer>.5f) {
			cursor_timer-=.5f;
			//toggle on/off
			show_cursor^=true;
		}
		cursor_timer+=dt;
	}

	void DrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float rad, olc::Pixel col) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=(sub/len).perp();

		float angle=std::atan2f(sub.y, sub.x);
		DrawRotatedDecal(a-rad*tang, prim_rect_dec, angle, {0, 0}, {len, 2*rad}, col);
	}

	void DrawThickRect(const olc::vf2d& pos, const olc::vf2d& sz, float rad, olc::Pixel col) {
		FillRectDecal(pos, {rad, sz.y}, col);//left
		FillRectDecal(pos, {sz.x, rad}, col);//top
		FillRectDecal({pos.x+sz.x-rad, pos.y}, {rad, sz.y}, col);//right
		FillRectDecal({pos.x, pos.y+sz.y-rad}, {sz.x, rad}, col);//bottom
	}

	void render() {
		switch(game.state) {
			case Wordle::PLAYING: Clear(olc::WHITE); break;
			case Wordle::LOST: Clear(olc::RED); break;
			case Wordle::WON: Clear(olc::BLUE); break;
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
				vf2d pos=vf2d(x, y)-box_sz/2;
				DrawThickRect(pos, box_sz, 3, olc::DARK_GREY);
				bool fill_rect=j<game.guess_index;
				if(fill_rect) FillRectDecal(pos, box_sz, game.colors[j][i]);

				//show current typing letters
				if(fill_rect||j==game.guess_index&&i<game.word_index) {
					DrawStringDecal(vf2d(x-4*scl, y-4*scl), std::string(1, game.guesses[j][i]), olc::BLACK, {scl, scl});
				}
			}
		}

		//show blinking cursor
		if(game.state==Wordle::PLAYING&&show_cursor) {
			float x=sx+dx*game.word_index;
			float y=sy+dy*game.guess_index;
			DrawStringDecal(vf2d(x-4*scl, y-4*scl), "_", olc::BLACK, {scl, scl});
		}
	}

	bool OnUserUpdate(float dt) override {
		update(dt);

		render();

		return true;
	}
};

int main() {
	WordleGame wg;
	if(wg.Construct(400, 500, 1, 1, false, true)) wg.Start();

	return 0;
}