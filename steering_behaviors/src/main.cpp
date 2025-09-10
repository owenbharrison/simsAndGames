#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
using olc::vf2d;

#include <vector>

#include <random>

static float random01() {
	static std::mt19937 rng{std::random_device{}()};
	static std::uniform_real_distribution<float> dist(0, 1);
	return dist(rng);
}

#include "vehicle.h"

class SteeringBehaviorsUI : public olc::PixelGameEngine {
	olc::Sprite* prim_circ_spr=nullptr;
	olc::Decal* prim_circ_dec=nullptr;
	olc::Sprite* prim_rect_spr=nullptr;
	olc::Decal* prim_rect_dec=nullptr;

	static const int num_letters='~'-'!'+1;
	olc::vi2d letter_sizes[num_letters];
	std::vector<vf2d> letter_points[num_letters];

	std::vector<Vehicle> vehicles;

	olc::Sprite* gradient_spr=nullptr;

public:
	SteeringBehaviorsUI() {
		sAppName="Steering Behaviors";
	}

	bool OnUserCreate() override {
		//make some "primitives" to draw with
		prim_rect_spr=new olc::Sprite(1, 1);
		prim_rect_spr->SetPixel(0, 0, olc::WHITE);
		prim_rect_dec=new olc::Decal(prim_rect_spr);
		{
			int sz=1024;
			prim_circ_spr=new olc::Sprite(sz, sz);
			SetDrawTarget(prim_circ_spr);
			Clear(olc::BLANK);
			FillCircle(sz/2, sz/2, sz/2);
			SetDrawTarget(nullptr);
			prim_circ_dec=new olc::Decal(prim_circ_spr);
		}

		//for each letter
		for(int l='!', ix=0; l<='~'; l++, ix++) {
			//load letter sprite
			std::string filename="assets/ascii/"+std::to_string(l)+".png";
			olc::Sprite* spr=new olc::Sprite(filename);

			//store sprite size
			letter_sizes[ix]={spr->width, spr->height};

			//for each pixel
			for(int i=1; i<spr->width-1; i++) {
				for(int j=1; j<spr->height-1; j++) {
					const auto& curr=spr->GetPixel(i, j);

					//check neighboring pixels
					bool edge=false;
					const int di[4]{-1, 0, 1, 0};
					const int dj[4]{0, -1, 0, 1};
					for(int d=0; d<4; d++) {
						int ci=i+di[d], cj=j+dj[d];

						//get difference
						const auto& other=spr->GetPixel(ci, cj);
						int dr=curr.r-other.r;
						int dg=curr.g-other.g;
						int db=curr.b-other.b;
						int d_sq=dr*dr+dg*dg+db*db;

						//if different: likely an edge
						const int col_thr=200;
						if(d_sq>col_thr*col_thr) {
							edge=true;
							break;
						}
					}

					if(!edge) continue;

					//use uv coords
					vf2d ij(.5f+i, .5f+j);

					//check proximity to other points
					bool unique=true;
					for(const auto& p:letter_points[ix]) {
						const float rad_thr=9;
						if((ij-p).mag2()<rad_thr*rad_thr) {
							unique=false;
							break;
						}
					}

					//only place new point so close to any other
					if(unique) letter_points[ix].push_back(ij);
				}
			}
			
			//unload sprite
			delete spr;

			//turn into uvs
			for(auto& p:letter_points[ix]) p/=letter_sizes[ix];
		}

		//place some text in middle of screen
		{
			//fit text inside box
			const auto margin=25;
			const vf2d min(margin, margin);
			const vf2d max(ScreenWidth()-margin, ScreenHeight()-margin);

			//get "size" if height=1
			std::string str="Hello, World!\ntesting...\nA1B2C3";
			vf2d size=textSize(str, 1);

			//how many times can that fit into the box?
			float scl_x=(max.x-min.x)/size.x;
			float scl_y=(max.y-min.y)/size.y;

			//take minimum
			float scl=std::min(scl_x, scl_y);
			const vf2d ctr=(min+max)/2;
			const auto positions=textToDots(ctr-size*scl/2, str, scl);
		
			//vehicle target should be points on text contour
			olc::Sprite* rainbow_spr=new olc::Sprite("assets/rainbow.png");
			for(const auto& t:positions) {
				//random color from gradient
				float u=random01();
				vehicles.push_back(Vehicle(t, t, rainbow_spr->Sample(u, 0)));
			}
			delete rainbow_spr;
		}

		randomizeVehicles();

		return true;
	}

	bool OnUserDestroy() override {
		delete prim_rect_dec;
		delete prim_rect_spr;
		delete prim_circ_dec;
		delete prim_circ_spr;

		delete gradient_spr;

		return true;
	}

