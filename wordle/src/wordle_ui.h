#define OLC_PGE_APPLICATION
#include "common/olcPixelGameEngine.h"
using olc::vf2d;

#include "wordle.h"

#include "message_effect.h"

#include "common/utils.h"
namespace cmn{
	vf2d polar(float rad, float angle) {
		return polar_generic<vf2d>(rad, angle);
	}
}

struct WordleUI : olc::PixelGameEngine {
	WordleUI() {
		sAppName="Wordle";
	}

	Wordle game;

	bool show_cursor=false;
	float cursor_timer=0;

	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;

	std::list<MessageEffect> msg_fx;
	vf2d gravity;

	vf2d block_step, block_sz;
	float letter_scl=0;

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

		//setup transforms
		block_step.x=ScreenWidth()/5.f;
		block_step.y=ScreenHeight()/6.f;
		block_sz=vf2d(60, 60);
		letter_scl=6;

		gravity={0, 32};

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

	void addEffect(std::string msg, olc::Pixel col, vf2d pos) {
		//random velocities
		float angle=cmn::random(2*cmn::Pi);
		float speed=cmn::random(25, 50);
		vf2d vel=cmn::polar(speed, angle);
		float rot_vel=cmn::random(-cmn::Pi, cmn::Pi);

		//random size
		float scl=cmn::random(5, 10);

		//random time
		float lifespan=cmn::random(3, 5);
		msg_fx.emplace_back(msg, col, pos, vel, rot_vel, scl, lifespan);
	}

	void update(float dt) {
		if(GetKey(olc::Key::CTRL).bHeld) {
			const std::string filename="assets/save.txt";

			//saving
			if(GetKey(olc::Key::S).bPressed) {
				if(game.save(filename)) {
					vf2d pos(ScreenWidth()/2, ScreenHeight()/2);
					addEffect("saved", olc::BLUE, pos);
					std::cout<<"saved to: "<<filename<<'\n';
				} else std::cout<<"unable to save\n";
			}

			//loading
			if(GetKey(olc::Key::L).bPressed) {
				Wordle other;
				if(other.load(filename)) {
					game=other;
					vf2d pos(ScreenWidth()/2, ScreenHeight()/2);
					addEffect("loaded", olc::GREEN, pos);
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
					char letter='a'+i;
					if(game.type(letter)) {
						std::string msg(1, letter);
						olc::Pixel col(rand()%255, rand()%255, rand()%255);
						vf2d pos(block_step.x*(game.word_index-.5f), block_step.y*(.5f+game.guess_index));
						addEffect(msg, col, pos);
					}
				}
			}

			//guess
			if(GetKey(olc::ENTER).bPressed) {
				resetCursor();
				if(game.guess()) {
					std::string msg;
					for(int i=0; i<5; i++) msg+=game.guesses[game.guess_index-1][i];
					olc::Pixel col(rand()%255, rand()%255, rand()%255);
					vf2d pos(ScreenWidth()/2, block_step.y*(.5f+game.guess_index));
					addEffect(msg, col, pos);
				}
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

		//sanitize messages
		for(auto it=msg_fx.begin(); it!=msg_fx.end();) {
			if(!it->isDead()) it++;
			else it=msg_fx.erase(it);
		}

		//kinematics
		for(auto& m:msg_fx) {
			m.applyForce(gravity);

			m.update(dt);
		}
	}

	void DrawThickLine(const olc::vf2d& a, const olc::vf2d& b, float rad, olc::Pixel col) {
		olc::vf2d sub=b-a;
		float len=sub.mag();
		olc::vf2d tang=sub.perp()/len;

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

		//show blocks
		for(int j=0; j<6; j++) {
			float y=block_step.y*(.5f+j);
			for(int i=0; i<5; i++) {
				float x=block_step.x*(.5f+i);

				//show box
				vf2d pos=vf2d(x, y)-block_sz/2;
				DrawThickRect(pos, block_sz, 3, olc::DARK_GREY);
				if(j<game.guess_index) FillRectDecal(pos, block_sz, game.colors[j][i]);
			}
		}

		//show messages
		for(const auto& m:msg_fx) {
			vf2d offset(4*m.msg.length(), 4);
			float t=1-m.age/m.lifespan;
			olc::Pixel fill=m.col;
			fill.a=255*cmn::clamp(t, 0.f, 1.f);
			DrawRotatedStringDecal(m.pos, m.msg, m.rot, offset, fill, {m.scl, m.scl});
		}

		//show letters
		for(int i=0; i<5; i++) {
			float x=block_step.x*(.5f+i);
			for(int j=0; j<6; j++) {
				float y=block_step.y*(.5f+j);
				if(j<game.guess_index||(j==game.guess_index&&i<game.word_index)) {
					DrawStringDecal(vf2d(x-4*letter_scl, y-4*letter_scl), std::string(1, game.guesses[j][i]), olc::BLACK, {letter_scl, letter_scl});
				}
			}
		}

		//show blinking cursor
		if(game.state==Wordle::PLAYING&&show_cursor) {
			float x=block_step.x*(.5f+game.word_index);
			float y=block_step.y*(.5f+game.guess_index);
			DrawStringDecal(vf2d(x-4*letter_scl, y-4*letter_scl), "_", olc::BLACK, {letter_scl, letter_scl});
		}
	}

	bool OnUserUpdate(float dt) override {
		update(dt);

		render();

		return true;
	}
};