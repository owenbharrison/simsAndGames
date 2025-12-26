/*TODO
make particles relative to area
better stage logic switch
randomize particle color?
emit cyan particles on level clear
check for close calls
*/

#define USE_SHADERS
#ifdef __EMSCRIPTEN__
#undef USE_SHADERS
#endif

#ifdef USE_SHADERS
#define OLC_GFX_OPENGL33
#endif
#define OLC_PGE_APPLICATION
#include "olc/include/olcPixelGameEngine.h"
using olc::vf2d;
using olc::vi2d;

#include "cmn/utils.h"

#include "cmn/geom/aabb.h"
namespace cmn { using AABB=AABB_generic<vf2d>; }

#include "cmn/stopwatch.h"

#include "particle.h"

#include "bullet.h"

#include "asteroid.h"

#include "ship.h"

#ifdef USE_SHADERS
#define OLC_PGEX_SHADERS
#include "olc/include/olcPGEX_Shaders.h"
#endif

class AsteroidsUI : public olc::PixelGameEngine {
	float particle_timer=0, bullet_timer=0, score_timer=0, warning_timer=0, end_timer=0;

	Ship ship;
	cmn::AABB screen_bounds;
	std::vector<Asteroid> asteroids;
	std::vector<Bullet> bullets;
	std::vector<Particle> particles;

	int score=0, level=0;
	enum State {
		Playing,
		Won,
		Lost
	} state=Playing;
	int warning_ct=0, end_ct=0;
	bool debug_mode=false;

	olc::Renderable console_tex;
	olc::Decal* font_dec=nullptr;

	olc::Renderable identity_tex;
	olc::Renderable crt_tex;

#pragma region SETUP_HELPERS
#ifdef USE_SHADERS
	olc::Shade shader;
	olc::Effect identity_effect, crt_effect;

	bool createEffect(olc::Effect& e, const std::string& filename) {
		//get file
		std::ifstream file(filename);
		if(file.fail()) return false;

		//dump contents into str stream
		std::stringstream mid;
		mid<<file.rdbuf();

		e=shader.MakeEffect({
			DEFAULT_VS,
			mid.str(),
			1,
			1
		});

		return e.IsOK();
	}
#endif

	bool setupShaders() {
#ifdef USE_SHADERS
		if(!createEffect(identity_effect, "assets/fx/identity.glsl")) {
			std::cerr<<"  unable to create effect.\n";

			return false;
		}

		if(!createEffect(crt_effect, "assets/fx/crt.glsl")) {
			std::cerr<<"  unable to create effect.\n";

			return false;
		}
#endif

		return true;
	}
#pragma endregion

	bool OnUserCreate() override {
		//extend screen bounds past edges
		{
			int margin=3;
			vi2d margin_vec(margin, margin);
			screen_bounds={-margin_vec, GetScreenSize()+margin_vec};
		}

		//ship in center of screen
		ship=Ship(GetScreenSize()/2, 2.5f);
		ship.rot=2*cmn::Pi*cmn::randFloat();

		console_tex.Create(ScreenWidth(), ScreenHeight());

		font_dec=new olc::Decal(GetFontSprite());

		//setup buffers
		identity_tex.Create(8*ScreenWidth(), 8*ScreenHeight());
		crt_tex.Create(8*ScreenWidth(), 8*ScreenHeight());

		if(!setupShaders()) return false;

		return true;
	}

	bool OnUserDestroy() override {
		delete[] font_dec;

		return true;
	}

#pragma region UPDATE_HELPERS
	void handleUserInput(float dt) {
		//if "alive"
		bool boosting=GetKey(olc::Key::UP).bHeld;
		if(state==Playing) {
			//ship movement
			if(boosting)  ship.boost(87.43f);

			//limit number of particles spawned
			while(particle_timer<0) {
				particle_timer+=.006f;
				if(boosting) particles.push_back(ship.emitParticle());
			}
			particle_timer-=dt;

			float turn_speed=2.78f;
			if(GetKey(olc::Key::RIGHT).bHeld) ship.turn(turn_speed*dt);
			if(GetKey(olc::Key::LEFT).bHeld) ship.turn(-turn_speed*dt);

			//limit number of bullets shot
			if(bullet_timer<0) {
				if(GetKey(olc::Key::SPACE).bPressed) {
					bullet_timer=.25f;
					bullets.push_back(ship.getBullet());
				}
			} else bullet_timer-=dt;
		}

		//whether or not to turn on debugMode
		if(GetKey(olc::Key::D).bPressed) debug_mode^=true;
	}

	void handleAsteroidBulletCollision() {
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
					particles.push_back(Particle::makeRandom(pos, olc::WHITE));
				}