	//find bounds of string
	vf2d textSize(const std::string& str, float height) {
		//for every character in string
		vf2d offset, size;
		for(const auto& c:str) {
			//space spacing :D
			if(c==' ') offset.x+=.5f*height;
			//newline formatting
			else if(c=='\n') offset.x=0, offset.y+=height;
			else if(c>='!'&&c<='~') {
				//letter spacing...
				int ix=c-'!';

				//aspect ratio to find scaled letter
				const auto& l_sz=letter_sizes[ix];
				float width=height*l_sz.x/l_sz.y;
				size.x=std::max(size.x, offset.x+width);
				size.y=std::max(size.y, offset.y+height);

				offset.x+=width;
			}
		}
		return size;
	}
	
	std::vector<vf2d> textToDots(const vf2d& pos, const std::string& str, float height) {
		//for every character in string
		vf2d offset=pos;
		std::vector<vf2d> pts;
		for(const auto& c:str) {
			//space spacing :D
			if(c==' ') offset.x+=.5f*height;
			//newline formatting
			else if(c=='\n') offset.x=pos.x, offset.y+=height;
			else if(c>='!'&&c<='~'){
				//letter spacing...
				int ix=c-'!';

				//aspect ratio to find scaled letter
				const auto& sz=letter_sizes[ix];
				vf2d scaled(height*sz.x/sz.y, height);
				for(const auto& uv:letter_points[ix]) {
					pts.push_back(offset+scaled*uv);
				}
				offset.x+=scaled.x;
			}
		}
		return pts;
	}

	//random vehicle positions
	void randomizeVehicles() {
		for(auto& v:vehicles) {
			v.pos.x=ScreenWidth()*random01();
			v.pos.y=ScreenHeight()*random01();
		}
	}

	void DrawThickLineDecal(const vf2d& a, const vf2d& b, float w, olc::Pixel col) {
		vf2d sub=b-a;
		float len=sub.mag();
		vf2d tang=(sub/len).perp();

		float angle=std::atan2(sub.y, sub.x);
		DrawRotatedDecal(a-w*tang, prim_rect_dec, angle, {0, 0}, {len, 2*w}, col);
	}

	void FillCircleDecal(const vf2d& pos, float rad, const olc::Pixel& col) {
		vf2d offset(rad, rad);
		vf2d scale{2*rad/prim_circ_spr->width, 2*rad/prim_circ_spr->width};
		DrawDecal(pos-offset, prim_circ_dec, scale, col);
	}

	bool OnUserUpdate(float dt) override {
		const vf2d mouse_pos=GetMousePos();

		if(GetKey(olc::Key::R).bPressed) randomizeVehicles();

		//steering behaviors
		for(auto& v:vehicles) {
			//arrive at target
			v.accelerate(20*v.getArrive(v.target));

			//if mouse too close, flee
			if((mouse_pos-v.pos).mag2()<1600) {
				v.accelerate(50*v.getFlee(mouse_pos));
			}

			//kinematics
			v.update(dt);
		}

		//draw background grid
		FillRectDecal({0, 0}, GetScreenSize(), olc::BLACK);

		//show grid
		{
			const auto grid_spacing=30;

			const int num_x=1+ScreenWidth()/grid_spacing;
			const int num_y=1+ScreenHeight()/grid_spacing;

			//vertical lines
			for(int i=0; i<=num_x; i++) {
				float x=grid_spacing*i;
				vf2d top(x, 0), btm(x, ScreenHeight());
				if(i%5==0) DrawThickLineDecal(top, btm, 2, olc::VERY_DARK_GREY);
				else DrawLineDecal(top, btm, olc::VERY_DARK_GREY);
			}

			//horizontal lines
			for(int j=0; j<=num_y; j++) {
				float y=grid_spacing*j;
				vf2d lft(0, y), rgt(ScreenWidth(), y);
				if(j%5==0) DrawThickLineDecal(lft, rgt, 2, olc::VERY_DARK_GREY);
				else DrawLineDecal(lft, rgt, olc::VERY_DARK_GREY);
			}
		}

		//draw "vehicles" as circles
		//close = white, far = random
		for(const auto& v:vehicles) {
			float dist=(v.pos-v.target).mag();
			float u=1-std::exp(-dist/45);
			olc::Pixel col=olc::PixelF(
				1+u*(v.col.r/255.f-1),
				1+u*(v.col.g/255.f-1),
				1+u*(v.col.b/255.f-1)
			);
			FillCircleDecal(v.pos, 2.9f, col);
		}

		return true;
	}
};

int main() {
	SteeringBehaviorsUI sbui;
	bool vsync=true;
	if(sbui.Construct(720, 480, 1, 1, false, vsync)) sbui.Start();

	return 0;
}