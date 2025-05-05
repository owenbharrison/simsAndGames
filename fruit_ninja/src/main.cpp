/*todo:
ui
* layers
* menu
cosmetic
* textures
*	uv coords in fruit
* update split()
scoring
* add score
* add bombs
* add particles
* add lives
* game over
* restart
*/

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#define OLC_SOUNDWAVE
#include "olcSoundWaveEngine.h"

#include "fruit.h"

float clamp(float x, float a, float b) {
	if(x<a) return a;
	if(x>b) return b;
	return x;
}

#include "particles.h"

struct FruitNinja : olc::PixelGameEngine {
	FruitNinja() {
		sAppName="Fruit Ninja";
	}

	//user input stuff
	vf2d mouse_pos;

	float total_time=0;

	std::vector<vf2d> addition;
	float addition_timer=0;

	std::list<vf2d> slice;

	const float total_ultra=100;
	float ultra=total_ultra;
	float ultra_reload=0;

	//actual fruit storage
	std::list<Fruit*> fruits;
	float throw_timer=0;

	vf2d gravity{0, 64};
	cmn::AABB screen_bounds;

	//visual things
	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;

	bool show_bounds=false;

	std::list<Particle*> particles;

	//sound stuff
	olc::sound::WaveEngine sound_engine;
	olc::sound::Wave throw_sound, slice1_sound, slice2_sound;

	bool OnUserCreate() override {
		srand(time(0));

		{//"primitive" to draw with
			int sz=1024;
			prim_circ_spr=new olc::Sprite(sz, sz);
			SetDrawTarget(prim_circ_spr);
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2);
			SetDrawTarget(nullptr);
			prim_circ_dec=new olc::Decal(prim_circ_spr);
		}

		screen_bounds={{0, 0}, GetScreenSize()};

		//sounds
		sound_engine.InitialiseAudio();
		sound_engine.SetOutputVolume(.8f);

		if(!throw_sound.LoadAudioWaveform("assets/throw.wav")) {
			std::cout<<"couldn't load audio\n";
			return false;
		}

		if(!slice1_sound.LoadAudioWaveform("assets/slice1.wav")) {
			std::cout<<"couldn't load audio\n";
			return false;
		}

		if(!slice2_sound.LoadAudioWaveform("assets/slice2.wav")) {
			std::cout<<"couldn't load audio\n";
			return false;
		}