				//finally, remove it
				ait=asteroids.erase(ait);
			} else ait++;
		}
		asteroids.insert(asteroids.end(), to_add.begin(), to_add.end());
	}

	void handleAsteroidShipCollision() {
		vf2d ship_outline[3];
		ship.getOutline(ship_outline[0], ship_outline[1], ship_outline[2]);
		for(const auto& a:asteroids) {
			if(state==Playing) {
				bool ship_hit=false;

				//check asteroid against all ship lines
				for(int i=0; i<3; i++) {
					vf2d sp0=a.worldToLocal(ship_outline[i]);
					vf2d sp1=a.worldToLocal(ship_outline[(i+1)%3]);

					//to all asteroid lines
					for(int j=0; j<a.num_pts; j++) {
						vf2d ap0=a.model[j];
						vf2d ap1=a.model[(j+1)%a.num_pts];
						vf2d tu=cmn::lineLineIntersection(sp0, sp1, ap0, ap1);

						//if even one hits, game over
						if(tu.x>0&&tu.x<1&&tu.y>0&&tu.y<1) {
							ship_hit=true;
							break;
						}
					}
				}
				if(ship_hit) {//end game
					state=Lost;
					int num_ptc=cmn::randInt(56, 84);
					for(int i=0; i<num_ptc; i++) {
						particles.push_back(Particle::makeRandom(ship.pos, olc::RED));
					}
				}
			}
		}
	}

	void handlePhysics(float dt) {
		//ship update
		ship.update(dt);
		ship.checkAABB(screen_bounds);

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

		//update asteroids
		for(auto& a:asteroids) {
			a.update(dt);
			a.checkAABB(screen_bounds);
		}

		handleAsteroidBulletCollision();

		handleAsteroidShipCollision();
	}

	//award points for living longer
	void handleScoring(float dt) {
		if(state==Playing) {
			if(score_timer<0) {
				score_timer+=2;
				//based on how "hard" to live
				score+=asteroids.size();
			}
			score_timer-=dt;
		}
	}

	//spawn new asteroids on clear
	void handleLevelling(float dt) {
		if(asteroids.empty()) {
			//win case
			if(level==4) state=Won;
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
	}

	void handleBlinking(float dt) {
		if(state!=Playing) {
			if(end_timer<0) {
				end_timer+=.7f;

				end_ct++;
			}

			end_timer-=dt;
		}
	}
#pragma endregion

	void update(float dt) {
		handleUserInput(dt);

		handlePhysics(dt);

		handleScoring(dt);

		handleLevelling(dt);

		handleBlinking(dt);
	}

#pragma region RENDER HELPERS
	//pack char into alpha component of color
	olc::Pixel packGlyph(char c, olc::Pixel col) {
		col.a=c;
		return col;
	}

	void ConsoleDrawString(const vi2d& pos, const std::string& str, olc::Pixel col) {
		for(int i=0; i<str.length(); i++) {
			const auto& ch=str[i];
			Draw({pos.x+i, pos.y}, packGlyph(ch, col));
		}
	}

	void renderDebug() {
		const auto red_glyph=packGlyph('.', olc::RED);
		const auto blue_glyph=packGlyph('.', olc::BLUE);
		const auto green_glyph=packGlyph('.', olc::GREEN);
		const auto grey_glyph=packGlyph('.', olc::GREY);

		//show asteroid pos, vel, & bounds
		for(const auto& a:asteroids) {
			DrawLine(a.pos, ship.pos, red_glyph);
			DrawLine(a.pos, a.pos+a.vel, blue_glyph);
			auto a_box=a.getAABB();
			DrawRect(a_box.min, a_box.max-a_box.min, grey_glyph);
		}

		//show bullet pos and vel
		for(const auto& b:bullets) {
			DrawLine(b.pos, ship.pos, green_glyph);
			DrawLine(b.pos, b.pos+b.vel, blue_glyph);
		}

		//show ship vel & bounds
		DrawLine(ship.pos, ship.pos+ship.vel, blue_glyph);
		auto ship_box=ship.getAABB();
		DrawRect(ship_box.min, ship_box.max-ship_box.min, grey_glyph);
	}

	//show bullets as tiny circles
	void renderBullets(const olc::Pixel& glyph) {
		for(const auto& b:bullets) {
			FillCircle(b.pos, 1, glyph);
		}
	}

	//show asteroid outlines
	void renderAsteroids(const olc::Pixel& glyph) {
		for(const auto& ast:asteroids) {
			for(int i=0; i<ast.num_pts; i++) {
				const auto& a=ast.localToWorld(ast.model[i]);
				const auto& b=ast.localToWorld(ast.model[(i+1)%ast.num_pts]);
				DrawLine(a, b, glyph);
			}
		}
	}

	//show ship outline
	void renderShip(const olc::Pixel& glyph) {
		vf2d pa, pb, pc;
		ship.getOutline(pa, pb, pc);
		DrawTriangle(pa, pb, pc, glyph);
	}

	//show particles as dots
	void renderParticles() {
		static const std::string ascii=" .:-=+*#%@";
		static const int ascii_size=ascii.length();

		for(const auto& p:particles) {
			//ramp to show how "young" or "vibrant"
			float pct=1-p.age/p.lifespan;
			int asi=cmn::clamp(int(ascii_size*pct), 0, ascii_size-1);
			Draw(p.pos, packGlyph(ascii[asi], p.col));
		}
	}

	//imminent asteroids overlay
	void renderWarningSign(const vi2d& ctr) {
		FillRect(ctr-vi2d(11, 2), {22, 5}, packGlyph(' ', olc::BLACK));
		DrawRect(ctr-vi2d(11, 2), {21, 4}, packGlyph('#', olc::CYAN));
		ConsoleDrawString(ctr-vi2d(9, 0), "ASTEROIDS IMMINENT", olc::CYAN);
	}

	//win lose overlay
	void renderEndSign(const vi2d& ctr) {
		FillRect(ctr-vi2d(11, 2), {21, 4}, packGlyph(' ', olc::BLACK));

		if(state==Won) {
			ConsoleDrawString(ctr-vi2d(4, 1), "You Win!", olc::GREEN);
			std::string str="Score: "+std::to_string(score);
			ConsoleDrawString(ctr-vi2d(str.length()/2, -1), str, olc::GREEN);
			DrawRect(ctr-vi2d(11, 2), {21, 4}, packGlyph('#', olc::GREEN));
		}

		if(state==Lost) {
			ConsoleDrawString(ctr-vi2d(5, 1), "GAME OVER!", olc::RED);
			ConsoleDrawString(ctr+vi2d(-10, 1), "You hit an asteroid.", olc::RED);
			DrawRect(ctr-vi2d(11, 2), {21, 4}, packGlyph('#', olc::RED));
		}
	}

	void renderStats(const olc::Pixel& col) {
		ConsoleDrawString({1, 1}, "Score: "+std::to_string(score), col);
		std::string level_str="Level: "+std::to_string(level);
		ConsoleDrawString(vi2d(ScreenWidth()-level_str.length()-1, 1), level_str, col);
	}

	void renderAsteroidStats() {
		ConsoleDrawString({ScreenWidth()-13, 0}, "Asteroid Data", olc::DARK_YELLOW);

		for(int i=0; i<asteroids.size(); i++) {
			const auto& a=asteroids.at(i);

			//show at top right
			std::string pos_str="[x:"+std::to_string((int)a.pos.x)+", y: "+std::to_string((int)a.pos.y)+"]";
			std::string str=std::to_string(i)+": [p: "+pos_str+", n: "+std::to_string(a.num_pts)+", r: "+std::to_string((int)a.base_rad)+"]";
			ConsoleDrawString(vi2d(ScreenWidth()-str.length(), i+1), str, olc::WHITE);
		}
	}
#pragma endregion

	void render(float dt) {
		//draw to console texture
		SetDrawTarget(console_tex.Sprite());

		//background
		FillRect({0, 0}, GetScreenSize(), packGlyph(' ', olc::BLACK));

		if(debug_mode) renderDebug();

		renderParticles();

		renderBullets(packGlyph('B', olc::WHITE));

		renderAsteroids(packGlyph('A', olc::WHITE));

		if(state!=Lost) renderShip(packGlyph('S', olc::WHITE));

		//screen center reference
		const vi2d ctr=GetScreenSize()/2;

		//blinking overlays
		if(warning_ct%2) renderWarningSign(ctr);
		if(end_ct%2) renderEndSign(ctr);

		renderStats(olc::CYAN);

		//only when debugging
		if(debug_mode) renderAsteroidStats();

		SetDrawTarget(nullptr);

#ifdef USE_SHADERS
		shader.SetTargetDecal(identity_tex.Decal());
		shader.Start(&identity_effect);
		shader.Clear(olc::Pixel(41, 41, 41));
#else
		FillRectDecal({0, 0}, GetScreenSize(), olc::Pixel(41, 41, 41));
#endif

		//render characters as string decals
		for(int i=0; i<ScreenWidth(); i++) {
			for(int j=0; j<ScreenHeight(); j++) {
				olc::Pixel col=console_tex.Sprite()->GetPixel(i, j);
				char c=col.a;
				col.a=255;
				int32_t ox=(c-32)%16;
				int32_t oy=(c-32)/16;
#ifdef USE_SHADERS
				shader.DrawPartialDecal(8*vf2d(i, j), font_dec, 8*vf2d(ox, oy), {8, 8}, {1, 1}, col);
#else
				DrawPartialDecal(vf2d(i, j), font_dec, 8*vf2d(ox, oy), {8, 8}, {.125f, .125f}, col);
#endif
			}
		}
#ifdef USE_SHADERS
		shader.End();

		//apply crt shader
		shader.SetTargetDecal(crt_tex.Decal());
		shader.Start(&crt_effect);
		shader.SetSourceDecal(identity_tex.Decal());
		shader.DrawQuad({-1, -1}, {2, 2});
		shader.End();

		//draw it
		DrawDecal({0, 0}, crt_tex.Decal(), {.125f, .125f});
#endif
	}

	bool OnUserUpdate(float dt) override {
		//quit on escape
		if(GetKey(olc::Key::ESCAPE).bPressed) return false;

		update(dt);

		render(dt);

		return true;
	}

public:
	AsteroidsUI() {
		sAppName="Console Asteroids";
	}
};

int main() {
	AsteroidsUI aui;
	bool fullscreen=false;
	bool vsync=true;
	//keep pixel size at 8, 8.
	if(aui.Construct(96, 54, 8, 8, fullscreen, vsync)) aui.Start();

	return 0;
}