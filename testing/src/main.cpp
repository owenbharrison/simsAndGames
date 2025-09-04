#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include <random>

#include "common/aabb.h"
namespace cmn {
	using AABB=AABB_generic<vf2d>;
}

#include "perlin_noise.h"

struct Bubble {
	vf2d pos, vel, acc;
	olc::Pixel col=olc::WHITE;
	bool to_collide=false;

	void accelerate(const vf2d& a) {
		acc+=a;
	}

	void update(float dt) {
		vel+=acc*dt;
		pos+=vel*dt;
		acc*=0;
	}

	void keepIn(const cmn::AABB& box) {
		if(pos.x<box.min.x) {
			pos.x=box.min.x;
			vel.x*=-1;
		}
		if(pos.y<box.min.y) {
			pos.y=box.min.y;
			vel.y*=-1;
		}
		if(pos.x>box.max.x) {
			pos.x=box.max.x;
			vel.x*=-1;
		}
		if(pos.y>box.max.y) {
			pos.y=box.max.y;
			vel.y*=-1;
		}
	}
};

constexpr float Pi=3.1415927f;

static float random01() {
	static thread_local std::mt19937 rng{std::random_device{}()};
	static thread_local std::uniform_real_distribution<float> dist(0, 1);
	return dist(rng);
}

class Example : public olc::PixelGameEngine {
	olc::Sprite* bubble_spr=nullptr;
	olc::Decal* bubble_dec=nullptr;

	int max_bubbles=0;
	float bubble_timer=0;

	const float bubble_rad=50;
	std::vector<Bubble> bubbles;

	cmn::AABB bounds;

	PerlinNoise noise_gen;

	float total_dt=0;

	vf2d gravity;

public:
	Example() {
		sAppName="bubble screensaver";
	}

public:
	bool OnUserCreate() override {
		bubble_spr=new olc::Sprite("assets/bubble.png");

		//fix bubble sprite...
		for(int i=0; i<bubble_spr->width; i++) {
			for(int j=0; j<bubble_spr->height; j++) {
				const auto& p=bubble_spr->GetPixel(i, j);

				//alpha=luminance, col=white
				int lum=(p.r+p.g+p.b)/3;
				bubble_spr->SetPixel(i, j, olc::Pixel(255, 255, 255, lum));
			}
		}

		bubble_dec=new olc::Decal(bubble_spr);

		{//estimate bubble count
			//circle packing efficiency
			float usable_area=.907f*ScreenWidth()*ScreenHeight();

			//use 50-75% of the screen
			float said_pct=.5f+.25f*random01();
			float said_area=said_pct*usable_area;

			//how many bubbles can fit?
			float bubble_area=Pi*bubble_rad*bubble_rad;
			max_bubbles=said_area/bubble_area;
		}

		bounds={vf2d(0, 0), vf2d(ScreenWidth(), ScreenHeight())};
		
		noise_gen=PerlinNoise(std::time(0));

		return true;
	}

	bool OnUserDestroy() override {
		delete bubble_dec;
		delete bubble_spr;

		return true;
	}

	bool OnUserUpdate(float dt) override {
		if(GetKey(olc::Key::ESCAPE).bHeld) return false;
		
		if(bubbles.size()<max_bubbles) {
			//spawn a new bubble every now and then
			if(bubble_timer<0) {
				bubble_timer+=.25f+.75f*random01();

				//bottom left, random vel up and right
				vf2d pos(0, ScreenHeight());
				float speed=70+130*random01();
				float angle=Pi*(1.5f+.5f*random01());
				vf2d vel(speed*std::cos(angle), speed*std::sin(angle));
				Bubble b_temp{pos, vel};
				
				//random bright color
				int r=256*random01();
				int g=256*random01();
				int b=256*random01();
				float l=std::max(r, std::max(g, b));
				b_temp.col=olc::PixelF(r/l, g/l, b/l);
				bubbles.push_back(b_temp);
			}
			bubble_timer-=dt;
		}

		//update gravity
		gravity.x=100-200*noise_gen.noise(.25f*total_dt, 0);
		gravity.y=100-200*noise_gen.noise(0, .25f*total_dt);

		//physics
		for(auto& b:bubbles) {
			b.keepIn(bounds);

			//first find collisions
			std::vector<Bubble*> collisions;
			for(auto& o:bubbles) {
				//dont check self
				if(&o==&b) continue;

				//dont check non-colliding
				if(!o.to_collide) continue;

				//broad phase detection
				vf2d sub=o.pos-b.pos;
				if(sub.mag2()<4*bubble_rad*bubble_rad) {
					collisions.push_back(&o);
				}
			}
			if(b.to_collide) {
				//solve collisions
				for(auto& c:collisions) {
					//push bubbles apart
					vf2d sub=b.pos-c->pos;
					float dist_sq=sub.mag2();
					float dist=std::sqrt(dist_sq);
					float diff=dist-2*bubble_rad;
					vf2d norm=sub/dist;
					vf2d delta=.5f*diff*norm;
					b.pos-=delta, c->pos+=delta;

					//inelastic collision
					//special case where m1=m2
					float v_rel=norm.dot(b.vel-c->vel);
					const float restitution=.5f;
					float impulse=-(1+restitution)*v_rel/2;
					vf2d vel_delta=impulse*norm;
					b.vel+=vel_delta, c->vel-=vel_delta;
				}
			}
			//start colliding if not colliding
			else if(collisions.empty()) b.to_collide=true;

			//kinematics
			b.accelerate(gravity);
			b.update(dt);
		}

		for(const auto& b:bubbles) {
			vf2d scl=2*bubble_rad/vf2d(bubble_spr->width, bubble_spr->height);
			DrawDecal(b.pos-bubble_rad, bubble_dec, scl, b.col);
		}

		total_dt+=dt;

		return true;
	}
};

int main() {
	Example demo;
	bool fullscreen=true;
	bool vsync=true;
	if(demo.Construct(1920, 1080, 1, 1, fullscreen, vsync)) demo.Start();

	return 0;
}