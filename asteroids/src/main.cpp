/*TODO
add crt effect
*/

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;
using olc::vi2d;

#include "common/utils.h"

#include "common/aabb.h"
namespace cmn { using AABB=AABB_generic<vf2d>; }

#include "common/stopwatch.h"

#include "particle.h"

#include "bullet.h"

#include "asteroid.h"

#include "ship.h"

class AsteroidsUI : public olc::PixelGameEngine {
	olc::Renderable console_texture;
	olc::Decal* font_dec=nullptr;

	float particle_timer=0, bullet_timer=0, score_timer=0, warning_timer=0, end_timer=0;

	Ship ship;
	cmn::AABB screen_bounds;
	std::vector<Asteroid> asteroids;
	std::vector<Bullet> bullets;
	std::vector<Particle> particles;

	int score=0, level=0;
	enum Stage {
		Playing,
		Won,
		Lost
	} stage=Playing;
	int warning_ct=0, end_ct=0;
	bool debug_mode=false;

public:
	AsteroidsUI() {
		sAppName="Console Asteroids";
	}

	bool OnUserCreate() override {
		console_texture.Create(ScreenWidth(), ScreenHeight());

		font_dec=new olc::Decal(GetFontSprite());

		//extend screen bounds past edges
		{
			int margin=4;
			vi2d margin_vec(margin, margin);
			screen_bounds={-margin_vec, GetScreenSize()+margin_vec};
		}

		//ship in center of screen
		ship=Ship(GetScreenSize()/2, 3);

		return true;
	}

	bool OnUserDestroy() override {
		delete[] font_dec;

		return true;
	}

	void update(float dt) {
		//whether or not to turn on debugMode
		if(GetKey(olc::Key::D).bPressed) debug_mode^=true;

		//update particles
		for(auto it=particles.begin(); it!=particles.end(); ) {
			auto& p=*it;
			p.update(dt);

			//"dynamically" clear those too old
			if(p.isDead()) {
				it=particles.erase(it);
			} else it++;
		}

		//update bullets
		for(auto it=bullets.begin(); it!=bullets.end(); ) {
			auto& b=*it;
			b.update(dt);

			//"dynamically" clear those offscreen
			if(!screen_bounds.contains(b.pos)) {
				it=bullets.erase(it);
			} else it++;
		}

		//if "alive"
		bool boosting=GetKey(olc::Key::UP).bHeld;
		if(stage==Playing) {
			//ship movement
			if(boosting) ship.boost(87.43f);
			float turnSpeed=2.78f;
			if(GetKey(olc::Key::RIGHT).bHeld) ship.turn(turnSpeed*dt);
			if(GetKey(olc::Key::LEFT).bHeld) ship.turn(-turnSpeed*dt);

			//ship update
			ship.update(dt);
			ship.checkAABB(screen_bounds);
		}

		//update asteroids
		for(auto& a:asteroids) {
			a.update(dt);
			//when hit edge spawn on other side
			a.checkAABB(screen_bounds);
		}

		//asteroids vs bullets
		{
			std::vector<Asteroid> to_add;
			for(auto ait=asteroids.begin(); ait!=asteroids.end();) {
				auto& a=*ait;

				//check all bullets
				bool hit_bullet=false;
				for(auto bit=bullets.begin(); bit!=bullets.end(); bit++) {
					if(a.contains(bit->pos)) {
						//delete bullet
						bullets.erase(bit);
						hit_bullet=true;
						break;
					}
				}

				if(hit_bullet) {
					//if we can, split the asteroid
					int num_ptc=0;
					vf2d pos;
					Asteroid a0, a1;
					if(a.split(a0, a1)) {
						to_add.push_back(a0);
						to_add.push_back(a1);
						//emit some particles for fx
						num_ptc=a.base_rad*cmn::randFloat(2, 4);
						pos=a.pos;

						//increment score
						score+=8;
					} else {//when we "fully" break an asteroid
						//emit more particles
						num_ptc=a.base_rad*cmn::randFloat(5, 7);
						pos=a.pos;

						//increment score more
						score+=24;
					}

					//spawn particles
					for(int k=0; k<num_ptc; k++) {
						particles.push_back(Particle::makeRandom(pos));
					}

					//finally, remove it
					ait=asteroids.erase(ait);
				} else ait++;
			}
			asteroids.insert(asteroids.end(), to_add.begin(), to_add.end());
		}

		//asteroids vs ship
		{
			vf2d ship_outline[3];
			ship.getOutline(ship_outline[0], ship_outline[1], ship_outline[2]);
			for(const auto& a:asteroids) {
				if(stage==Playing) {
					bool ship_hit=false;
					//check asteroid against all ship lines
					for(int i=0; i<3; i++) {
						vf2d sp0=ship_outline[i];
						vf2d sp1=ship_outline[(i+1)%3];
						//to all asteroid lines
						for(int j=0; j<a.num_pts; j++) {
							vf2d ap0=a.points[j];
							vf2d ap1=a.points[(j+1)%a.num_pts];
							vf2d tu=cmn::lineLineIntersection(sp0, sp1, ap0, ap1);
							//if even one hits, game over
							if(tu.x>0&&tu.x<1&&tu.y>0&&tu.y<1) {
								ship_hit=true;
								break;
							}
						}
					}
					if(ship_hit) {//end game
						stage=Lost;
						int num_ptc=cmn::randInt(56, 84);
						for(int i=0; i<num_ptc; i++) {
							particles.push_back(Particle::makeRandom(ship.pos));
						}
					}
				}
			}
		}

		if(stage!=Lost) {
			//limit number of particles spawned
			if(particle_timer<0) {
				particle_timer+=.003f;
				if(boosting) particles.push_back(ship.emitParticle());
			}
			particle_timer-=dt;

			//limit number of bullets shot
			if(GetKey(olc::Key::SPACE).bPressed&&bullet_timer<0) {
				bullet_timer=.25f;
				bullets.push_back(ship.getBullet());
			}
			bullet_timer-=dt;
		}

		if(stage==Playing) {
			//award points for living longer
			if(score_timer<0) {
				score_timer+=2;
				//based on how "hard" to live
				score+=asteroids.size();
			}
			score_timer-=dt;
		}

		//"leveling"
		if(asteroids.empty()) {
			//win case
			if(level==4) stage=Won;
			else {
				//blinking
				if(warning_timer<0) {
					//reset
					warning_timer=.4f;
					if(warning_ct<8) warning_ct++;
					else {//add asteroids, next level
						warning_ct=0;

						//add asteroids based on level
						for(int i=0; i<1+2*level; i++) asteroids.push_back(Asteroid::makeRandom(screen_bounds));
						level++;
					}
				}
				warning_timer-=dt;
			}
		}

		//game over
		if(stage!=Playing) {
			if(end_timer<0) {
				end_timer=.7f;
				end_ct++;
			}
		}
		end_timer-=dt;
	}

#pragma region RENDER HELPERS
	void ConsoleDraw(const vi2d& p, char ch, olc::Pixel col) {
		col.a=ch;
		Draw(p, col);
	}