		return true;
	}

	void reset() {
		for(const auto& f:fruits) delete f;
		fruits.clear();
	}

	bool OnUserDestroy() override {
		reset();

		return true;
	}

	void handleUserInput(float dt) {
		//draw new objects
		const auto a_key=GetKey(olc::Key::A);
		if(a_key.bPressed) {
			addition.clear();
		}
		if(addition_timer<0) {
			addition_timer=.1f;

			if(a_key.bHeld) {
				//make sure points cant be too close
				bool exist=false;
				for(const auto& a:addition) {
					if((a-mouse_pos).mag()<1) exist=true;
				}
				if(!exist) addition.emplace_back(mouse_pos);
			}
		}
		addition_timer-=dt;
		if(a_key.bReleased) {
			//sanitize user input
			if(addition.size()>3) {
				fruits.emplace_back(new Fruit(addition, olc::WHITE));
			}
			addition.clear();
		}

		//ultra ability: slow down time
		if(GetKey(olc::Key::SHIFT).bHeld) {
			//can we even use it?
			if(ultra>0) {
				//use it up
				ultra-=17*dt;

				//reset reload timer
				ultra_reload=1;

				//clamp values
				if(ultra<0) {
					ultra=0;

					//take longer to regain
					ultra_reload=3;
				}

				//actual ability
				dt/=10;
			}
		}
		//after some time, reload ability.
		if(ultra_reload<0) {
			ultra+=10*dt;
			//clamp values
			if(ultra>100) ultra=100;
		}
		ultra_reload-=dt;

		//slice objects
		const auto s_key=GetKey(olc::Key::S);
		if(s_key.bPressed) slice.clear();
		if(s_key.bHeld) {
			//make sure points cant be too close
			bool exist=false;
			for(const auto& s:slice) {
				if((s-mouse_pos).mag()<1) exist=true;
			}
			if(!exist) slice.emplace_back(mouse_pos);
		}
		if(s_key.bReleased) {
			std::vector<vf2d> slice_vec(slice.begin(), slice.end());

			//store temporary new fruits
			Fruit fa, fb;
			for(auto it=fruits.begin(); it!=fruits.end();) {
				const auto& f=*it;
				if(f->slice(slice_vec, fa, fb)) {
					//add splatter near fruit
					{
						cmn::AABB box=f->getAABB();
						vf2d ctr=box.getCenter();
						float rad=(box.max-ctr).mag();
						int num=cmn::random(20, 35);
						for(int i=0; i<num; i++) {
							float pos_rad=cmn::random(rad);
							vf2d offset=cmn::polar(pos_rad, cmn::random(2*cmn::Pi));
							float speed=cmn::random(5, 15);
							vf2d vel=cmn::polar(speed, cmn::random(2*cmn::Pi));
							float lifespan=cmn::random(2, 3);
							float rad=cmn::random(3, 4);
							particles.emplace_back(new Splatter(ctr+offset, vel, f->col, lifespan, rad));
						}
					}

					//add message
					{
						//determine score
						int pct=100*fa.mass/(fa.mass+fb.mass);
						//how close to 50/50?
						int diff=abs(50-pct);

						//string feedback
						std::string msg; olc::Pixel col;
						if(diff<5) msg="awesome!!", col=olc::BLUE;
						else if(diff<10) msg="nice!", col=olc::CYAN;
						else if(diff<20) msg="decent", col=olc::GREEN;
						else if(diff<30) msg="meh.", col=olc::YELLOW;
						else msg="bad!", col=olc::RED;

						//random movement
						float speed=cmn::random(50, 75);
						vf2d vel=cmn::polar(speed, cmn::random(2*cmn::Pi));
						float rot_vel=cmn::random(-.5f*cmn::Pi, .5f*cmn::Pi);
						float lifespan=cmn::random(2, 4);
						particles.push_back(new Message(f->pos, vel, col, lifespan, msg, rot_vel));
					}

					//update new fruit velocities
					{
						float speed_a=cmn::random(40, 70);
						fa.vel=cmn::polar(speed_a, cmn::random(2*cmn::Pi));
						fa.rot_vel=cmn::random(-.5f*cmn::Pi, .5f*cmn::Pi);
						float speed_b=cmn::random(40, 70);
						fb.vel=cmn::polar(speed_b, cmn::random(2*cmn::Pi));
						fb.rot_vel=cmn::random(-.5f*cmn::Pi, .5f*cmn::Pi);
						//point opposite directions
						if(fa.vel.dot(fb.vel)>0) fb.vel*=-1;
						if(fa.rot_vel*fb.rot_vel>0) fb.rot_vel*=-1;
					}

					//add new at front to avoid reslice
					fruits.emplace_front(new Fruit(fa));
					fruits.emplace_front(new Fruit(fb));

					//remove old fruit
					delete f;
					it=fruits.erase(it);

					//play random slide noise
					if(cmn::random()<.5f) sound_engine.PlayWaveform(&slice1_sound);
					else sound_engine.PlayWaveform(&slice2_sound);
				} else it++;
			}

			slice.clear();
		}

		//remove objects
		if(GetKey(olc::Key::X).bPressed) {
			for(auto it=fruits.begin(); it!=fruits.end(); ) {
				const auto& f=*it;
				if(f->contains(mouse_pos)) {
					delete f;
					it=fruits.erase(it);
				} else it++;
			}
		}

		//visual toggles
		if(GetKey(olc::Key::B).bPressed) show_bounds^=true;
	}

	void automaticUpdate(float dt) {
		//every now and then throw a fruit
		if(throw_timer<0) {
			throw_timer=cmn::random(1, 4);

			//make random polygon
			int num=4+rand()%16;
			float r_min=cmn::random(20, 30);
			float r_max=cmn::random(40, 60);
			Fruit f(num);
			for(int i=0; i<num; i++) {
				vf2d dir=cmn::polar(1, 2*cmn::Pi*i/num);
				float r=cmn::random(r_min, r_max);
				f.pts[i]=r*dir;
			}
			f.init();

			cmn::AABB box=f.getAABB();
			//place on bottom
			f.pos.y=screen_bounds.max.y-box.max.y;
			//random along bottom
			f.pos.x=cmn::random(-box.min.x, screen_bounds.max.x-box.max.x);

			//random velocity
			f.vel.x=cmn::random(-50, 50);
			f.vel.y=cmn::random(-220, -150);
			f.rot_vel=cmn::random(-.5f*cmn::Pi, .5f*cmn::Pi);

			//random col	
			f.col=olc::Pixel(rand()%255, rand()%255, rand()%255);

			//add the sucker
			fruits.emplace_back(new Fruit(f));

			sound_engine.PlayWaveform(&throw_sound);
		}
		throw_timer-=dt;

		//keep slice short
		if(slice.size()) {
			bool is_first=true;
			vf2d prev;
			float length=0;
			for(const auto& s:slice) {
				if(is_first) is_first=false;
				else length+=(s-prev).mag();
				prev=s;
			}
			if(length>300.f) slice.pop_front();
		}

		//"sanitize" fruits
		for(auto it=fruits.begin(); it!=fruits.end();) {
			const auto& f=*it;
			//is fruit offscreen?
			if(!screen_bounds.overlaps(f->getAABB())) {
				//remove the sucker
				delete f;
				it=fruits.erase(it);
			} else it++;
		}

		//"sanitize" messages
		for(auto it=particles.begin(); it!=particles.end();) {
			const auto& p=*it;
			if(p->isDead()) {
				delete p;
				it=particles.erase(it);
			} else it++;
		}
	}

	void handlePhysics(float dt) {
		//fruit kinematics
		for(const auto& f:fruits) {
			f->applyForce(f->mass*gravity);
			f->update(dt);
		}

		//particle kinematics
		for(const auto& p:particles) {
			p->update(dt);
		}
	}

	bool update(float dt) {
		mouse_pos=GetMousePos();

		total_time+=dt;

		if(!IsConsoleShowing()) handleUserInput(dt);

		//open built in console
		if(GetKey(olc::Key::ESCAPE).bPressed) ConsoleShow(olc::Key::ESCAPE);

		automaticUpdate(dt);

		if(!IsConsoleShowing()) handlePhysics(dt);
	}

	bool OnConsoleCommand(const std::string& line) {
		std::stringstream line_str(line);
		std::string cmd; line_str>>cmd;

		if(cmd=="clear") ConsoleClear();

		//english + logic = grammar :D
		else if(cmd=="count") {
			int num=fruits.size();
			std::cout<<"there ";
			std::cout<<(num==1?"is ":"are ");
			std::cout<<num<<" fruit";
			if(num!=1) std::cout<<'s';
			std::cout<<'\n';
		}

		else if(cmd=="reset") {
			int f_num=fruits.size();
			std::cout<<"removed "<<f_num<<" fruit";
			if(f_num!=1) std::cout<<'s';
			std::cout<<'\n';

			reset();
		}

		return true;
	}

	void FillCircleDecal(const olc::vf2d& pos, float rad, olc::Pixel col) {
		olc::vf2d offset(rad, rad);
		olc::vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	void render() {
		Clear(olc::GREY);

		//show addition
		for(int i=0; i<addition.size(); i++) {
			const auto& a=addition[i];
			const auto& b=addition[(i+1)%addition.size()];
			DrawLineDecal(a, b, olc::CYAN);
		}

		//show all fruits
		for(const auto& f:fruits) {
			//show bounding box rectangle
			if(show_bounds) {
				cmn::AABB box=f->getAABB();
				DrawRectDecal(box.min, box.max-box.min, olc::RED);
			}

			//initialize indexes
			std::list<int> indexes;
			for(int i=0; i<f->getNum(); i++) indexes.emplace_back(i);

			//ear clipping triangulation
			for(auto curr=indexes.begin(); curr!=indexes.end();) {
				//get previous and next points
				auto prev=std::prev(curr==indexes.begin()?indexes.end():curr);
				auto next=std::next(curr); if(next==indexes.end()) next=indexes.begin();
				const auto& pt_p=f->pts[*prev];
				const auto& pt_c=f->pts[*curr];
				const auto& pt_n=f->pts[*next];

				//make sure this is a convex angle
				if((pt_p-pt_c).cross(pt_n-pt_c)>0) {
					curr++;
					continue;
				}

				//make sure this triangle doesnt contain any pts
				bool contains=false;
				for(auto other=std::next(curr); other!=indexes.end(); other++) {
					//dont check self
					if(other==next) continue;

					//is this point to the left/right of all trilines
					const auto& pt_o=f->pts[*other];
					bool side1=(pt_c-pt_p).cross(pt_o-pt_p)>0;
					bool side2=(pt_n-pt_c).cross(pt_o-pt_c)>0;
					bool side3=(pt_p-pt_n).cross(pt_o-pt_n)>0;
					if(side1==side2&&side2==side3) {
						contains=true;
						break;
					}
				}

				//this is an ear!
				if(!contains) {
					FillTriangleDecal(
						f->localToWorld(pt_p),
						f->localToWorld(pt_c),
						f->localToWorld(pt_n),
						f->col
					);

					//remove this index and start over
					indexes.erase(curr);
					curr=indexes.begin();
				} else curr++;
			}

			//outlines
			for(int i=0; i<f->getNum(); i++) {
				const auto& a=f->pts[i];
				const auto& b=f->pts[(i+1)%f->getNum()];
				DrawLineDecal(
					f->localToWorld(a),
					f->localToWorld(b),
					olc::BLACK
				);
			}
		}

		//show slice
		if(slice.size()) {
			bool is_first=true;
			vf2d prev;
			float length=0;
			for(const auto& s:slice) {
				if(is_first) is_first=false;
				else DrawLineDecal(prev, s, olc::RED);
				prev=s;
			}
		}

		//show ultra
		{
			cmn::AABB box{{20, 20}, vf2d(screen_bounds.max.x-20, 40)};
			//draw usage rect
			{
				float t=clamp(ultra/total_ultra, 0, 1);

				olc::Pixel col;
				if(t<.33f) col=olc::RED;
				else if(t<.67f) col=olc::YELLOW;
				else col=olc::GREEN;

				int sz=t*(box.max.x-box.min.x);
				FillRectDecal(box.min, vf2d(sz, box.max.y-box.min.y), col);
			}
			//draw text
			DrawStringDecal(box.min, "ultra", olc::BLACK);
			{
				//draw outline
				vf2d tr{box.max.x, box.min.y};
				vf2d bl{box.min.x, box.max.y};
				DrawLineDecal(box.min, tr, olc::BLACK);
				DrawLineDecal(tr, box.max, olc::BLACK);
				DrawLineDecal(box.max, bl, olc::BLACK);
				DrawLineDecal(bl, box.min, olc::BLACK);
			}
		}

		//show particles
		for(const auto& p:particles) {
			switch(p->getID()) {
				case Particle::MESSAGE:
				{
					const auto& m=dynamic_cast<Message*>(p);
					vf2d offset(4*m->msg.length(), 4);
					float t=1-m->age/m->lifespan;
					olc::Pixel fill=m->col;
					fill.a=255*clamp(t, 0, 1);
					DrawRotatedStringDecal(m->pos, m->msg, m->rot, offset, fill, {2, 2});
					break;
				}
				case Particle::SPLATTER:
				{
					const auto& s=dynamic_cast<Splatter*>(p);
					float t=1-s->age/s->lifespan;
					olc::Pixel fill=s->col;
					fill.a=255*clamp(t, 0, 1);
					FillCircleDecal(s->pos, s->rad, fill);
					break;
				}
			}
		}
	}

	bool OnUserUpdate(float dt) override {
		update(dt);

		render();

		return true;
	}
};

int main() {
	FruitNinja fj;
	if(fj.Construct(600, 800, 1, 1, false, true)) fj.Start();

	return 0;
}