	void ConsoleDrawLine(const vi2d& a, const vi2d& b, char ch, olc::Pixel col) {
		col.a=ch;
		DrawLine(a, b, col);
	}

	void ConsoleDrawRect(const vi2d& pos, const vi2d& sz, char ch, olc::Pixel col) {
		col.a=ch;
		DrawRect(pos, sz, col);
	}

	void ConsoleFillRect(const vi2d& pos, const vi2d& sz, char ch, olc::Pixel col) {
		col.a=ch;
		FillRect(pos, sz, col);
	}

	void ConsoleDrawTriangle(const vi2d& a, const vi2d& b, const vi2d& c, char ch, olc::Pixel col) {
		col.a=ch;
		DrawTriangle(a, b, c, col);
	}

	void ConsoleDrawString(const vi2d& pos, const std::string& str, olc::Pixel col) {
		for(int i=0; i<str.length(); i++) {
			const auto& ch=str[i];
			col.a=ch;
			ConsoleDraw({pos.x+i, pos.y}, ch, col);
		}
	}
#pragma endregion

	void render(float dt) {
		//draw to console texture
		SetDrawTarget(console_texture.Sprite());
		
		//background
		ConsoleFillRect({0, 0}, GetScreenSize(), ' ', olc::BLACK);

		//if debugging
		if(debug_mode) {
			//line to all asteroids
			for(const auto& a:asteroids) {
				ConsoleDrawLine(a.pos, ship.pos, '.', olc::RED);
			}

			//line to all bullets
			for(const auto& b:bullets) {
				ConsoleDrawLine(b.pos, ship.pos, '.', olc::GREEN);
			}

			//show asteroids vel
			for(const auto& a:asteroids) {
				ConsoleDrawLine(a.pos, a.pos+a.vel, '.', olc::BLUE);
			}

			//show bullets vel
			for(const auto& b:bullets) {
				ConsoleDrawLine(b.pos, b.pos+b.vel, '.', olc::BLUE);
			}

			//show ship vel
			ConsoleDrawLine(ship.pos, ship.pos+ship.vel, '.', olc::BLUE);

			//show asteroid bounds
			for(const auto& a:asteroids) {
				auto a_box=a.getAABB();
				ConsoleDrawRect(a_box.min, a_box.max-a_box.min, '.', olc::GREY);
			}

			//show ship bounds
			auto ship_box=ship.getAABB();
			ConsoleDrawRect(ship_box.min, ship_box.max-ship_box.min, '.', olc::GREY);
		}

		//show particles
		for(const auto& p:particles) {
			static const std::string ascii=" .:-=+*#%@";
			static const int ascii_size=ascii.length();
			//ramp to show how "young" or "vibrant"
			float pct=1-p.age/p.lifespan;
			int asi=cmn::clamp(int(ascii_size*pct), 0, ascii_size-1);
			ConsoleDraw(p.pos, ascii[asi], olc::Pixel(255, 100, 0));
		}

		//show all bullets
		for(const auto& b:bullets) {
			ConsoleFillRect(b.pos-1, {3, 3}, 'B', olc::WHITE);
		}

		//show asteroids
		for(const auto& ast:asteroids) {
			for(int i=0; i<ast.num_pts; i++) {
				const auto& a=ast.points[i];
				const auto& b=ast.points[(i+1)%ast.num_pts];
				ConsoleDrawLine(a, b, 'A', olc::WHITE);
			}
		}

		if(stage!=Lost) {
			//show ship
			vf2d a, b, c;
			ship.getOutline(a, b, c);
			ConsoleDrawTriangle(a, b, c, 'S', olc::WHITE);
		}

		//screen center reference
		const vi2d ctr=GetScreenSize()/2;

		//show warning sign
		if(warning_ct%2) {
			ConsoleFillRect(ctr-vi2d(11, 2), {22, 5}, ' ', olc::BLACK);
			ConsoleDrawRect(ctr-vi2d(11, 2), {21, 4}, '#', olc::CYAN);
			ConsoleDrawString(ctr-vi2d(9, 0), "ASTEROIDS IMMINENT", olc::CYAN);
		}

		//show end sign
		if(end_ct%2) {
			ConsoleFillRect(ctr-vi2d(11, 2), {21, 4}, ' ', olc::BLACK);
			if(stage==Won) {
				ConsoleDrawString(ctr-vi2d(4, 1), "You Win!", olc::GREEN);
				std::string str="Score: "+std::to_string(score);
				ConsoleDrawString(ctr-vi2d(str.length()/2, -1), str, olc::GREEN);
			}
			if(stage==Lost) {
				ConsoleDrawString(ctr-vi2d(5, 1), "GAME OVER!", olc::RED);
				ConsoleDrawString(ctr+vi2d(-10, 1), "You hit an asteroid.", olc::RED);
			}
			ConsoleDrawRect(ctr-vi2d(11, 2), {21, 4}, '#', olc::RED);
		}

		//show stats
		{
			ConsoleDrawString({0, 0}, "Score: "+std::to_string(score), olc::CYAN);
			std::string level_str="Level: "+std::to_string(level);
			ConsoleDrawString(vi2d(ScreenWidth()-level_str.length(), 0), level_str, olc::CYAN);
		}

		//only when debugging
		if(debug_mode) {
			//display asteroid stats
			ConsoleDrawString({ScreenWidth()-13, 0}, "Asteroid Data", olc::DARK_YELLOW);
			for(int i=0; i<asteroids.size(); i++) {
				const auto& a=asteroids.at(i);
				//show at top right
				std::string pos_str="[x:"+std::to_string((int)a.pos.x)+", y: "+std::to_string((int)a.pos.y)+"]";
				std::string str=std::to_string(i)+": [p: "+pos_str+", n: "+std::to_string(a.num_pts)+", r: "+std::to_string((int)a.base_rad)+"]";
				ConsoleDrawString(vi2d(ScreenWidth()-str.length(), i+1), str, olc::WHITE);
			}
		}

		SetDrawTarget(nullptr);

		//render characters as string decals
		{
			const vf2d source_size(8, 8);
			const vf2d scale(.125f, .125f);
			for(int j=0; j<ScreenHeight(); j++) {
				for(int i=0; i<ScreenWidth(); i++) {
					olc::Pixel col=console_texture.Sprite()->GetPixel(i, j);
					char c=col.a;
					col.a=255;
					int32_t ox=(c-32)%16;
					int32_t oy=(c-32)/16;
					DrawPartialDecal(vf2d(i, j), font_dec, vf2d(8*ox, 8*oy), source_size, scale, col);
				}
			}
		}
	}

	bool OnUserUpdate(float dt) override {
		//quit on escape
		if(GetKey(olc::Key::ESCAPE).bPressed) return false;
		
		update(dt);

		render(dt);

		return true;
	}
};

int main() {
	AsteroidsUI aui;
	bool fullscreen=false;
	bool vsync=false;
	if(aui.Construct(128, 72, 8, 8, fullscreen, vsync)) aui.Start();

	return 0;
}